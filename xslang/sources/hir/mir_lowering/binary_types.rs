/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::*;

impl HirToMirLowerer
{
    pub(super) fn binary_operand_type(
        &self,
        operator: BinaryOperator,
        target_type: XlilType,
        left: &Expression,
        right: &Expression,
        lowered: &mir::Function,
    ) -> Option<XlilType>
    {
        match (operator, target_type)
        {
            (operator, value_type) if value_type.is_integer() && integer_operation(operator).is_some() =>
            {
                Some(value_type)
            }
            (BinaryOperator::Add | BinaryOperator::Sub | BinaryOperator::Mul, XlilType::I32) => Some(XlilType::I32),
            (operator, XlilType::I32) if binary_i32_operation(operator).is_some() => Some(XlilType::I32),
            (BinaryOperator::Add | BinaryOperator::Sub | BinaryOperator::Mul, XlilType::I64) => Some(XlilType::I64),
            (operator, XlilType::I64) if binary_i64_operation(operator).is_some() => Some(XlilType::I64),
            (operator, XlilType::F32 | XlilType::F64) if binary_float_operation(operator).is_some() =>
            {
                Some(target_type)
            }
            (
                BinaryOperator::Equal |
                BinaryOperator::NotEqual |
                BinaryOperator::Less |
                BinaryOperator::LessEqual |
                BinaryOperator::Greater |
                BinaryOperator::GreaterEqual,
                XlilType::BOOL,
            ) => self.common_operand_type(left, right, lowered).or(Some(XlilType::I64)),
            _ => None,
        }
    }

    fn common_operand_type(&self, left: &Expression, right: &Expression, lowered: &mir::Function) -> Option<XlilType>
    {
        let left_type = self.expression_value_type(left, lowered);
        let right_type = self.expression_value_type(right, lowered);
        match (left_type, right_type)
        {
            (Some(left_type), Some(right_type)) if left_type == right_type => Some(left_type),
            (Some(value_type), None) | (None, Some(value_type)) => Some(value_type),
            _ => None,
        }
    }

    pub(super) fn expression_value_type(&self, expression: &Expression, lowered: &mir::Function) -> Option<XlilType>
    {
        match expression
        {
            Expression::Local {
                name, ..
            } => self.local_value_type(*self.locals.get(name)?, lowered),
            Expression::Field {
                path,
            } => self.known_value_type(&path.ty),
            Expression::Member {
                field_type, ..
            } => self.known_value_type(field_type),
            Expression::Object {
                ..
            } => None,
            Expression::EnumData {
                enum_type, ..
            } => self.aggregate_types.get(enum_type).copied(),
            Expression::Array {
                ..
            } |
            Expression::Set {
                ..
            } |
            Expression::Map {
                ..
            } => None,
            Expression::Tuple {
                tuple_type, ..
            } => self
                .tuple_types
                .iter()
                .find(|(source, _)| source == tuple_type.as_ref())
                .map(|(_, value_type)| *value_type),
            Expression::TupleElement {
                element_type, ..
            } => self.known_value_type(element_type),
            Expression::Index {
                element_type, ..
            } => self.known_value_type(element_type),
            Expression::ArrayLength {
                ..
            } => Some(XlilType::I64),
            Expression::Update {
                target, ..
            } => self.local_value_type(*self.locals.get(target)?, lowered),
            Expression::Binary {
                operator,
                left,
                right,
                ..
            } => match operator
            {
                BinaryOperator::Add |
                BinaryOperator::Sub |
                BinaryOperator::Mul |
                BinaryOperator::Div |
                BinaryOperator::Rem |
                BinaryOperator::BitAnd |
                BinaryOperator::BitOr |
                BinaryOperator::BitXor |
                BinaryOperator::ShiftLeft |
                BinaryOperator::ShiftRight => self.common_operand_type(left, right, lowered),
                BinaryOperator::LogicalAnd | BinaryOperator::LogicalOr => Some(XlilType::BOOL),
                BinaryOperator::Coalesce => self.expression_value_type(right, lowered),
                BinaryOperator::Equal |
                BinaryOperator::NotEqual |
                BinaryOperator::Less |
                BinaryOperator::LessEqual |
                BinaryOperator::Greater |
                BinaryOperator::GreaterEqual => Some(XlilType::BOOL),
            },
            Expression::Unary {
                operator,
                operand,
                ..
            } => match operator
            {
                UnaryOperator::LogicalNot => Some(XlilType::BOOL),
                UnaryOperator::Positive | UnaryOperator::Negative => self.expression_value_type(operand, lowered),
            },
            Expression::OptionalUnwrap {
                element_type, ..
            } => self.known_value_type(element_type),
            Expression::OptionalCoalesceAssign {
                optional_type, ..
            } => self.known_value_type(optional_type),
            Expression::OptionalMember {
                result_type, ..
            } => self.known_value_type(result_type),
            Expression::Call {
                return_type, ..
            } => self.known_value_type(return_type),
            Expression::If {
                result_type, ..
            } |
            Expression::Match {
                result_type, ..
            } => self.known_value_type(result_type),
            Expression::Literal {
                literal, ..
            } => match literal
            {
                Literal::String(_) => Some(XlilType::STR),
                Literal::Bool(_) => Some(XlilType::BOOL),
                Literal::Char(_) => Some(XlilType::U32),
                Literal::EnumVariant {
                    enum_type, ..
                } => self.aggregate_types.get(enum_type).copied(),
                Literal::Integer(_) | Literal::Float(_) | Literal::None => None,
            },
            Expression::Assign {
                ..
            } |
            Expression::AssignField {
                ..
            } |
            Expression::ResultPropagation {
                ..
            } => None,
        }
    }

    pub(super) fn known_value_type(&self, value: &Type) -> Option<XlilType>
    {
        match value
        {
            Type::Primitive(primitive) => primitive_to_xlil(*primitive),
            Type::Named(name) => self.aggregate_types.get(name).copied(),
            Type::Tuple {
                ..
            } => self
                .tuple_types
                .iter()
                .find(|(source, _)| source == value)
                .map(|(_, ty)| *ty),
            Type::Array {
                ..
            } => self
                .array_types
                .iter()
                .find(|(source, _)| source == value)
                .map(|(_, ty)| *ty),
            Type::Reference {
                referent,
                mutable: false,
            } if **referent == Type::Primitive(PrimitiveType::Str) => Some(XlilType::STR),
            Type::Optional {
                ..
            } => self
                .optional_types
                .iter()
                .find(|(source, _)| source == value)
                .map(|(_, ty)| *ty),
            Type::Result {
                ..
            } => self
                .result_types
                .iter()
                .find(|(source, _)| source == value)
                .map(|(_, ty)| *ty),
            Type::Unit |
            Type::Set {
                ..
            } |
            Type::Map {
                ..
            } |
            Type::Reference {
                ..
            } => None,
        }
    }
}
