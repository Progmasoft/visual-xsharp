/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use std::collections::{HashMap, HashSet};

use super::async_check::Span;
use super::match_model::{MatchArm, MatchPattern};
use super::result_desugar::{DesugaredBlock, DesugaredExpression, DesugaredFunction, DesugaredStatement};
use super::type_check::{
    BinaryOperator, Block, Expression, Function, Literal, PrimitiveType, Statement, Type, UnaryOperator,
    UpdateOperator, UpdatePosition,
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
    enum_data_layouts: Vec<crate::hir::aggregate_registry::EnumDataLayout>,
    tuple_types: Vec<(Type, XlilType)>,
    optional_types: Vec<(Type, XlilType)>,
    optional_layouts: Vec<(XlilType, XlilType)>,
    result_types: Vec<(Type, XlilType)>,
    result_layouts: Vec<(XlilType, XlilType, XlilType)>,
    function_return_type: Option<Type>,
    array_types: Vec<(Type, XlilType)>,
    array_layouts: Vec<(XlilType, XlilType, Option<u64>)>,
    nominal_locals: HashMap<String, String>,
    field_locals: HashMap<String, mir::LocalId>,
}

mod conditional;
mod diagnostic;
mod enum_data;
mod enum_data_match;
#[cfg(test)]
mod enum_data_match_tests;
#[cfg(test)]
mod enum_data_tests;
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
mod result;
mod result_match;
#[cfg(test)]
mod result_tests;
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
        if let Some(registry) = crate::hir::aggregate_registry::build(types)
        {
            self = self.with_aggregate_types(&registry);
        }
        self
    }

    #[must_use]
    pub(crate) fn with_collection_types(
        mut self,
        registry: &crate::hir::collection_registry::CollectionRegistry,
    ) -> Self
    {
        self.array_types = registry
            .arrays
            .iter()
            .map(|layout| (layout.source_type.clone(), layout.value_type))
            .collect();
        self.array_layouts = registry
            .arrays
            .iter()
            .map(|layout| (layout.value_type, layout.element_type, layout.length))
            .collect();
        self
    }

    #[must_use]
    pub(crate) fn with_aggregate_types(mut self, registry: &crate::hir::aggregate_registry::AggregateRegistry) -> Self
    {
        self.aggregate_types.clone_from(&registry.types);
        self.aggregate_layouts = registry
            .layouts
            .iter()
            .map(|layout| (layout.value_type, layout.fields.clone()))
            .collect();
        self.enum_data_layouts.clone_from(&registry.enum_data);
        self.tuple_types.clone_from(&registry.tuples);
        self.optional_types.clone_from(&registry.optionals);
        self.optional_layouts = registry
            .optionals
            .iter()
            .filter_map(|(source, value_type)| {
                let Type::Optional {
                    ..
                } = source
                else
                {
                    return None;
                };
                let element_type = registry
                    .layouts
                    .get(value_type.registry_id as usize)?
                    .fields
                    .get(1)
                    .copied()?;
                Some((*value_type, element_type))
            })
            .collect();
        self.result_types.clone_from(&registry.results);
        self.result_layouts = registry
            .results
            .iter()
            .filter_map(|(_, value_type)| {
                let fields = registry.layouts.get(value_type.registry_id as usize)?.fields.as_slice();
                let [tag, success, error] = fields
                else
                {
                    return None;
                };
                (*tag == XlilType::BOOL).then_some((*value_type, *success, *error))
            })
            .collect();
        self
    }

    pub fn lower_function(self, function: &Function) -> Result<mir::Function, Vec<Diagnostic>>
    {
        self.lower_function_with_parameters(function, 0)
    }

    pub fn lower_desugared_function(self, function: &DesugaredFunction) -> Result<mir::Function, Vec<Diagnostic>>
    {
        self.lower_desugared_function_with_parameters(function, 0)
    }

    pub fn lower_desugared_function_with_parameters(
        mut self,
        function: &DesugaredFunction,
        parameter_count: usize,
    ) -> Result<mir::Function, Vec<Diagnostic>>
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
        let surface = Function {
            name: function.name.clone(),
            return_type: function.return_type.clone(),
            locals: function.locals.clone(),
            body,
        };
        self.lower_function_with_parameters(&surface, parameter_count)
    }

    fn lower_statement(&mut self, statement: &Statement, lowered: &mut mir::Function)
    {
        match statement
        {
            Statement::Let {
                local,
                initializer,
            } =>
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
            Statement::Expr(Expression::Assign {
                target,
                value,
                span,
            }) =>
            {
                if let Some(type_name) = self.nominal_locals.get(target).cloned()
                {
                    self.lower_nominal_root_assignment(target, &type_name, value, *span, lowered);
                    return;
                }
                let Some(id) = self.locals.get(target).copied()
                else
                {
                    self.report(
                        DiagnosticCode::UnknownLocal,
                        format!("unknown HIR local '{target}'"),
                        *span,
                    );
                    return;
                };
                self.lower_assignment(id, value, lowered);
            }
            Statement::Expr(Expression::AssignField {
                target,
                value,
                ..
            }) => self.lower_field_assignment(target, value, lowered),
            Statement::AssignIndex {
                target,
                index,
                value,
                element_type,
                span,
            } => self.lower_index_assignment(target, index, value, element_type, *span, lowered),
            Statement::AssignTupleElement {
                ..
            } => self.lower_tuple_assignment(statement, lowered),
            Statement::Expr(
                expression @ Expression::Update {
                    ..
                },
            ) =>
            {
                let Some(value_type) = self.expression_value_type(expression, lowered)
                else
                {
                    self.unsupported_expression(expression);
                    return;
                };
                let _ = self.lower_update_expression(expression, value_type, lowered);
            }
            Statement::Expr(
                expression @ Expression::OptionalCoalesceAssign {
                    ..
                },
            ) =>
            {
                let Some(value_type) = self.expression_value_type(expression, lowered)
                else
                {
                    self.unsupported_expression(expression);
                    return;
                };
                let _ = self.lower_expression_to_local(expression, value_type, lowered);
            }
            Statement::Expr(
                expression @ Expression::Call {
                    ..
                },
            ) => self.lower_call_statement(expression, lowered),
            Statement::Expr(expression) => self.unsupported_expression(expression),
            Statement::Return {
                value,
                span,
            } => self.lower_return(value.as_ref(), *span, lowered),
            Statement::If {
                ..
            } => self.lower_if_statement(statement, lowered),
            Statement::While {
                ..
            } => self.lower_while_statement(statement, lowered),
            Statement::For {
                ..
            } => self.lower_for_statement(statement, lowered),
            Statement::ForEach {
                ..
            } => self.lower_for_each_statement(statement, lowered),
            Statement::Match {
                ..
            } => self.lower_match_statement(statement, lowered),
            Statement::Break {
                span,
            } => self.lower_loop_jump(false, *span, lowered),
            Statement::Continue {
                span,
            } => self.lower_loop_jump(true, *span, lowered),
            Statement::Panic {
                span,
            } => self.lower_panic(*span, lowered),
        }
    }

    fn lower_assignment(&mut self, target: mir::LocalId, expression: &Expression, lowered: &mut mir::Function)
    {
        let Some(value_type) = self.local_value_type(target, lowered)
        else
        {
            self.report(
                DiagnosticCode::UnsupportedType,
                "assignment target has no MIR value type",
                expression_span(expression),
            );
            return;
        };
        let Some(value) = self.lower_expression_to_local(expression, value_type, lowered)
        else
        {
            return;
        };
        self.current_block_mut(lowered)
            .statements
            .push(mir::Statement::StoreLocal {
                local: target,
                value,
                span: expression_span(expression),
            });
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
            Some(
                expression @ Expression::If {
                    ..
                },
            ) =>
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

    fn lower_expression_to_local(
        &mut self,
        expression: &Expression,
        expected_type: XlilType,
        lowered: &mut mir::Function,
    ) -> Option<mir::LocalId>
    {
        match expression
        {
            Expression::Local {
                name,
                span,
            } =>
            {
                if let Some(type_name) = self.nominal_locals.get(name).cloned() &&
                    self.aggregate_types.get(&type_name).copied() == Some(expected_type)
                {
                    return self.lower_nominal_place_value(name, &type_name, *span, lowered);
                }
                let Some(local) = self.locals.get(name).copied()
                else
                {
                    self.report(
                        DiagnosticCode::UnknownLocal,
                        format!("unknown HIR local '{name}'"),
                        *span,
                    );
                    return None;
                };
                if self.local_value_type(local, lowered) != Some(expected_type)
                {
                    self.report(
                        DiagnosticCode::UnsupportedType,
                        "HIR local type does not match the expected MIR expression type",
                        *span,
                    );
                    return None;
                }
                if self.storage_locals.contains(&local)
                {
                    let result = self.declare_temp(expected_type, *span, lowered)?;
                    self.current_block_mut(lowered)
                        .statements
                        .push(mir::Statement::LoadLocal {
                            result,
                            local,
                            span: *span,
                        });
                    Some(result)
                }
                else
                {
                    Some(local)
                }
            }
            Expression::Field {
                path,
            } => self.lower_field_load(path, expected_type, lowered),
            Expression::Member {
                ..
            } => self.lower_member_value(expression, expected_type, lowered),
            Expression::Object {
                nominal_type,
                fields,
                span,
            } => self.lower_object_value(nominal_type, fields, *span, lowered),
            Expression::EnumData {
                ..
            } => self.lower_enum_data_value(expression, expected_type, lowered),
            Expression::Array {
                ..
            } => self.lower_array_expression(expression, expected_type, lowered),
            Expression::Tuple {
                ..
            } => self.lower_tuple_expression(expression, expected_type, lowered),
            Expression::TupleElement {
                ..
            } => self.lower_tuple_element(expression, expected_type, lowered),
            Expression::Set {
                ..
            } |
            Expression::Map {
                ..
            } =>
            {
                self.unsupported_expression(expression);
                None
            }
            Expression::Index {
                ..
            } => self.lower_index_expression(expression, expected_type, lowered),
            Expression::ArrayLength {
                collection,
                span,
            } => self.lower_array_length(collection, expected_type, *span, lowered),
            Expression::Literal {
                literal,
                span,
            } =>
            {
                let local = self.declare_temp(expected_type, *span, lowered)?;
                self.lower_literal_into(local, literal, *span, lowered);
                Some(local)
            }
            Expression::Binary {
                operator,
                left,
                right,
                span,
            } =>
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
            Expression::Unary {
                operator: UnaryOperator::Positive,
                operand,
                ..
            } => self.lower_expression_to_local(operand, expected_type, lowered),
            Expression::Unary {
                operator,
                operand,
                span,
            } =>
            {
                let local = self.declare_temp(expected_type, *span, lowered)?;
                self.lower_unary_into(local, *operator, operand, *span, lowered);
                Some(local)
            }
            Expression::OptionalUnwrap {
                value,
                span,
                ..
            } => self.lower_optional_unwrap(value, expected_type, *span, lowered),
            Expression::OptionalCoalesceAssign {
                target,
                value,
                span,
                ..
            } => self.lower_optional_coalesce_assign(target, value, expected_type, *span, lowered),
            Expression::OptionalMember {
                ..
            } => self.lower_optional_member(expression, expected_type, lowered),
            Expression::Assign {
                ..
            } |
            Expression::AssignField {
                ..
            } =>
            {
                self.unsupported_expression(expression);
                None
            }
            Expression::Update {
                ..
            } => self.lower_update_expression(expression, expected_type, lowered),
            Expression::ResultPropagation {
                ..
            } => self.lower_result_propagation(expression, expected_type, lowered),
            Expression::Call {
                span, ..
            } =>
            {
                let local = self.declare_temp(expected_type, *span, lowered)?;
                self.lower_call_into(local, expression, lowered);
                Some(local)
            }
            Expression::If {
                ..
            } => self.lower_if_expression(expression, expected_type, lowered),
            Expression::Match {
                ..
            } => self.lower_match_expression(expression, expected_type, lowered),
        }
    }
}
include!("mir_lowering/expression_and_blocks.rs");
