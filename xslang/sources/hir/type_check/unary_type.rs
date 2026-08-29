/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

use super::*;

impl TypeChecker
{
  pub(super) fn unary_expression_type(&self, operator: UnaryOperator, operand: &Expression) -> Option<Type>
  {
    let Type::Primitive(primitive) = self.expression_type(operand)?
    else
    {
      return None;
    };
    match operator
    {
      UnaryOperator::Positive if is_integer(primitive) => Some(Type::Primitive(primitive)),
      UnaryOperator::Negative if is_signed_integer(primitive) => Some(Type::Primitive(primitive)),
      UnaryOperator::LogicalNot if primitive == PrimitiveType::Bool => Some(Type::Primitive(PrimitiveType::Bool)),
      _ => None,
    }
  }

  pub(super) fn unary_expression_matches_type(&self,
                                              operator: UnaryOperator,
                                              operand: &Expression,
                                              expected: &Type)
                                              -> bool
  {
    match (operator, expected)
    {
      (UnaryOperator::Positive, Type::Primitive(primitive)) if is_integer(*primitive) => match operand
      {
        Expression::Literal { literal, .. } => literal_matches_type(literal, expected),
        Expression::Unary { operator,
                            operand,
                            .. } => self.unary_expression_matches_type(*operator, operand, expected),
        Expression::Binary { operator,
                             left,
                             right,
                             .. } => self.binary_expression_matches_type(*operator, left, right, expected),
        _ => self.expression_type(operand).as_ref() == Some(expected),
      },
      (UnaryOperator::Negative, Type::Primitive(primitive)) if is_signed_integer(*primitive) => match operand
      {
        Expression::Literal { literal, .. } => literal_matches_type(literal, expected),
        Expression::Unary { operator,
                            operand,
                            .. } => self.unary_expression_matches_type(*operator, operand, expected),
        Expression::Binary { operator,
                             left,
                             right,
                             .. } => self.binary_expression_matches_type(*operator, left, right, expected),
        _ => self.expression_type(operand).as_ref() == Some(expected),
      },
      (UnaryOperator::LogicalNot, Type::Primitive(PrimitiveType::Bool)) =>
      {
        self.expression_type(operand).as_ref() == Some(expected)
      }
      _ => false,
    }
  }
}

fn is_integer(value: PrimitiveType) -> bool
{
  matches!(value,
           PrimitiveType::Byte |
           PrimitiveType::SByte |
           PrimitiveType::Short |
           PrimitiveType::Long |
           PrimitiveType::Int |
           PrimitiveType::Integer |
           PrimitiveType::UShort |
           PrimitiveType::ULong |
           PrimitiveType::UInt |
           PrimitiveType::UInteger)
}

fn is_signed_integer(value: PrimitiveType) -> bool
{
  matches!(value,
           PrimitiveType::SByte |
           PrimitiveType::Short |
           PrimitiveType::Long |
           PrimitiveType::Int |
           PrimitiveType::Integer)
}
