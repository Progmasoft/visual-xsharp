/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use std::collections::HashSet;

use crate::hir::async_check::Span;

use super::{BasicBlock, BlockId, Function, LocalId, Statement, Terminator};

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum DiagnosticCode
{
  EmptyParameterName,
  DuplicateParameter,
  DuplicateLocal,
  DuplicateBlock,
  MissingTerminator,
  UnknownLocal,
  UnknownBlock,
  MissingLocalType,
  LocalTypeMismatch,
  ReturnTypeMismatch,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct Diagnostic
{
  pub code: DiagnosticCode,
  pub message: String,
  pub span: Span,
}

#[must_use]
pub fn verify_function(function: &Function) -> Vec<Diagnostic>
{
  let mut verifier = Verifier::new(function);
  verifier.verify();
  verifier.diagnostics
}

struct Verifier<'a>
{
  function: &'a Function,
  parameters: HashSet<String>,
  locals: HashSet<LocalId>,
  blocks: HashSet<BlockId>,
  diagnostics: Vec<Diagnostic>,
}

impl<'a> Verifier<'a>
{
  fn new(function: &'a Function) -> Self
  {
    Self { function,
           parameters: HashSet::new(),
           locals: HashSet::new(),
           blocks: HashSet::new(),
           diagnostics: Vec::new() }
  }

  fn verify(&mut self)
  {
    self.index_parameters();
    self.index_locals();
    self.index_blocks();
    for block in &self.function.blocks
    {
      self.verify_block(block);
    }
  }

  fn index_parameters(&mut self)
  {
    for parameter in &self.function.parameters
    {
      if parameter.name.is_empty()
      {
        self.report(DiagnosticCode::EmptyParameterName,
                    "MIR parameter name must not be empty".to_string(),
                    parameter.span);
      }
      if !self.parameters.insert(parameter.name.clone())
      {
        self.report(DiagnosticCode::DuplicateParameter,
                    format!("parameter '{}' is declared more than once", parameter.name),
                    parameter.span);
      }
      if !self.locals.insert(parameter.local)
      {
        self.report(DiagnosticCode::DuplicateLocal,
                    format!("local id {} is declared more than once", parameter.local.0),
                    parameter.span);
      }
    }
  }

  fn index_locals(&mut self)
  {
    for local in &self.function.locals
    {
      if !self.locals.insert(local.id)
      {
        self.report(DiagnosticCode::DuplicateLocal,
                    format!("local id {} is declared more than once", local.id.0),
                    local.span);
      }
    }
  }

  fn index_blocks(&mut self)
  {
    for block in &self.function.blocks
    {
      if !self.blocks.insert(block.id)
      {
        self.report(DiagnosticCode::DuplicateBlock,
                    format!("block id {} is declared more than once", block.id.0),
                    block.span);
      }
    }
  }

  fn verify_block(&mut self, block: &BasicBlock)
  {
    for statement in &block.statements
    {
      self.verify_statement(statement);
    }
    match &block.terminator
    {
      Some(terminator) => self.verify_terminator(terminator, block.span),
      None => self.report(DiagnosticCode::MissingTerminator,
                          "MIR block is missing a terminator".to_string(),
                          block.span),
    }
  }

  fn verify_statement(&mut self, statement: &Statement)
  {
    match *statement
    {
      Statement::Use { local,
                       span, } |
      Statement::Move { local,
                        span, } |
      Statement::BorrowShared { local,
                                span, } |
      Statement::BorrowMutable { local,
                                 span, } |
      Statement::EndBorrow { local,
                             span, } |
      Statement::Drop { local,
                        span, } => self.verify_local(local, span),
      Statement::ConstI64 { local,
                            span,
                            .. } => self.verify_const_i64(local, span),
      Statement::ConstI32 { local,
                            span,
                            .. } => self.verify_const_i32(local, span),
      Statement::ConstU16 { local,
                            span,
                            .. } => self.verify_typed_const(local, crate::xlil::Type::U16, "const.u16", span),
      Statement::ConstInteger { local,
                                value,
                                span, } =>
      {
        self.verify_typed_const(local, value.value_type(), &format!("const.{}", value.text_name()), span)
      }
      Statement::ConstF32 { local,
                            span,
                            .. } => self.verify_typed_const(local, crate::xlil::Type::F32, "const.f32", span),
      Statement::ConstF64 { local,
                            span,
                            .. } => self.verify_typed_const(local, crate::xlil::Type::F64, "const.f64", span),
      Statement::ConstStr { local,
                            span,
                            .. } => self.verify_typed_const(local, crate::xlil::Type::STR, "const.str", span),
      Statement::ConstBool { local,
                             span,
                             .. } => self.verify_const_bool(local, span),
      Statement::BinaryInteger { operation,
                                 value_type,
                                 result,
                                 left,
                                 right,
                                 span, } =>
      {
        self.verify_integer_binary(operation, value_type, result, left, right, span)
      }
      Statement::BinaryFloat { operation,
                               value_type,
                               result,
                               left,
                               right,
                               span, } =>
      {
        self.verify_float_binary(result,
                                 left,
                                 right,
                                 value_type,
                                 &format!("{}.{}", operation.text_stem(), crate::xlil::type_name(value_type)),
                                 span);
      }
      Statement::CompareFloat { operation,
                                value_type,
                                result,
                                left,
                                right,
                                span, } =>
      {
        let instruction = format!("{}.{}", operation.text_stem(), crate::xlil::type_name(value_type));
        self.verify_bool_local(result, &format!("{instruction} result local"), span);
        self.verify_float_local(left, value_type, &format!("{instruction} left operand"), span);
        self.verify_float_local(right, value_type, &format!("{instruction} right operand"), span);
      }
      Statement::CompareStr { operation,
                              result,
                              left,
                              right,
                              span, } =>
      {
        let instruction = format!("{}.str", operation.text_stem());
        self.verify_bool_local(result, &format!("{instruction} result local"), span);
        self.verify_exact_local(left,
                                crate::xlil::Type::STR,
                                &format!("{instruction} left operand"),
                                span);
        self.verify_exact_local(right,
                                crate::xlil::Type::STR,
                                &format!("{instruction} right operand"),
                                span);
      }
      Statement::StoreLocal { local,
                              value,
                              span, } => self.verify_local_storage(local, value, "store.local", span),
      Statement::LoadLocal { result,
                             local,
                             span, } => self.verify_local_storage(local, result, "load.local", span),
      Statement::Aggregate { result,
                             value_type,
                             ref fields,
                             ref field_types,
                             span, } => self.verify_aggregate(result, value_type, fields, field_types, span),
      Statement::Extract { result,
                           aggregate,
                           field_type,
                           span,
                           .. } => self.verify_extract(result, aggregate, field_type, span),
      Statement::ArrayGet { result,
                            array,
                            index,
                            array_type,
                            element_type,
                            span, } =>
      {
        self.verify_array_access(result, array, index, (array_type, element_type), None, span)
      }
      Statement::ArraySet { result,
                            array,
                            index,
                            value,
                            array_type,
                            element_type,
                            span, } =>
      {
        self.verify_array_access(result, array, index, (array_type, element_type), Some(value), span)
      }
      Statement::ArrayLength { result,
                               array,
                               array_type,
                               span, } =>
      {
        self.verify_exact_local(result, crate::xlil::Type::I64, "array.length result", span);
        self.verify_exact_local(array, array_type, "array.length source", span);
        if array_type.kind != crate::xlil::TypeKind::Array
        {
          self.report(DiagnosticCode::LocalTypeMismatch,
                      "array.length source type must reference an array registry".to_string(),
                      span);
        }
      }
      Statement::AddI64 { result,
                          left,
                          right,
                          span, } => self.verify_i64_binary(result, left, right, "add.i64", span),
      Statement::SubI64 { result,
                          left,
                          right,
                          span, } => self.verify_i64_binary(result, left, right, "sub.i64", span),
      Statement::MulI64 { result,
                          left,
                          right,
                          span, } => self.verify_i64_binary(result, left, right, "mul.i64", span),
      Statement::EqI64 { result,
                         left,
                         right,
                         span, } => self.verify_eq_i64(result, left, right, span),
      Statement::BinaryI64 { operation,
                             result,
                             left,
                             right,
                             span, } => self.verify_i64_binary(result, left, right, operation.text_name(), span),
      Statement::CompareI64 { operation,
                              result,
                              left,
                              right,
                              span, } => self.verify_i64_comparison(result, left, right, operation.text_name(), span),
      Statement::AddI32 { result,
                          left,
                          right,
                          span, } => self.verify_i32_binary(result, left, right, "add.i32", span),
      Statement::SubI32 { result,
                          left,
                          right,
                          span, } => self.verify_i32_binary(result, left, right, "sub.i32", span),
      Statement::MulI32 { result,
                          left,
                          right,
                          span, } => self.verify_i32_binary(result, left, right, "mul.i32", span),
      Statement::BinaryI32 { operation,
                             result,
                             left,
                             right,
                             span, } => self.verify_i32_binary(result, left, right, operation.text_name(), span),
      Statement::EqI32 { result,
                         left,
                         right,
                         span, } => self.verify_i32_compare(result, left, right, "eq.i32", span),
      Statement::LtI32 { result,
                         left,
                         right,
                         span, } => self.verify_i32_compare(result, left, right, "lt.i32", span),
      Statement::LeI32 { result,
                         left,
                         right,
                         span, } => self.verify_i32_compare(result, left, right, "le.i32", span),
      Statement::GtI32 { result,
                         left,
                         right,
                         span, } => self.verify_i32_compare(result, left, right, "gt.i32", span),
      Statement::GeI32 { result,
                         left,
                         right,
                         span, } => self.verify_i32_compare(result, left, right, "ge.i32", span),
      Statement::NotBool { result,
                           operand,
                           span, } =>
      {
        self.verify_bool_local(result, "not.bool result local", span);
        self.verify_bool_local(operand, "not.bool operand local", span);
      }
      Statement::Call { result,
                        ref arguments,
                        return_type,
                        span,
                        .. } => self.verify_call(result, arguments, return_type, span),
    }
  }

  fn verify_aggregate(&mut self,
                      result: LocalId,
                      value_type: crate::xlil::Type,
                      fields: &[LocalId],
                      field_types: &[crate::xlil::Type],
                      span: Span)
  {
    if !matches!(value_type.kind,
                 crate::xlil::TypeKind::Aggregate | crate::xlil::TypeKind::Array)
    {
      self.report(DiagnosticCode::LocalTypeMismatch,
                  "aggregate statement requires a composite result type".to_string(),
                  span);
    }
    self.verify_exact_local(result, value_type, "aggregate result local", span);
    if fields.len() != field_types.len()
    {
      self.report(DiagnosticCode::LocalTypeMismatch,
                  "aggregate field values and field types have different lengths".to_string(),
                  span);
      return;
    }
    for (index, (field, field_type)) in fields.iter().zip(field_types).enumerate()
    {
      self.verify_exact_local(*field, *field_type, &format!("aggregate field {index}"), span);
    }
    if value_type.kind == crate::xlil::TypeKind::Array &&
       field_types.first()
                  .is_some_and(|first| field_types.iter().any(|field| field != first))
    {
      self.report(DiagnosticCode::LocalTypeMismatch,
                  "array aggregate elements must have one homogeneous type".to_string(),
                  span);
    }
  }

  fn verify_extract(&mut self, result: LocalId, aggregate: LocalId, field_type: crate::xlil::Type, span: Span)
  {
    self.verify_local(aggregate, span);
    if let Some(value_type) = self.local_type(aggregate) &&
       !matches!(value_type.kind,
                 crate::xlil::TypeKind::Aggregate | crate::xlil::TypeKind::Array)
    {
      self.report(DiagnosticCode::LocalTypeMismatch,
                  "extract source local must have a composite type".to_string(),
                  span);
    }
    self.verify_exact_local(result, field_type, "extract result local", span);
  }

  fn verify_array_access(&mut self,
                         result: LocalId,
                         array: LocalId,
                         index: LocalId,
                         types: (crate::xlil::Type, crate::xlil::Type),
                         value: Option<LocalId>,
                         span: Span)
  {
    let (array_type, element_type) = types;
    if array_type.kind != crate::xlil::TypeKind::Array
    {
      self.report(DiagnosticCode::LocalTypeMismatch,
                  "array access requires an array registry type".to_string(),
                  span);
    }
    self.verify_exact_local(array, array_type, "array access source", span);
    self.verify_exact_local(index, crate::xlil::Type::I64, "array access index", span);
    if let Some(value) = value
    {
      self.verify_exact_local(value, element_type, "array.set value", span);
      self.verify_exact_local(result, array_type, "array.set result", span);
    }
    else
    {
      self.verify_exact_local(result, element_type, "array.get result", span);
    }
  }

  fn verify_terminator(&mut self, terminator: &Terminator, span: Span)
  {
    match *terminator
    {
      Terminator::Return(Some(local)) => self.verify_return_value(local, span),
      Terminator::Goto(block) => self.verify_block_target(block, span),
      Terminator::BranchIf { condition,
                             then_block,
                             else_block, } =>
      {
        self.verify_bool_local(condition, "branch_if condition local", span);
        self.verify_block_target(then_block, span);
        self.verify_block_target(else_block, span);
      }
      Terminator::Return(None) =>
      {
        if self.function.return_type != crate::xlil::Type::VOID
        {
          self.report(DiagnosticCode::ReturnTypeMismatch,
                      "non-void MIR function must return a typed local".to_string(),
                      span);
        }
      }
      Terminator::Panic =>
      {}
      Terminator::Unreachable =>
      {}
    }
  }

  fn verify_local(&mut self, local: LocalId, span: Span)
  {
    if !self.locals.contains(&local)
    {
      self.report(DiagnosticCode::UnknownLocal,
                  format!("local id {} is not declared in this function", local.0),
                  span);
    }
  }

  fn verify_block_target(&mut self, block: BlockId, span: Span)
  {
    if !self.blocks.contains(&block)
    {
      self.report(DiagnosticCode::UnknownBlock,
                  format!("block id {} is not declared in this function", block.0),
                  span);
    }
  }

  fn verify_local_storage(&mut self, local: LocalId, value: LocalId, instruction: &str, span: Span)
  {
    self.verify_local(local, span);
    self.verify_local(value, span);
    let local_type = self.local_type(local);
    let value_type = self.local_type(value);
    match (local_type, value_type)
    {
      (Some(local_type), Some(value_type)) if local_type == value_type =>
      {}
      (Some(_), Some(_)) => self.report(DiagnosticCode::LocalTypeMismatch,
                                        format!("{instruction} local and value types must match"),
                                        span),
      _ => self.report(DiagnosticCode::MissingLocalType,
                       format!("{instruction} requires typed locals"),
                       span),
    }
  }

  fn local_type(&self, id: LocalId) -> Option<crate::xlil::Type>
  {
    self.function
        .parameters
        .iter()
        .find(|parameter| parameter.local == id)
        .map(|parameter| parameter.value_type)
        .or_else(|| {
          self.function
              .locals
              .iter()
              .find(|local| local.id == id)
              .and_then(|local| local.value_type)
        })
  }

  fn verify_const_i64(&mut self, local: LocalId, span: Span)
  {
    self.verify_local(local, span);
    let Some(local) = self.function.locals.iter().find(|candidate| candidate.id == local)
    else
    {
      return;
    };
    match local.value_type
    {
      Some(crate::xlil::Type::I64) =>
      {}
      Some(_) => self.report(DiagnosticCode::LocalTypeMismatch,
                             "const.i64 target local must have XLIL i64 type".to_string(),
                             span),
      None => self.report(DiagnosticCode::MissingLocalType,
                          "const.i64 target local has no XLIL value type".to_string(),
                          span),
    }
  }

  fn verify_const_bool(&mut self, local: LocalId, span: Span)
  {
    self.verify_local(local, span);
    let Some(local) = self.function.locals.iter().find(|candidate| candidate.id == local)
    else
    {
      return;
    };
    match local.value_type
    {
      Some(crate::xlil::Type::BOOL) =>
      {}
      Some(_) => self.report(DiagnosticCode::LocalTypeMismatch,
                             "const.bool target local must have XLIL bool type".to_string(),
                             span),
      None => self.report(DiagnosticCode::MissingLocalType,
                          "const.bool target local has no XLIL value type".to_string(),
                          span),
    }
  }

  fn verify_const_i32(&mut self, local: LocalId, span: Span)
  {
    self.verify_local(local, span);
    let Some(local) = self.function.locals.iter().find(|candidate| candidate.id == local)
    else
    {
      return;
    };
    match local.value_type
    {
      Some(crate::xlil::Type::I32) =>
      {}
      Some(_) => self.report(DiagnosticCode::LocalTypeMismatch,
                             "const.i32 target local must have XLIL i32 type".to_string(),
                             span),
      None => self.report(DiagnosticCode::MissingLocalType,
                          "const.i32 target local has no XLIL value type".to_string(),
                          span),
    }
  }

  fn verify_typed_const(&mut self, local: LocalId, expected: crate::xlil::Type, instruction: &str, span: Span)
  {
    self.verify_local(local, span);
    let Some(local) = self.function.locals.iter().find(|candidate| candidate.id == local)
    else
    {
      return;
    };
    match local.value_type
    {
      Some(actual) if actual == expected =>
      {}
      Some(_) => self.report(DiagnosticCode::LocalTypeMismatch,
                             format!("{instruction} target local has the wrong XLIL type"),
                             span),
      None => self.report(DiagnosticCode::MissingLocalType,
                          format!("{instruction} target local has no XLIL value type"),
                          span),
    }
  }

  fn verify_exact_local(&mut self, local: LocalId, expected: crate::xlil::Type, label: &str, span: Span)
  {
    self.verify_local(local, span);
    match self.local_type(local)
    {
      Some(actual) if actual == expected =>
      {}
      Some(_) => self.report(DiagnosticCode::LocalTypeMismatch,
                             format!("{label} has the wrong XLIL type"),
                             span),
      None => self.report(DiagnosticCode::MissingLocalType,
                          format!("{label} has no XLIL value type"),
                          span),
    }
  }

  fn verify_i64_binary(&mut self, result: LocalId, left: LocalId, right: LocalId, instruction: &str, span: Span)
  {
    self.verify_i64_local(result, &format!("{instruction} result local"), span);
    self.verify_i64_local(left, &format!("{instruction} left operand"), span);
    self.verify_i64_local(right, &format!("{instruction} right operand"), span);
  }

  fn verify_integer_binary(&mut self,
                           operation: crate::xlil::IntegerBinaryOperation,
                           value_type: crate::xlil::Type,
                           result: LocalId,
                           left: LocalId,
                           right: LocalId,
                           span: Span)
  {
    let name = format!("{}.{}", operation.text_stem(), crate::xlil::type_name(value_type));
    if !value_type.is_integer()
    {
      self.report(DiagnosticCode::LocalTypeMismatch,
                  format!("{name} declares a non-integer operation type"),
                  span);
      return;
    }
    let expected_result = if operation.is_comparison()
    {
      crate::xlil::Type::BOOL
    }
    else
    {
      value_type
    };
    for (local, expected, role) in [(result, expected_result, "result"),
                                    (left, value_type, "left operand"),
                                    (right, value_type, "right operand")]
    {
      self.verify_local(local, span);
      match self.local_type(local)
      {
        Some(actual) if actual == expected =>
        {}
        Some(_) => self.report(DiagnosticCode::LocalTypeMismatch,
                               format!("{name} {role} has the wrong XLIL type"),
                               span),
        None => self.report(DiagnosticCode::MissingLocalType,
                            format!("{name} {role} has no XLIL value type"),
                            span),
      }
    }
  }

  fn verify_float_binary(&mut self,
                         result: LocalId,
                         left: LocalId,
                         right: LocalId,
                         value_type: crate::xlil::Type,
                         instruction: &str,
                         span: Span)
  {
    self.verify_float_local(result, value_type, &format!("{instruction} result local"), span);
    self.verify_float_local(left, value_type, &format!("{instruction} left operand"), span);
    self.verify_float_local(right, value_type, &format!("{instruction} right operand"), span);
  }

  fn verify_float_local(&mut self, local: LocalId, expected: crate::xlil::Type, label: &str, span: Span)
  {
    self.verify_local(local, span);
    if !matches!(expected, crate::xlil::Type::F32 | crate::xlil::Type::F64)
    {
      self.report(DiagnosticCode::LocalTypeMismatch,
                  format!("{label} declares a non-floating operation type"),
                  span);
      return;
    }
    let Some(local) = self.function.locals.iter().find(|candidate| candidate.id == local)
    else
    {
      return;
    };
    match local.value_type
    {
      Some(actual) if actual == expected =>
      {}
      Some(_) => self.report(DiagnosticCode::LocalTypeMismatch,
                             format!("{label} has the wrong XLIL floating type"),
                             span),
      None => self.report(DiagnosticCode::MissingLocalType,
                          format!("{label} has no XLIL value type"),
                          span),
    }
  }

  fn verify_eq_i64(&mut self, result: LocalId, left: LocalId, right: LocalId, span: Span)
  {
    self.verify_bool_local(result, "eq.i64 result local", span);
    self.verify_i64_local(left, "eq.i64 left operand", span);
    self.verify_i64_local(right, "eq.i64 right operand", span);
  }

  fn verify_i64_comparison(&mut self, result: LocalId, left: LocalId, right: LocalId, instruction: &str, span: Span)
  {
    self.verify_bool_local(result, &format!("{instruction} result local"), span);
    self.verify_i64_local(left, &format!("{instruction} left operand"), span);
    self.verify_i64_local(right, &format!("{instruction} right operand"), span);
  }

  fn verify_i32_binary(&mut self, result: LocalId, left: LocalId, right: LocalId, instruction: &str, span: Span)
  {
    self.verify_i32_local(result, &format!("{instruction} result local"), span);
    self.verify_i32_local(left, &format!("{instruction} left operand"), span);
    self.verify_i32_local(right, &format!("{instruction} right operand"), span);
  }

  fn verify_i32_compare(&mut self, result: LocalId, left: LocalId, right: LocalId, instruction: &str, span: Span)
  {
    self.verify_bool_local(result, &format!("{instruction} result local"), span);
    self.verify_i32_local(left, &format!("{instruction} left operand"), span);
    self.verify_i32_local(right, &format!("{instruction} right operand"), span);
  }

  fn verify_bool_local(&mut self, local: LocalId, label: &str, span: Span)
  {
    self.verify_local(local, span);
    let Some(local) = self.function.locals.iter().find(|candidate| candidate.id == local)
    else
    {
      return;
    };
    match local.value_type
    {
      Some(crate::xlil::Type::BOOL) =>
      {}
      Some(_) => self.report(DiagnosticCode::LocalTypeMismatch,
                             format!("{label} must have XLIL bool type"),
                             span),
      None => self.report(DiagnosticCode::MissingLocalType,
                          format!("{label} has no XLIL value type"),
                          span),
    }
  }

  fn verify_i64_local(&mut self, local: LocalId, label: &str, span: Span)
  {
    self.verify_local(local, span);
    let Some(local) = self.function.locals.iter().find(|candidate| candidate.id == local)
    else
    {
      return;
    };
    match local.value_type
    {
      Some(crate::xlil::Type::I64) =>
      {}
      Some(_) => self.report(DiagnosticCode::LocalTypeMismatch,
                             format!("{label} must have XLIL i64 type"),
                             span),
      None => self.report(DiagnosticCode::MissingLocalType,
                          format!("{label} has no XLIL value type"),
                          span),
    }
  }

  fn verify_i32_local(&mut self, local: LocalId, label: &str, span: Span)
  {
    self.verify_local(local, span);
    let Some(local) = self.function.locals.iter().find(|candidate| candidate.id == local)
    else
    {
      return;
    };
    match local.value_type
    {
      Some(crate::xlil::Type::I32) =>
      {}
      Some(_) => self.report(DiagnosticCode::LocalTypeMismatch,
                             format!("{label} must have XLIL i32 type"),
                             span),
      None => self.report(DiagnosticCode::MissingLocalType,
                          format!("{label} has no XLIL value type"),
                          span),
    }
  }

  fn verify_return_value(&mut self, local: LocalId, span: Span)
  {
    self.verify_local(local, span);
    let Some(local) = self.function.locals.iter().find(|candidate| candidate.id == local)
    else
    {
      return;
    };
    match local.value_type
    {
      Some(value_type) if value_type == self.function.return_type =>
      {}
      Some(_) => self.report(DiagnosticCode::ReturnTypeMismatch,
                             "MIR return local type does not match function return type".to_string(),
                             span),
      None => self.report(DiagnosticCode::MissingLocalType,
                          "MIR return local has no XLIL value type".to_string(),
                          span),
    }
  }

  fn verify_call(&mut self, result: Option<LocalId>, arguments: &[LocalId], return_type: crate::xlil::Type, span: Span)
  {
    for argument in arguments
    {
      self.verify_local(*argument, span);
    }
    match (result, return_type)
    {
      (Some(result), crate::xlil::Type::VOID) =>
      {
        self.verify_local(result, span);
        self.report(DiagnosticCode::ReturnTypeMismatch,
                    "void MIR call cannot write a result local".to_string(),
                    span);
      }
      (Some(result), return_type) =>
      {
        self.verify_local(result, span);
        let Some(local) = self.function.locals.iter().find(|candidate| candidate.id == result)
        else
        {
          return;
        };
        match local.value_type
        {
          Some(value_type) if value_type == return_type =>
          {}
          Some(_) => self.report(DiagnosticCode::LocalTypeMismatch,
                                 "MIR call result local type must match call return type".to_string(),
                                 span),
          None => self.report(DiagnosticCode::MissingLocalType,
                              "MIR call result local has no XLIL value type".to_string(),
                              span),
        }
      }
      (None, return_type) if return_type != crate::xlil::Type::VOID =>
      {
        self.report(DiagnosticCode::ReturnTypeMismatch,
                    "non-void MIR call must write a result local".to_string(),
                    span);
      }
      (None, _) =>
      {}
    }
  }

  fn report(&mut self, code: DiagnosticCode, message: String, span: Span)
  {
    self.diagnostics.push(Diagnostic { code,
                                       message,
                                       span });
  }
}

#[cfg(test)]
#[path = "verify/tests.rs"]
mod tests;
