/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

use super::{Diagnostic, DiagnosticCode};
use crate::hir::type_check::{
    BinaryOperator, Expression, Literal, PrimitiveType, Type, UnaryOperator, literal_matches_type,
};

pub(super) fn unary_expression_type(operator: UnaryOperator, operand_type: Type) -> Option<Type>
{
    let Type::Primitive(primitive) = operand_type
    else
    {
        return None;
    };
    match operator
    {
        UnaryOperator::Positive | UnaryOperator::Negative
            if matches!(primitive, PrimitiveType::Long | PrimitiveType::Int) =>
        {
            Some(Type::Primitive(primitive))
        }
        UnaryOperator::LogicalNot if primitive == PrimitiveType::Bool => Some(Type::Primitive(PrimitiveType::Bool)),
        _ => None,
    }
}

pub(super) fn literal_default_type(literal: &Literal) -> Option<Type>
{
    let primitive = match literal
    {
        Literal::Bool(_) => PrimitiveType::Bool,
        Literal::Integer(_) => PrimitiveType::Int,
        Literal::Float(_) => PrimitiveType::Float,
        Literal::Char(_) => PrimitiveType::Char,
        Literal::String(_) => PrimitiveType::Str,
        Literal::None => return None,
        Literal::EnumVariant {
            enum_type, ..
        } => return Some(Type::Named(enum_type.clone())),
    };
    Some(Type::Primitive(primitive))
}

impl super::ResultDesugar
{
    pub(super) fn expression_type(&mut self, expression: &Expression) -> Option<Type>
    {
        match expression
        {
            Expression::Literal {
                literal, ..
            } => literal_default_type(literal),
            Expression::Local {
                name,
                span,
            } => self.find_local(name).map(|local| local.ty.clone()).or_else(|| {
                self.diagnostics.push(Diagnostic {
                    code: DiagnosticCode::UnknownLocal,
                    message: format!("unknown local '{name}'"),
                    span: *span,
                });
                None
            }),
            Expression::Field {
                path,
            } => Some(path.ty.clone()),
            Expression::Member {
                field_type, ..
            } => Some(field_type.as_ref().clone()),
            Expression::Object {
                nominal_type, ..
            } => Some(Type::Named(nominal_type.clone())),
            Expression::EnumData {
                enum_type, ..
            } => Some(Type::Named(enum_type.clone())),
            Expression::Array {
                elements, ..
            } =>
            {
                let first = self.expression_type(elements.first()?)?;
                elements
                    .iter()
                    .skip(1)
                    .all(|value| self.expression_type(value).as_ref() == Some(&first))
                    .then(|| Type::Array {
                        element: Box::new(first),
                        length: u64::try_from(elements.len()).ok(),
                    })
            }
            Expression::Set {
                elements, ..
            } =>
            {
                let first = self.expression_type(elements.first()?)?;
                elements
                    .iter()
                    .skip(1)
                    .all(|value| self.expression_type(value).as_ref() == Some(&first))
                    .then(|| Type::Set {
                        element: Box::new(first),
                    })
            }
            Expression::Map {
                entries, ..
            } =>
            {
                let first = entries.first()?;
                let key = self.expression_type(&first.key)?;
                let value = self.expression_type(&first.value)?;
                entries
                    .iter()
                    .skip(1)
                    .all(|entry| {
                        self.expression_type(&entry.key).as_ref() == Some(&key) &&
                            self.expression_type(&entry.value).as_ref() == Some(&value)
                    })
                    .then(|| Type::Map {
                        key: Box::new(key),
                        value: Box::new(value),
                    })
            }
            Expression::Tuple {
                tuple_type, ..
            } => Some(tuple_type.as_ref().clone()),
            Expression::TupleElement {
                element_type, ..
            } => Some(element_type.as_ref().clone()),
            Expression::Index {
                element_type, ..
            } => Some(element_type.as_ref().clone()),
            Expression::ArrayLength {
                ..
            } => Some(Type::Primitive(PrimitiveType::Int)),
            Expression::Assign {
                value, ..
            } => self.expression_type(value),
            Expression::AssignField {
                value, ..
            } => self.expression_type(value),
            Expression::Update {
                target,
                span,
                ..
            } => self.find_local(target).map(|local| local.ty.clone()).or_else(|| {
                self.diagnostics.push(Diagnostic {
                    code: DiagnosticCode::UnknownLocal,
                    message: format!("unknown local '{target}'"),
                    span: *span,
                });
                None
            }),
            Expression::Binary {
                operator,
                left,
                right,
                ..
            } => self.binary_expression_type(*operator, left, right),
            Expression::Unary {
                operator,
                operand,
                ..
            } => unary_expression_type(*operator, self.expression_type(operand)?),
            Expression::OptionalUnwrap {
                element_type, ..
            } => Some(element_type.as_ref().clone()),
            Expression::OptionalCoalesceAssign {
                optional_type, ..
            } => Some(optional_type.as_ref().clone()),
            Expression::OptionalMember {
                result_type, ..
            } => Some(result_type.as_ref().clone()),
            Expression::ResultPropagation {
                value,
                span,
            } => self
                .result_parts_of_expression(value, *span)
                .map(|(success, _)| success),
            Expression::Call {
                return_type, ..
            } => Some(return_type.as_ref().clone()),
            Expression::If {
                result_type, ..
            } => Some(result_type.as_ref().clone()),
            Expression::Match {
                result_type, ..
            } => Some(result_type.as_ref().clone()),
        }
    }

    pub(super) fn binary_expression_type(
        &mut self,
        operator: BinaryOperator,
        left: &Expression,
        right: &Expression,
    ) -> Option<Type>
    {
        if operator == BinaryOperator::Coalesce
        {
            let right_type = self.expression_type(right)?;
            if matches!(left, Expression::Literal {
                literal: Literal::None,
                ..
            })
            {
                return Some(right_type);
            }
            let Type::Optional {
                element,
            } = self.expression_type(left)?
            else
            {
                return None;
            };
            return (*element == right_type).then_some(*element);
        }
        let mut left_type = self.expression_type(left)?;
        let mut right_type = self.expression_type(right)?;
        if left_type != right_type
        {
            if let Expression::Literal {
                literal, ..
            } = left &&
                literal_matches_type(literal, &right_type)
            {
                left_type = right_type.clone();
            }
            else if let Expression::Literal {
                literal, ..
            } = right &&
                literal_matches_type(literal, &left_type)
            {
                right_type = left_type.clone();
            }
        }
        if left_type != right_type
        {
            return None;
        }
        let Type::Primitive(primitive) = left_type
        else
        {
            return None;
        };
        match operator
        {
            BinaryOperator::LogicalAnd | BinaryOperator::LogicalOr if primitive == PrimitiveType::Bool =>
            {
                Some(Type::Primitive(PrimitiveType::Bool))
            }
            BinaryOperator::Add |
            BinaryOperator::Sub |
            BinaryOperator::Mul |
            BinaryOperator::Div |
            BinaryOperator::Rem |
            BinaryOperator::BitAnd |
            BinaryOperator::BitOr |
            BinaryOperator::BitXor |
            BinaryOperator::ShiftLeft |
            BinaryOperator::ShiftRight
                if matches!(primitive, PrimitiveType::Long | PrimitiveType::Int) =>
            {
                Some(Type::Primitive(primitive))
            }
            BinaryOperator::Equal | BinaryOperator::NotEqual
                if matches!(primitive, PrimitiveType::Long | PrimitiveType::Int) =>
            {
                Some(Type::Primitive(PrimitiveType::Bool))
            }
            BinaryOperator::Less |
            BinaryOperator::LessEqual |
            BinaryOperator::Greater |
            BinaryOperator::GreaterEqual
                if primitive == PrimitiveType::Long =>
            {
                Some(Type::Primitive(PrimitiveType::Bool))
            }
            _ => None,
        }
    }
}
