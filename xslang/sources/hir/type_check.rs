/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::async_check::Span;
use super::match_model::{MatchArm, MatchPattern};

use binary_type::is_supported_integer;

mod model;
pub use model::*;

mod diagnostic;
pub use diagnostic::{Diagnostic, DiagnosticCode};
#[cfg(test)]
mod enum_data_match_tests;

#[derive(Default)]
pub struct TypeChecker
{
    locals: Vec<Local>,
    diagnostics: Vec<Diagnostic>,
    return_type: Option<Type>,
    scope_starts: Vec<usize>,
    loop_depth: usize,
    nominal_types: std::collections::HashMap<String, crate::hir::declarations::NominalType>,
    enum_data: crate::hir::enum_data::EnumDataRegistry,
}

impl TypeChecker
{
    #[must_use]
    pub fn new() -> Self
    {
        Self::default()
    }

    #[must_use]
    pub fn with_nominal_types(mut self, types: &[crate::hir::declarations::NominalType]) -> Self
    {
        self.enum_data = crate::hir::enum_data::EnumDataRegistry::build(types);
        self.nominal_types = types.iter().map(|ty| (ty.name.clone(), ty.clone())).collect();
        self
    }

    #[must_use]
    pub fn check_function(mut self, function: &Function) -> Vec<Diagnostic>
    {
        self.locals.extend(function.locals.iter().cloned());
        self.scope_starts.push(0);
        self.return_type = function.return_type.clone();
        for statement in &function.body
        {
            self.check_statement(statement, function.return_type.as_ref());
        }
        self.diagnostics
    }

    fn check_statement(&mut self, statement: &Statement, return_type: Option<&Type>)
    {
        match statement
        {
            Statement::Let {
                local,
                initializer,
            } =>
            {
                if let Some(initializer) = initializer
                {
                    self.check_expression_against_type(initializer, &local.ty);
                }
                let scope_start = self.scope_starts.last().copied().unwrap_or(0);
                if self.locals[scope_start..]
                    .iter()
                    .any(|existing| existing.name == local.name)
                {
                    self.diagnostics.push(Diagnostic {
                        code: DiagnosticCode::DuplicateLocal,
                        message: format!("local '{}' is already declared in this scope", local.name),
                        span: local.span,
                    });
                }
                else
                {
                    self.locals.push(local.clone());
                }
            }
            Statement::Expr(expression) => self.check_expression(expression),
            Statement::AssignIndex {
                target,
                index,
                value,
                element_type,
                span,
            } => self.check_index_assignment(target, index, value, element_type, *span),
            Statement::AssignTupleElement {
                target,
                index,
                value,
                tuple_type,
                element_type,
                span,
            } => self.check_tuple_assignment(target, *index, value, tuple_type, element_type, *span),
            Statement::Return {
                value,
                span,
            } =>
            {
                if let (Some(value), Some(return_type)) = (value, return_type)
                {
                    self.check_expression_against_type(value, return_type);
                }
                if value.is_none() && return_type.is_some()
                {
                    self.diagnostics.push(Diagnostic {
                        code: DiagnosticCode::LiteralTypeMismatch,
                        message: "return without a value is not assignable to the function return type".to_string(),
                        span: *span,
                    });
                }
            }
            Statement::If {
                condition,
                then_block,
                else_block,
                span,
            } =>
            {
                self.check_condition(condition, *span);
                self.check_block(then_block, None);
                if let Some(else_block) = else_block
                {
                    self.check_block(else_block, None);
                }
            }
            Statement::While {
                condition,
                body,
                span,
            } =>
            {
                self.check_condition(condition, *span);
                self.loop_depth += 1;
                self.check_block(body, None);
                self.loop_depth -= 1;
            }
            Statement::For {
                initializer,
                condition,
                update,
                body,
                span,
            } => self.check_for_statement(
                initializer.as_deref(),
                condition.as_ref(),
                update.as_ref(),
                body,
                *span,
                return_type,
            ),
            Statement::ForEach {
                binding,
                iterable,
                iterable_type,
                body,
                span,
            } => self.check_for_each_statement(binding, iterable, iterable_type, body, *span),
            Statement::Match {
                selector,
                selector_type,
                arms,
                span,
            } => self.check_match_statement(selector, selector_type, arms, *span),
            Statement::Break {
                span,
            } if self.loop_depth == 0 =>
            {
                self.diagnostics.push(Diagnostic {
                    code: DiagnosticCode::BreakOutsideLoop,
                    message: "break may only appear inside a loop".to_string(),
                    span: *span,
                });
            }
            Statement::Continue {
                span,
            } if self.loop_depth == 0 =>
            {
                self.diagnostics.push(Diagnostic {
                    code: DiagnosticCode::ContinueOutsideLoop,
                    message: "continue may only appear inside a loop".to_string(),
                    span: *span,
                });
            }
            Statement::Break {
                ..
            } |
            Statement::Continue {
                ..
            } =>
            {}
            Statement::Panic {
                ..
            } =>
            {}
        }
    }

    fn check_expression(&mut self, expression: &Expression)
    {
        match expression
        {
            Expression::Assign {
                target,
                value,
                span,
            } =>
            {
                let Some(local) = self.find_local(target).cloned()
                else
                {
                    self.diagnostics.push(Diagnostic {
                        code: DiagnosticCode::UnknownLocal,
                        message: format!("unknown local '{target}'"),
                        span: *span,
                    });
                    self.check_expression(value);
                    return;
                };

                if !local.mutable
                {
                    self.diagnostics.push(Diagnostic {
                        code: DiagnosticCode::ImmutableAssignment,
                        message: format!("cannot reassign binding '{target}'"),
                        span: *span,
                    });
                }
                self.check_expression_against_type(value, &local.ty);
            }
            Expression::AssignField {
                target,
                value,
                span,
            } =>
            {
                if !target.mutable
                {
                    self.diagnostics.push(Diagnostic {
                        code: DiagnosticCode::ImmutableAssignment,
                        message: "cannot assign to an immutable field".to_string(),
                        span: *span,
                    });
                }
                self.check_expression_against_type(value, &target.ty);
            }
            Expression::Update {
                target,
                span,
                ..
            } =>
            {
                let Some(local) = self.find_local(target).cloned()
                else
                {
                    self.diagnostics.push(Diagnostic {
                        code: DiagnosticCode::UnknownLocal,
                        message: format!("unknown local '{target}'"),
                        span: *span,
                    });
                    return;
                };
                if !local.mutable
                {
                    self.diagnostics.push(Diagnostic {
                        code: DiagnosticCode::ImmutableAssignment,
                        message: format!("cannot update binding '{target}'"),
                        span: *span,
                    });
                }
                if !matches!(local.ty, Type::Primitive(primitive) if is_supported_integer(primitive))
                {
                    self.diagnostics.push(Diagnostic {
                        code: DiagnosticCode::UnaryTypeMismatch,
                        message: "increment and decrement require an integer binding".to_string(),
                        span: *span,
                    });
                }
            }
            Expression::Binary {
                operator,
                left,
                right,
                span,
            } =>
            {
                self.check_expression(left);
                self.check_expression(right);
                if self.binary_expression_type(*operator, left, right).is_none()
                {
                    self.diagnostics.push(Diagnostic {
                        code: DiagnosticCode::BinaryTypeMismatch,
                        message: "binary expression operands are not valid for this operator".to_string(),
                        span: *span,
                    });
                }
            }
            Expression::Unary {
                operator,
                operand,
                span,
            } =>
            {
                self.check_expression(operand);
                if self.unary_expression_type(*operator, operand).is_none()
                {
                    self.diagnostics.push(Diagnostic {
                        code: DiagnosticCode::UnaryTypeMismatch,
                        message: "unary expression operand is not valid for this operator".to_string(),
                        span: *span,
                    });
                }
            }
            Expression::OptionalUnwrap {
                value,
                element_type,
                span,
            } =>
            {
                self.check_expression(value);
                let expected = Type::Optional {
                    element: element_type.clone(),
                };
                if self.expression_type(value).as_ref() != Some(&expected)
                {
                    self.diagnostics.push(Diagnostic {
                        code: DiagnosticCode::UnaryTypeMismatch,
                        message: "optional-forgiving '!' requires Optional<T>".to_string(),
                        span: *span,
                    });
                }
            }
            Expression::OptionalCoalesceAssign {
                target,
                value,
                optional_type,
                span,
            } =>
            {
                let Some(local) = self.find_local(target).cloned()
                else
                {
                    self.diagnostics.push(Diagnostic {
                        code: DiagnosticCode::UnknownLocal,
                        message: format!("unknown local '{target}'"),
                        span: *span,
                    });
                    return;
                };
                if !local.mutable
                {
                    self.diagnostics.push(Diagnostic {
                        code: DiagnosticCode::ImmutableAssignment,
                        message: format!("cannot update binding '{target}'"),
                        span: *span,
                    });
                }
                if local.ty != **optional_type || !optional_type.is_optional()
                {
                    self.diagnostics.push(Diagnostic {
                        code: DiagnosticCode::BinaryTypeMismatch,
                        message: "coalescing assignment requires an Optional<T> binding".to_string(),
                        span: *span,
                    });
                    return;
                }
                self.check_expression_against_type(value, optional_type);
            }
            Expression::ResultPropagation {
                value,
                span,
            } =>
            {
                self.check_expression(value);
                self.check_result_propagation(value, *span);
            }
            Expression::Call {
                arguments,
                parameter_types,
                span,
                ..
            } =>
            {
                if arguments.len() != parameter_types.len()
                {
                    self.diagnostics.push(Diagnostic {
                        code: DiagnosticCode::LiteralTypeMismatch,
                        message: "call argument count does not match its typed signature".to_string(),
                        span: *span,
                    });
                }
                for (argument, parameter_type) in arguments.iter().zip(parameter_types)
                {
                    self.check_expression_against_type(argument, parameter_type);
                }
            }
            Expression::If {
                condition,
                then_block,
                else_block,
                result_type,
                span,
            } =>
            {
                self.check_condition(condition, *span);
                self.check_block(then_block, Some(result_type));
                self.check_block(else_block, Some(result_type));
            }
            Expression::Match {
                selector,
                selector_type,
                arms,
                result_type,
                span,
            } => self.check_match_expression(selector, selector_type, arms, result_type, *span),
            Expression::Local {
                name,
                span,
            } =>
            {
                if self.find_local(name).is_none()
                {
                    self.diagnostics.push(Diagnostic {
                        code: DiagnosticCode::UnknownLocal,
                        message: format!("unknown local '{name}'"),
                        span: *span,
                    });
                }
            }
            Expression::Field {
                path,
            } =>
            {
                self.check_field_path(path);
            }
            Expression::Member {
                receiver,
                owner,
                name,
                field_type,
                span,
            } => self.check_member(receiver, owner, name, field_type, *span),
            Expression::OptionalMember {
                receiver,
                owner,
                name,
                field_type,
                result_type,
                span,
            } =>
            {
                self.check_optional_member(receiver, owner, name, field_type, result_type, *span);
            }
            Expression::Object {
                nominal_type,
                fields,
                span,
            } => self.check_object(nominal_type, fields, *span),
            Expression::EnumData {
                enum_type,
                owner,
                variant,
                tag,
                payload,
                payload_type,
                span,
                ..
            } =>
            {
                let selected = self.enum_data.select(enum_type, variant, payload_type.as_deref());
                if !selected.is_ok_and(|selected| selected.owner == *owner && selected.tag == *tag)
                {
                    self.diagnostics.push(Diagnostic {
                        code: DiagnosticCode::UnknownEnumVariant,
                        message: format!(
                            "enum-data variant '{enum_type}::{variant}' does not match its recorded payload, owner, \
                             or tag"
                        ),
                        span: *span,
                    });
                }
                match (payload.as_deref(), payload_type.as_deref())
                {
                    (Some(value), Some(ty)) => self.check_expression_against_type(value, ty),
                    (None, None) =>
                    {}
                    _ => self.diagnostics.push(Diagnostic {
                        code: DiagnosticCode::LiteralTypeMismatch,
                        message: "enum-data payload metadata is inconsistent".to_string(),
                        span: *span,
                    }),
                }
            }
            Expression::Array {
                elements, ..
            } |
            Expression::Set {
                elements, ..
            } =>
            {
                for element in elements
                {
                    self.check_expression(element);
                }
            }
            Expression::Map {
                entries, ..
            } =>
            {
                for entry in entries
                {
                    self.check_expression(&entry.key);
                    self.check_expression(&entry.value);
                }
            }
            Expression::Tuple {
                fields,
                tuple_type,
                span,
            } => self.check_tuple(fields, tuple_type, *span),
            Expression::TupleElement {
                tuple,
                index,
                element_type,
                span,
            } => self.check_tuple_element(tuple, *index, element_type, *span),
            Expression::Index {
                collection,
                index,
                element_type,
                span,
            } =>
            {
                self.check_expression(collection);
                self.check_expression(index);
                let collection_type = self.expression_type(collection);
                if !matches!(collection_type, Some(Type::Array { element, .. }) if element.as_ref() == element_type.as_ref())
                {
                    self.report_collection_mismatch(
                        *span,
                        "index source must be an array with the recorded element type",
                    );
                }
                if !matches!(self.expression_type(index), Some(Type::Primitive(primitive)) if is_supported_integer(primitive))
                {
                    self.report_collection_mismatch(*span, "array index must have an integer type");
                }
            }
            Expression::ArrayLength {
                collection,
                span,
            } =>
            {
                self.check_expression(collection);
                if !matches!(self.expression_type(collection), Some(Type::Array { .. }))
                {
                    self.report_collection_mismatch(*span, "array length source must have an array type");
                }
            }
            Expression::Literal {
                ..
            } =>
            {}
        }
    }

    fn check_expression_against_type(&mut self, expression: &Expression, ty: &Type)
    {
        match expression
        {
            Expression::Literal {
                literal:
                    Literal::EnumVariant {
                        enum_type,
                        variant,
                        tag,
                    },
                span,
            } =>
            {
                let valid = self
                    .nominal_types
                    .get(enum_type)
                    .filter(|definition| definition.kind == crate::hir::declarations::NominalKind::Enum)
                    .and_then(|definition| definition.variants.iter().find(|candidate| candidate.name == *variant))
                    .is_some_and(|candidate| candidate.tag == *tag) &&
                    ty == &Type::Named(enum_type.clone());
                if !valid
                {
                    self.diagnostics.push(Diagnostic {
                        code: DiagnosticCode::UnknownEnumVariant,
                        message: format!("enum variant '{enum_type}::{variant}' is not valid for the target type"),
                        span: *span,
                    });
                }
            }
            Expression::Literal {
                literal,
                span,
            } if !literal_matches_type(literal, ty) =>
            {
                self.diagnostics.push(Diagnostic {
                    code: DiagnosticCode::LiteralTypeMismatch,
                    message: "literal is not assignable to the target type".to_string(),
                    span: *span,
                });
            }
            Expression::Binary {
                operator,
                left,
                right,
                span,
            } =>
            {
                self.check_expression(expression);
                if !self.binary_expression_matches_type(*operator, left, right, ty)
                {
                    self.diagnostics.push(Diagnostic {
                        code: DiagnosticCode::LiteralTypeMismatch,
                        message: "expression is not assignable to the target type".to_string(),
                        span: *span,
                    });
                }
            }
            Expression::Unary {
                operator,
                operand,
                span,
            } =>
            {
                self.check_expression(expression);
                if !self.unary_expression_matches_type(*operator, operand, ty)
                {
                    self.diagnostics.push(Diagnostic {
                        code: DiagnosticCode::LiteralTypeMismatch,
                        message: "unary expression is not assignable to the target type".to_string(),
                        span: *span,
                    });
                }
            }
            Expression::Local {
                name,
                span,
            } =>
            {
                let Some(local) = self.find_local(name)
                else
                {
                    self.diagnostics.push(Diagnostic {
                        code: DiagnosticCode::UnknownLocal,
                        message: format!("unknown local '{name}'"),
                        span: *span,
                    });
                    return;
                };
                if local.ty != *ty
                {
                    self.diagnostics.push(Diagnostic {
                        code: DiagnosticCode::LiteralTypeMismatch,
                        message: "local expression is not assignable to the target type".to_string(),
                        span: *span,
                    });
                }
            }
            Expression::Field {
                path,
            } => self.check_field_against_type(expression, path, ty),
            Expression::Member {
                field_type,
                span,
                ..
            } => self.check_member_against_type(expression, field_type, *span, ty),
            Expression::Assign {
                span, ..
            } |
            Expression::AssignField {
                span, ..
            } |
            Expression::Update {
                span, ..
            } =>
            {
                self.check_expression(expression);
                if self.expression_type(expression).as_ref() != Some(ty)
                {
                    self.diagnostics.push(Diagnostic {
                        code: DiagnosticCode::LiteralTypeMismatch,
                        message: "updated value is not assignable to the target type".to_string(),
                        span: *span,
                    });
                }
            }
            Expression::OptionalUnwrap {
                element_type,
                span,
                ..
            } =>
            {
                self.check_expression(expression);
                if element_type.as_ref() != ty
                {
                    self.diagnostics.push(Diagnostic {
                        code: DiagnosticCode::LiteralTypeMismatch,
                        message: "unboxed Optional value is not assignable to the target type".to_string(),
                        span: *span,
                    });
                }
            }
            Expression::OptionalCoalesceAssign {
                optional_type,
                span,
                ..
            } =>
            {
                self.check_expression(expression);
                if optional_type.as_ref() != ty
                {
                    self.diagnostics.push(Diagnostic {
                        code: DiagnosticCode::LiteralTypeMismatch,
                        message: "coalescing assignment result is not assignable to the target type".to_string(),
                        span: *span,
                    });
                }
            }
            Expression::OptionalMember {
                result_type,
                span,
                ..
            } =>
            {
                self.check_expression(expression);
                if result_type.as_ref() != ty
                {
                    self.diagnostics.push(Diagnostic {
                        code: DiagnosticCode::LiteralTypeMismatch,
                        message: "optional member value is not assignable to the target type".to_string(),
                        span: *span,
                    });
                }
            }
            Expression::ResultPropagation {
                span, ..
            } =>
            {
                self.check_expression(expression);
                let Some(result_type) = self.expression_type(expression)
                else
                {
                    return;
                };
                if result_type != *ty
                {
                    self.diagnostics.push(Diagnostic {
                        code: DiagnosticCode::LiteralTypeMismatch,
                        message: "Result propagation success value is not assignable to the target type".to_string(),
                        span: *span,
                    });
                }
            }
            Expression::Call {
                span, ..
            } =>
            {
                self.check_expression(expression);
                if self.expression_type(expression).as_ref() != Some(ty)
                {
                    self.diagnostics.push(Diagnostic {
                        code: DiagnosticCode::LiteralTypeMismatch,
                        message: "call result is not assignable to the target type".to_string(),
                        span: *span,
                    });
                }
            }
            Expression::If {
                span, ..
            } =>
            {
                self.check_expression(expression);
                if self.expression_type(expression).as_ref() != Some(ty)
                {
                    self.diagnostics.push(Diagnostic {
                        code: DiagnosticCode::LiteralTypeMismatch,
                        message: "if expression result is not assignable to the target type".to_string(),
                        span: *span,
                    });
                }
            }
            Expression::Match {
                span, ..
            } =>
            {
                self.check_expression(expression);
                if self.expression_type(expression).as_ref() != Some(ty)
                {
                    self.diagnostics.push(Diagnostic {
                        code: DiagnosticCode::LiteralTypeMismatch,
                        message: "match expression result is not assignable to the target type".to_string(),
                        span: *span,
                    });
                }
            }
            Expression::Object {
                span, ..
            } =>
            {
                self.check_expression(expression);
                if self.expression_type(expression).as_ref() != Some(ty)
                {
                    self.diagnostics.push(Diagnostic {
                        code: DiagnosticCode::LiteralTypeMismatch,
                        message: "object literal is not assignable to the target type".to_string(),
                        span: *span,
                    });
                }
            }
            Expression::EnumData {
                enum_type,
                span,
                ..
            } =>
            {
                self.check_expression(expression);
                if ty != &Type::Named(enum_type.clone())
                {
                    self.diagnostics.push(Diagnostic {
                        code: DiagnosticCode::LiteralTypeMismatch,
                        message: "enum-data value is not assignable to the target type".to_string(),
                        span: *span,
                    });
                }
            }
            Expression::Array {
                elements,
                span,
            } =>
            {
                let Type::Array {
                    element,
                    length,
                } = ty
                else
                {
                    self.report_collection_mismatch(*span, "array literal requires an array target type");
                    return;
                };
                if length.is_some_and(|expected| u64::try_from(elements.len()).ok() != Some(expected))
                {
                    self.report_collection_mismatch(
                        *span,
                        "fixed-size array literal length does not match its target type",
                    );
                }
                for value in elements
                {
                    self.check_expression_against_type(value, element);
                }
            }
            Expression::Set {
                elements,
                span,
            } =>
            {
                let Type::Set {
                    element,
                } = ty
                else
                {
                    self.report_collection_mismatch(*span, "set literal requires a set target type");
                    return;
                };
                for value in elements
                {
                    self.check_expression_against_type(value, element);
                }
            }
            Expression::Map {
                entries,
                span,
            } =>
            {
                let Type::Map {
                    key,
                    value,
                } = ty
                else
                {
                    self.report_collection_mismatch(*span, "map literal requires a map target type");
                    return;
                };
                for entry in entries
                {
                    self.check_expression_against_type(&entry.key, key);
                    self.check_expression_against_type(&entry.value, value);
                }
            }
            Expression::Tuple {
                fields,
                tuple_type,
                span,
            } => self.check_tuple_against_type(fields, tuple_type, *span, ty),
            Expression::TupleElement {
                span, ..
            } => self.check_tuple_element_against_type(expression, *span, ty),
            Expression::Index {
                span, ..
            } =>
            {
                self.check_expression(expression);
                if self.expression_type(expression).as_ref() != Some(ty)
                {
                    self.report_collection_mismatch(*span, "indexed element is not assignable to the target type");
                }
            }
            Expression::ArrayLength {
                span, ..
            } =>
            {
                self.check_expression(expression);
                if *ty != Type::Primitive(PrimitiveType::Int)
                {
                    self.report_collection_mismatch(*span, "array length has type Int");
                }
            }
            Expression::Literal {
                ..
            } =>
            {}
        }
    }
}

include!("type_check/modules.rs");

#[cfg(test)]
mod enum_data_tests;
#[cfg(test)]
mod tests;
