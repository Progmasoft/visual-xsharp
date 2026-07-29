/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

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
    Literal::EnumVariant { enum_type, .. } => return Some(Type::Named(enum_type.clone())),
  };
  Some(Type::Primitive(primitive))
}

impl super::ResultDesugar
{
  pub(super) fn binary_expression_type(&mut self,
                                       operator: BinaryOperator,
                                       left: &Expression,
                                       right: &Expression)
                                       -> Option<Type>
  {
    if operator == BinaryOperator::Coalesce
    {
      let right_type = self.expression_type(right)?;
      if matches!(left, Expression::Literal { literal: Literal::None,
                                              .. })
      {
        return Some(right_type);
      }
      let Type::Optional { element } = self.expression_type(left)?
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
      BinaryOperator::Less | BinaryOperator::LessEqual | BinaryOperator::Greater | BinaryOperator::GreaterEqual
        if primitive == PrimitiveType::Long =>
      {
        Some(Type::Primitive(PrimitiveType::Bool))
      }
      _ => None,
    }
  }
}
