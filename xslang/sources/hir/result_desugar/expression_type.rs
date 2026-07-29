/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use crate::hir::type_check::{Literal, PrimitiveType, Type, UnaryOperator};

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
