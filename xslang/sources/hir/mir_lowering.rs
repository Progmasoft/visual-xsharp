/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use std::collections::{HashMap, HashSet};

use super::async_check::Span;
use super::match_model::{MatchArm, MatchPattern};
use super::result_desugar::{DesugaredBlock, DesugaredExpression, DesugaredFunction, DesugaredStatement};
use super::type_check::{
  BinaryOperator, Block, Expression, Function, Literal, PrimitiveType, Statement, Type, UnaryOperator, UpdateOperator,
  UpdatePosition,
};
use crate::mir;
use crate::xlil::{Type as XlilType, TypeKind};

use binary_operations::{
  binary_float_operation, binary_i32_operation, binary_i64_operation, comparison_float_operation,
  comparison_i64_operation, comparison_str_operation, integer_operation,
};

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum DiagnosticCode
{
  UnsupportedType,
  UnsupportedExpression,
  UnknownLocal,
  InvalidIntegerLiteral,
  MissingReturn,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct Diagnostic
{
  pub code: DiagnosticCode,
  pub message: String,
  pub span: Span,
}

#[derive(Default)]
pub struct HirToMirLowerer
{
  diagnostics: Vec<Diagnostic>,
  locals: HashMap<String, mir::LocalId>,
  next_local: u32,
  current_block: mir::BlockId,
  loop_targets: Vec<(mir::BlockId, mir::BlockId)>,
  storage_locals: HashSet<mir::LocalId>,
  nominal_types: HashMap<String, crate::hir::declarations::NominalType>,
  aggregate_types: HashMap<String, XlilType>,
  aggregate_layouts: HashMap<XlilType, Vec<XlilType>>,
  tuple_types: Vec<(Type, XlilType)>,
  optional_types: Vec<(Type, XlilType)>,
  optional_layouts: Vec<(XlilType, XlilType)>,
  array_types: Vec<(Type, XlilType)>,
  array_layouts: Vec<(XlilType, XlilType, Option<u64>)>,
  nominal_locals: HashMap<String, String>,
  field_locals: HashMap<String, mir::LocalId>,
}

mod diagnostic;
mod enum_value;
#[cfg(test)]
mod float_tests;
#[cfg(test)]
mod for_tests;
#[cfg(test)]
mod integer_operator_tests;
#[cfg(test)]
mod integer_width_tests;
mod nominal;
#[cfg(test)]
mod nominal_return_tests;
#[cfg(test)]
mod operator_tests;
mod optional;
#[cfg(test)]
mod optional_member_tests;
#[cfg(test)]
mod optional_tests;
mod unary;
#[cfg(test)]
mod unary_tests;

impl HirToMirLowerer
{
  #[must_use]
  pub fn new() -> Self
  {
    Self::default()
  }

  #[must_use]
  pub fn with_nominal_types(mut self, types: &[crate::hir::declarations::NominalType]) -> Self
  {
    self.nominal_types = types.iter().map(|ty| (ty.name.clone(), ty.clone())).collect();
    self.aggregate_types =
      crate::hir::aggregate_registry::build(types).map_or_else(HashMap::new, |registry| registry.types);
    self
  }

  #[must_use]
  pub(crate) fn with_collection_types(mut self, registry: &crate::hir::collection_registry::CollectionRegistry)
                                      -> Self
  {
    self.array_types = registry.arrays
                               .iter()
                               .map(|layout| (layout.source_type.clone(), layout.value_type))
                               .collect();
    self.array_layouts = registry.arrays
                                 .iter()
                                 .map(|layout| (layout.value_type, layout.element_type, layout.length))
                                 .collect();
    self
  }

  #[must_use]
  pub(crate) fn with_aggregate_types(mut self, registry: &crate::hir::aggregate_registry::AggregateRegistry) -> Self
  {
    self.aggregate_types.clone_from(&registry.types);
    self.aggregate_layouts = registry.layouts
                                     .iter()
                                     .map(|layout| (layout.value_type, layout.fields.clone()))
                                     .collect();
    self.tuple_types.clone_from(&registry.tuples);
    self.optional_types.clone_from(&registry.optionals);
    self.optional_layouts = registry.optionals
                                    .iter()
                                    .filter_map(|(source, value_type)| {
                                      let Type::Optional { .. } = source
                                      else
                                      {
                                        return None;
                                      };
                                      let element_type = registry.layouts
                                                                 .get(value_type.registry_id as usize)?
                                                                 .fields
                                                                 .get(1)
                                                                 .copied()?;
                                      Some((*value_type, element_type))
                                    })
                                    .collect();
    self
  }

  pub fn lower_function(self, function: &Function) -> Result<mir::Function, Vec<Diagnostic>>
  {
    self.lower_function_with_parameters(function, 0)
  }

  pub fn lower_desugared_function(mut self, function: &DesugaredFunction) -> Result<mir::Function, Vec<Diagnostic>>
  {
    let mut body = Vec::with_capacity(function.body.len());
    for statement in &function.body
    {
      if let Some(statement) = self.surface_statement_from_desugared(statement)
      {
        body.push(statement);
      }
    }
    if !self.diagnostics.is_empty()
    {
      return Err(self.diagnostics);
    }
    let surface = Function { name: function.name.clone(),
                             return_type: function.return_type.clone(),
                             locals: function.locals.clone(),
                             body };
    self.lower_function(&surface)
  }

  fn lower_statement(&mut self, statement: &Statement, lowered: &mut mir::Function)
  {
    match statement
    {
      Statement::Let { local,
                       initializer, } =>
      {
        if matches!(local.ty, Type::Named(_))
        {
          self.lower_nominal_binding(local, initializer.as_ref(), lowered);
          return;
        }
        let id = self.declare_local(local.name.clone(), &local.ty, local.mutable, local.span, lowered);
        if let Some(initializer) = initializer
        {
          self.lower_assignment(id, initializer, lowered);
        }
      }
      Statement::Expr(Expression::Assign { target,
                                           value,
                                           span, }) =>
      {
        if let Some(type_name) = self.nominal_locals.get(target).cloned()
        {
          self.lower_nominal_root_assignment(target, &type_name, value, *span, lowered);
          return;
        }
        let Some(id) = self.locals.get(target).copied()
        else
        {
          self.report(DiagnosticCode::UnknownLocal,
                      format!("unknown HIR local '{target}'"),
                      *span);
          return;
        };
        self.lower_assignment(id, value, lowered);
      }
      Statement::Expr(Expression::AssignField { target,
                                                value,
                                                .. }) => self.lower_field_assignment(target, value, lowered),
      Statement::AssignIndex { target,
                               index,
                               value,
                               element_type,
                               span, } =>
      {
        self.lower_index_assignment(target, index, value, element_type, *span, lowered)
      }
      Statement::AssignTupleElement { .. } => self.lower_tuple_assignment(statement, lowered),
      Statement::Expr(expression @ Expression::Update { .. }) =>
      {
        let Some(value_type) = self.expression_value_type(expression, lowered)
        else
        {
          self.unsupported_expression(expression);
          return;
        };
        let _ = self.lower_update_expression(expression, value_type, lowered);
      }
      Statement::Expr(expression @ Expression::OptionalCoalesceAssign { .. }) =>
      {
        let Some(value_type) = self.expression_value_type(expression, lowered)
        else
        {
          self.unsupported_expression(expression);
          return;
        };
        let _ = self.lower_expression_to_local(expression, value_type, lowered);
      }
      Statement::Expr(expression @ Expression::Call { .. }) => self.lower_call_statement(expression, lowered),
      Statement::Expr(expression) => self.unsupported_expression(expression),
      Statement::Return { value,
                          span, } => self.lower_return(value.as_ref(), *span, lowered),
      Statement::If { .. } => self.lower_if_statement(statement, lowered),
      Statement::While { .. } => self.lower_while_statement(statement, lowered),
      Statement::For { .. } => self.lower_for_statement(statement, lowered),
      Statement::ForEach { .. } => self.lower_for_each_statement(statement, lowered),
      Statement::Match { .. } => self.lower_match_statement(statement, lowered),
      Statement::Break { span } => self.lower_loop_jump(false, *span, lowered),
      Statement::Continue { span } => self.lower_loop_jump(true, *span, lowered),
      Statement::Panic { span } => self.lower_panic(*span, lowered),
    }
  }

  fn lower_assignment(&mut self, target: mir::LocalId, expression: &Expression, lowered: &mut mir::Function)
  {
    let Some(value_type) = self.local_value_type(target, lowered)
    else
    {
      self.report(DiagnosticCode::UnsupportedType,
                  "assignment target has no MIR value type",
                  expression_span(expression));
      return;
    };
    let Some(value) = self.lower_expression_to_local(expression, value_type, lowered)
    else
    {
      return;
    };
    self.current_block_mut(lowered)
        .statements
        .push(mir::Statement::StoreLocal { local: target,
                                           value,
                                           span: expression_span(expression) });
  }

  fn lower_return(&mut self, value: Option<&Expression>, span: Span, lowered: &mut mir::Function)
  {
    if self.current_block_mut(lowered).terminator.is_some()
    {
      return;
    }
    let terminator = match value
    {
      None => mir::Terminator::Return(None),
      Some(expression @ Expression::If { .. }) =>
      {
        self.lower_if_return(expression, span, lowered);
        return;
      }
      Some(expression) =>
      {
        let Some(local) = self.lower_expression_to_local(expression, lowered.return_type, lowered)
        else
        {
          return;
        };
        mir::Terminator::Return(Some(local))
      }
    };
    let block = self.current_block_mut(lowered);
    block.terminator = Some(terminator);
    block.span = span;
  }

  fn lower_expression_to_local(&mut self,
                               expression: &Expression,
                               expected_type: XlilType,
                               lowered: &mut mir::Function)
                               -> Option<mir::LocalId>
  {
    match expression
    {
      Expression::Local { name,
                          span, } =>
      {
        if let Some(type_name) = self.nominal_locals.get(name).cloned() &&
           self.aggregate_types.get(&type_name).copied() == Some(expected_type)
        {
          return self.lower_nominal_place_value(name, &type_name, *span, lowered);
        }
        let Some(local) = self.locals.get(name).copied()
        else
        {
          self.report(DiagnosticCode::UnknownLocal,
                      format!("unknown HIR local '{name}'"),
                      *span);
          return None;
        };
        if self.local_value_type(local, lowered) != Some(expected_type)
        {
          self.report(DiagnosticCode::UnsupportedType,
                      "HIR local type does not match the expected MIR expression type",
                      *span);
          return None;
        }
        if self.storage_locals.contains(&local)
        {
          let result = self.declare_temp(expected_type, *span, lowered)?;
          self.current_block_mut(lowered)
              .statements
              .push(mir::Statement::LoadLocal { result,
                                                local,
                                                span: *span });
          Some(result)
        }
        else
        {
          Some(local)
        }
      }
      Expression::Field { path } => self.lower_field_load(path, expected_type, lowered),
      Expression::Member { .. } => self.lower_member_value(expression, expected_type, lowered),
      Expression::Object { nominal_type,
                           fields,
                           span, } => self.lower_object_value(nominal_type, fields, *span, lowered),
      Expression::Array { .. } => self.lower_array_expression(expression, expected_type, lowered),
      Expression::Tuple { .. } => self.lower_tuple_expression(expression, expected_type, lowered),
      Expression::TupleElement { .. } => self.lower_tuple_element(expression, expected_type, lowered),
      Expression::Set { .. } | Expression::Map { .. } =>
      {
        self.unsupported_expression(expression);
        None
      }
      Expression::Index { .. } => self.lower_index_expression(expression, expected_type, lowered),
      Expression::ArrayLength { collection,
                                span, } => self.lower_array_length(collection, expected_type, *span, lowered),
      Expression::Literal { literal,
                            span, } =>
      {
        let local = self.declare_temp(expected_type, *span, lowered)?;
        self.lower_literal_into(local, literal, *span, lowered);
        Some(local)
      }
      Expression::Binary { operator,
                           left,
                           right,
                           span, } =>
      {
        if *operator == BinaryOperator::Coalesce
        {
          return self.lower_optional_coalesce(left, right, expected_type, *span, lowered);
        }
        if matches!(operator, BinaryOperator::LogicalAnd | BinaryOperator::LogicalOr)
        {
          return self.lower_short_circuit_expression(*operator, left, right, *span, lowered);
        }
        let local = self.declare_temp(expected_type, *span, lowered)?;
        self.lower_binary_into(local, *operator, left, right, *span, lowered);
        Some(local)
      }
      Expression::Unary { operator: UnaryOperator::Positive,
                          operand,
                          .. } => self.lower_expression_to_local(operand, expected_type, lowered),
      Expression::Unary { operator,
                          operand,
                          span, } =>
      {
        let local = self.declare_temp(expected_type, *span, lowered)?;
        self.lower_unary_into(local, *operator, operand, *span, lowered);
        Some(local)
      }
      Expression::OptionalUnwrap { value,
                                   span,
                                   .. } => self.lower_optional_unwrap(value, expected_type, *span, lowered),
      Expression::OptionalCoalesceAssign { target,
                                           value,
                                           span,
                                           .. } =>
      {
        self.lower_optional_coalesce_assign(target, value, expected_type, *span, lowered)
      }
      Expression::OptionalMember { .. } => self.lower_optional_member(expression, expected_type, lowered),
      Expression::Assign { .. } | Expression::AssignField { .. } =>
      {
        self.unsupported_expression(expression);
        None
      }
      Expression::Update { .. } => self.lower_update_expression(expression, expected_type, lowered),
      Expression::ResultPropagation { .. } =>
      {
        self.unsupported_expression(expression);
        None
      }
      Expression::Call { span, .. } =>
      {
        let local = self.declare_temp(expected_type, *span, lowered)?;
        self.lower_call_into(local, expression, lowered);
        Some(local)
      }
      Expression::If { .. } => self.lower_if_expression(expression, expected_type, lowered),
      Expression::Match { .. } => self.lower_match_expression(expression, expected_type, lowered),
    }
  }

  fn lower_binary_into(&mut self,
                       target: mir::LocalId,
                       operator: BinaryOperator,
                       left: &Expression,
                       right: &Expression,
                       span: Span,
                       lowered: &mut mir::Function)
  {
    let Some(target_type) = self.local_value_type(target, lowered)
    else
    {
      self.report(DiagnosticCode::UnsupportedType,
                  "binary expression target local has no MIR value type",
                  span);
      return;
    };
    let Some(operand_type) = self.binary_operand_type(operator, target_type, left, right, lowered)
    else
    {
      self.report(DiagnosticCode::UnsupportedExpression,
                  "HIR binary expression cannot lower to MIR for these operand/result types",
                  span);
      return;
    };
    let Some(left) = self.lower_expression_to_local(left, operand_type, lowered)
    else
    {
      return;
    };
    let Some(right) = self.lower_expression_to_local(right, operand_type, lowered)
    else
    {
      return;
    };
    if self.lower_enum_comparison(target, operator, operand_type, (left, right), span, lowered)
    {
      return;
    }
    match (operator, target_type, operand_type)
    {
      (BinaryOperator::Add, XlilType::I32, XlilType::I32) => self.current_block_mut(lowered)
                                                                 .statements
                                                                 .push(mir::Statement::AddI32 { result: target,
                                                                                                left,
                                                                                                right,
                                                                                                span }),
      (BinaryOperator::Sub, XlilType::I32, XlilType::I32) => self.current_block_mut(lowered)
                                                                 .statements
                                                                 .push(mir::Statement::SubI32 { result: target,
                                                                                                left,
                                                                                                right,
                                                                                                span }),
      (BinaryOperator::Mul, XlilType::I32, XlilType::I32) => self.current_block_mut(lowered)
                                                                 .statements
                                                                 .push(mir::Statement::MulI32 { result: target,
                                                                                                left,
                                                                                                right,
                                                                                                span }),
      (operator, XlilType::I32, XlilType::I32) if binary_i32_operation(operator).is_some() =>
      {
        self.current_block_mut(lowered)
            .statements
            .push(mir::Statement::BinaryI32 { operation: binary_i32_operation(operator).expect("guarded i32 \
                                                                                                operation must \
                                                                                                exist"),
                                              result: target,
                                              left,
                                              right,
                                              span });
      }
      (BinaryOperator::Equal, XlilType::BOOL, XlilType::I32) => self.current_block_mut(lowered)
                                                                    .statements
                                                                    .push(mir::Statement::EqI32 { result: target,
                                                                                                  left,
                                                                                                  right,
                                                                                                  span }),
      (BinaryOperator::Less, XlilType::BOOL, XlilType::I32) => self.current_block_mut(lowered)
                                                                   .statements
                                                                   .push(mir::Statement::LtI32 { result: target,
                                                                                                 left,
                                                                                                 right,
                                                                                                 span }),
      (BinaryOperator::LessEqual, XlilType::BOOL, XlilType::I32) => self.current_block_mut(lowered)
                                                                        .statements
                                                                        .push(mir::Statement::LeI32 { result: target,
                                                                                                      left,
                                                                                                      right,
                                                                                                      span }),
      (BinaryOperator::Greater, XlilType::BOOL, XlilType::I32) => self.current_block_mut(lowered)
                                                                      .statements
                                                                      .push(mir::Statement::GtI32 { result: target,
                                                                                                    left,
                                                                                                    right,
                                                                                                    span }),
      (BinaryOperator::GreaterEqual, XlilType::BOOL, XlilType::I32) => self.current_block_mut(lowered)
                                                                           .statements
                                                                           .push(mir::Statement::GeI32 { result: target,
                                                                                                         left,
                                                                                                         right,
                                                                                                         span }),
      (BinaryOperator::Add, XlilType::I64, XlilType::I64) => self.current_block_mut(lowered)
                                                                 .statements
                                                                 .push(mir::Statement::AddI64 { result: target,
                                                                                                left,
                                                                                                right,
                                                                                                span }),
      (BinaryOperator::Sub, XlilType::I64, XlilType::I64) => self.current_block_mut(lowered)
                                                                 .statements
                                                                 .push(mir::Statement::SubI64 { result: target,
                                                                                                left,
                                                                                                right,
                                                                                                span }),
      (BinaryOperator::Mul, XlilType::I64, XlilType::I64) => self.current_block_mut(lowered)
                                                                 .statements
                                                                 .push(mir::Statement::MulI64 { result: target,
                                                                                                left,
                                                                                                right,
                                                                                                span }),
      (operator, XlilType::I64, XlilType::I64) if binary_i64_operation(operator).is_some() =>
      {
        self.current_block_mut(lowered)
            .statements
            .push(mir::Statement::BinaryI64 { operation: binary_i64_operation(operator).expect("guarded i64 \
                                                                                                operation must \
                                                                                                exist"),
                                              result: target,
                                              left,
                                              right,
                                              span });
      }
      (BinaryOperator::Equal, XlilType::BOOL, XlilType::I64) => self.current_block_mut(lowered)
                                                                    .statements
                                                                    .push(mir::Statement::EqI64 { result: target,
                                                                                                  left,
                                                                                                  right,
                                                                                                  span }),
      (operator, XlilType::BOOL, XlilType::I64) if comparison_i64_operation(operator).is_some() =>
      {
        self.current_block_mut(lowered)
            .statements
            .push(mir::Statement::CompareI64 { operation: comparison_i64_operation(operator).expect("guarded i64 \
                                                                                                     comparison \
                                                                                                     must exist"),
                                               result: target,
                                               left,
                                               right,
                                               span });
      }
      (operator, value_type, operand_type)
        if value_type == operand_type && value_type.is_integer() && integer_operation(operator).is_some() =>
      {
        self.current_block_mut(lowered)
            .statements
            .push(mir::Statement::BinaryInteger { operation: integer_operation(operator).expect("guarded integer \
                                                                                                 operation must \
                                                                                                 exist"),
                                                  value_type,
                                                  result: target,
                                                  left,
                                                  right,
                                                  span });
      }
      (operator, XlilType::BOOL, operand_type)
        if operand_type.is_integer() &&
           integer_operation(operator).is_some_and(|operation| operation.is_comparison()) =>
      {
        self.current_block_mut(lowered)
            .statements
            .push(mir::Statement::BinaryInteger { operation: integer_operation(operator).expect("guarded integer \
                                                                                                 comparison must \
                                                                                                 exist"),
                                                  value_type: operand_type,
                                                  result: target,
                                                  left,
                                                  right,
                                                  span });
      }
      (operator, value_type, operand_type)
        if value_type == operand_type &&
           matches!(value_type, XlilType::F32 | XlilType::F64) &&
           binary_float_operation(operator).is_some() =>
      {
        self.current_block_mut(lowered)
            .statements
            .push(mir::Statement::BinaryFloat { operation:
                                                  binary_float_operation(operator).expect("guarded float operation \
                                                                                           must exist"),
                                                value_type,
                                                result: target,
                                                left,
                                                right,
                                                span });
      }
      (operator, XlilType::BOOL, operand_type)
        if matches!(operand_type, XlilType::F32 | XlilType::F64) && comparison_float_operation(operator).is_some() =>
      {
        self.current_block_mut(lowered)
            .statements
            .push(mir::Statement::CompareFloat { operation:
                                                   comparison_float_operation(operator).expect("guarded float \
                                                                                                comparison must exist"),
                                                 value_type: operand_type,
                                                 result: target,
                                                 left,
                                                 right,
                                                 span });
      }
      (operator, XlilType::BOOL, XlilType::STR) if comparison_str_operation(operator).is_some() =>
      {
        self.current_block_mut(lowered)
            .statements
            .push(mir::Statement::CompareStr { operation: comparison_str_operation(operator).expect("guarded Str \
                                                                                                     comparison"),
                                               result: target,
                                               left,
                                               right,
                                               span });
      }
      _ => self.report(DiagnosticCode::UnsupportedExpression,
                       "HIR binary expression has no MIR instruction for this type combination",
                       span),
    }
  }

  fn local_value_type(&self, local: mir::LocalId, lowered: &mir::Function) -> Option<XlilType>
  {
    lowered.parameters
           .iter()
           .find(|candidate| candidate.local == local)
           .map(|candidate| candidate.value_type)
           .or_else(|| {
             lowered.locals
                    .iter()
                    .find(|candidate| candidate.id == local)
                    .and_then(|candidate| candidate.value_type)
           })
  }

  fn unsupported_expression(&mut self, expression: &Expression)
  {
    self.report(DiagnosticCode::UnsupportedExpression,
                "HIR expression cannot lower to MIR yet",
                expression_span(expression));
  }

  fn current_block_mut<'a>(&self, lowered: &'a mut mir::Function) -> &'a mut mir::BasicBlock
  {
    lowered.blocks
           .iter_mut()
           .find(|block| block.id == self.current_block)
           .expect("HIR lowering current MIR block must exist")
  }

  fn append_block(&mut self, span: Span, lowered: &mut mir::Function) -> mir::BlockId
  {
    let id = mir::BlockId(u32::try_from(lowered.blocks.len()).expect("MIR block count must fit u32"));
    lowered.blocks.push(mir::BasicBlock { id,
                                          statements: Vec::new(),
                                          terminator: None,
                                          span });
    id
  }

  fn switch_to(&mut self, block: mir::BlockId)
  {
    self.current_block = block;
  }

  fn set_terminator(&mut self, terminator: mir::Terminator, span: Span, lowered: &mut mir::Function)
  {
    let block = self.current_block_mut(lowered);
    if block.terminator.is_none()
    {
      block.terminator = Some(terminator);
      block.span = span;
    }
  }
}

#[must_use]
pub const fn primitive_to_xlil(primitive: PrimitiveType) -> Option<XlilType>
{
  let kind = match primitive
  {
    PrimitiveType::Bool => TypeKind::Bool,
    PrimitiveType::Byte => TypeKind::U8,
    PrimitiveType::SByte => TypeKind::I8,
    PrimitiveType::UShort => TypeKind::U16,
    PrimitiveType::Char => TypeKind::U32,
    PrimitiveType::Short => TypeKind::I16,
    PrimitiveType::Long => TypeKind::I32,
    PrimitiveType::Int => TypeKind::I64,
    PrimitiveType::Integer => TypeKind::I128,
    PrimitiveType::ULong => TypeKind::U32,
    PrimitiveType::UInt => TypeKind::U64,
    PrimitiveType::UInteger => TypeKind::U128,
    PrimitiveType::SFloat => TypeKind::F16,
    PrimitiveType::LFloat => TypeKind::F32,
    PrimitiveType::Float => TypeKind::F64,
    PrimitiveType::Double => TypeKind::F128,
    PrimitiveType::Str => TypeKind::Str,
    PrimitiveType::String => TypeKind::String,
  };
  Some(XlilType { kind,
                  registry_id: 0 })
}

const fn expression_span(expression: &Expression) -> Span
{
  match expression
  {
    Expression::Literal { span, .. } |
    Expression::Local { span, .. } |
    Expression::Object { span, .. } |
    Expression::Array { span, .. } |
    Expression::Set { span, .. } |
    Expression::Map { span, .. } |
    Expression::Tuple { span, .. } |
    Expression::TupleElement { span, .. } |
    Expression::Index { span, .. } |
    Expression::ArrayLength { span, .. } |
    Expression::Assign { span, .. } |
    Expression::AssignField { span, .. } |
    Expression::Update { span, .. } |
    Expression::Binary { span, .. } |
    Expression::Unary { span, .. } |
    Expression::OptionalUnwrap { span, .. } |
    Expression::OptionalCoalesceAssign { span, .. } |
    Expression::OptionalMember { span, .. } |
    Expression::ResultPropagation { span, .. } => *span,
    Expression::Field { path } => path.span,
    Expression::Member { span, .. } => *span,
    Expression::Call { span, .. } | Expression::If { span, .. } | Expression::Match { span, .. } => *span,
  }
}
mod binary_operations;
mod binary_types;
mod call_lowering;
mod collection;
#[cfg(test)]
mod collection_tests;
mod control_flow;
#[cfg(test)]
mod control_flow_tests;
#[cfg(test)]
mod desugar_tests;
mod desugared;
mod function_lowering;
mod integer_literal;
#[cfg(test)]
mod match_tests;
mod short_circuit;
#[cfg(test)]
mod short_circuit_tests;
mod tuple;
#[cfg(test)]
mod tuple_tests;
mod update;
mod value_lowering;
#[cfg(test)]
mod value_tests;

#[cfg(test)]
mod tests;
