/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::*;

impl TypeChecker
{
  pub(super) fn binary_expression_type(&self,
                                       operator: BinaryOperator,
                                       left: &Expression,
                                       right: &Expression)
                                       -> Option<Type>
  {
    if operator == BinaryOperator::Coalesce
    {
      let right_type = self.expression_type(right)?;
      if matches!(left, Expression::Literal { literal: Literal::None, .. })
      {
        return Some(right_type);
      }
      let Type::Optional { element } = self.expression_type(left)?
      else
      {
        return None;
      };
      return self.expression_matches_type(right, &element).then_some(*element);
    }
    let mut left_type = self.expression_type(left)?;
    let mut right_type = self.expression_type(right)?;
    if left_type != right_type
    {
      if let Expression::Literal { literal, .. } = left &&
         literal_matches_type(literal, &right_type)
      {
        left_type = right_type.clone();
      }
      else if let Expression::Literal { literal, .. } = right &&
                literal_matches_type(literal, &left_type)
      {
        right_type = left_type.clone();
      }
    }
    if left_type != right_type
    {
      return None;
    }
    if matches!(operator, BinaryOperator::Equal | BinaryOperator::NotEqual) &&
       matches!(&left_type,
                Type::Reference { referent,
                                  mutable: false }
                  if **referent == Type::Primitive(PrimitiveType::Str))
    {
      return Some(Type::Primitive(PrimitiveType::Bool));
    }
    if let Type::Named(name) = &left_type &&
       matches!(operator, BinaryOperator::Equal | BinaryOperator::NotEqual) &&
       self.nominal_types
           .get(name)
           .is_some_and(|definition| definition.kind == crate::hir::declarations::NominalKind::Enum)
    {
      return Some(Type::Primitive(PrimitiveType::Bool));
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
      operator if is_integer_value_operator(operator) && is_supported_integer(primitive) =>
      {
        Some(Type::Primitive(primitive))
      }
      operator if is_float_value_operator(operator) && is_supported_float(primitive) =>
      {
        Some(Type::Primitive(primitive))
      }
      BinaryOperator::Equal | BinaryOperator::NotEqual
        if is_supported_integer(primitive) || is_supported_float(primitive) || primitive == PrimitiveType::Str =>
      {
        Some(Type::Primitive(PrimitiveType::Bool))
      }
      BinaryOperator::Less | BinaryOperator::LessEqual | BinaryOperator::Greater | BinaryOperator::GreaterEqual
        if is_supported_integer(primitive) || is_supported_float(primitive) =>
      {
        Some(Type::Primitive(PrimitiveType::Bool))
      }
      _ => None,
    }
  }

  pub(super) fn binary_expression_matches_type(&self,
                                               operator: BinaryOperator,
                                               left: &Expression,
                                               right: &Expression,
                                               expected: &Type)
                                               -> bool
  {
    if operator == BinaryOperator::Coalesce
    {
      let optional = Type::Optional { element: Box::new(expected.clone()) };
      return self.expression_matches_type(left, &optional) && self.expression_matches_type(right, expected);
    }
    if is_integer_value_operator(operator) &&
       matches!(expected, Type::Primitive(primitive) if is_supported_integer(*primitive))
    {
      return self.expression_matches_type(left, expected) && self.expression_matches_type(right, expected);
    }
    if is_float_value_operator(operator) &&
       matches!(expected, Type::Primitive(primitive) if is_supported_float(*primitive))
    {
      return self.expression_matches_type(left, expected) && self.expression_matches_type(right, expected);
    }
    self.binary_expression_type(operator, left, right).as_ref() == Some(expected)
  }

  fn expression_matches_type(&self, expression: &Expression, expected: &Type) -> bool
  {
    match expression
    {
      Expression::Literal { literal, .. } => literal_matches_type(literal, expected),
      Expression::Binary { operator,
                           left,
                           right,
                           .. } => self.binary_expression_matches_type(*operator, left, right, expected),
      Expression::Unary { operator,
                          operand,
                          .. } => self.unary_expression_matches_type(*operator, operand, expected),
      _ => self.expression_type(expression).as_ref() == Some(expected),
    }
  }
}

pub(super) const fn is_supported_integer(primitive: PrimitiveType) -> bool
{
  matches!(primitive,
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

const fn is_supported_float(primitive: PrimitiveType) -> bool
{
  matches!(primitive,
           PrimitiveType::SFloat | PrimitiveType::LFloat | PrimitiveType::Float | PrimitiveType::Double)
}

const fn is_float_value_operator(operator: BinaryOperator) -> bool
{
  matches!(operator,
           BinaryOperator::Add | BinaryOperator::Sub | BinaryOperator::Mul | BinaryOperator::Div | BinaryOperator::Rem)
}

const fn is_integer_value_operator(operator: BinaryOperator) -> bool
{
  matches!(operator,
           BinaryOperator::Add |
           BinaryOperator::Sub |
           BinaryOperator::Mul |
           BinaryOperator::Div |
           BinaryOperator::Rem |
           BinaryOperator::BitAnd |
           BinaryOperator::BitOr |
           BinaryOperator::BitXor |
           BinaryOperator::ShiftLeft |
           BinaryOperator::ShiftRight)
}
