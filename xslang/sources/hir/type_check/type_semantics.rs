/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::{Literal, PrimitiveType, Type};

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum ValueOwnership
{
  Value,
  BorrowedStatic,
  BoxedOwned,
}

impl Type
{
  #[must_use]
  pub fn optional_element(&self) -> Option<&Type>
  {
    match self
    {
      Self::Optional { element } => Some(element),
      _ => None,
    }
  }

  #[must_use]
  pub fn is_optional(&self) -> bool
  {
    self.optional_element().is_some()
  }

  #[must_use]
  pub fn result_parts(&self) -> Option<(&Type, &Type)>
  {
    match self
    {
      Self::Result { success,
                     error, } => Some((success, error)),
      _ => None,
    }
  }

  #[must_use]
  pub fn is_result(&self) -> bool
  {
    self.result_parts().is_some()
  }

  #[must_use]
  pub fn ownership(&self) -> ValueOwnership
  {
    match self
    {
      Self::Reference { .. } => ValueOwnership::BorrowedStatic,
      Self::Primitive(PrimitiveType::String) | Self::Optional { .. } | Self::Result { .. } =>
      {
        ValueOwnership::BoxedOwned
      }
      _ => ValueOwnership::Value,
    }
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
    Literal::String(_) =>
    {
      return Some(Type::Reference { referent: Box::new(Type::Primitive(PrimitiveType::Str)),
                                    mutable: false });
    }
    Literal::None => return None,
    Literal::EnumVariant { enum_type, .. } => return Some(Type::Named(enum_type.clone())),
  };
  Some(Type::Primitive(primitive))
}

#[must_use]
pub fn literal_matches_type(literal: &Literal, ty: &Type) -> bool
{
  if let Type::Optional { element } = ty
  {
    return matches!(literal, Literal::None) || literal_matches_type(literal, element);
  }
  if let Literal::EnumVariant { enum_type, .. } = literal
  {
    return ty == &Type::Named(enum_type.clone());
  }
  if matches!((literal, ty),
              (Literal::String(_),
               Type::Reference { referent, mutable: false }
               ) if **referent == Type::Primitive(PrimitiveType::Str))
  {
    return true;
  }
  let Type::Primitive(primitive) = ty
  else
  {
    return true;
  };
  match literal
  {
    Literal::None => true,
    Literal::Bool(_) => *primitive == PrimitiveType::Bool,
    Literal::Integer(_) => matches!(primitive,
                                    PrimitiveType::Bool |
                                    PrimitiveType::Byte |
                                    PrimitiveType::SByte |
                                    PrimitiveType::Short |
                                    PrimitiveType::Long |
                                    PrimitiveType::Int |
                                    PrimitiveType::Integer |
                                    PrimitiveType::UShort |
                                    PrimitiveType::ULong |
                                    PrimitiveType::UInt |
                                    PrimitiveType::UInteger),
    Literal::Float(_) => matches!(primitive,
                                  PrimitiveType::SFloat |
                                  PrimitiveType::LFloat |
                                  PrimitiveType::Float |
                                  PrimitiveType::Double),
    Literal::Char(_) => *primitive == PrimitiveType::Char,
    Literal::String(_) => false,
    Literal::EnumVariant { .. } => false,
  }
}

#[cfg(test)]
mod tests
{
  use super::*;
  use crate::hir::type_check::{Literal, literal_matches_type};

  #[test]
  fn distinguishes_borrowed_str_from_owned_string_and_optional()
  {
    let borrowed = Type::Reference { referent: Box::new(Type::Primitive(PrimitiveType::Str)),
                                     mutable: false };
    let optional = Type::Optional { element: Box::new(borrowed.clone()) };
    assert_eq!(borrowed.ownership(),
               ValueOwnership::BorrowedStatic);
    assert_eq!(Type::Primitive(PrimitiveType::String).ownership(), ValueOwnership::BoxedOwned);
    assert_eq!(optional.ownership(), ValueOwnership::BoxedOwned);
    assert_eq!(Type::Primitive(PrimitiveType::Int).ownership(), ValueOwnership::Value);
  }

  #[test]
  fn optional_values_accept_nil_or_implicit_some()
  {
    let optional = Type::Optional { element: Box::new(Type::Primitive(PrimitiveType::Int)) };
    assert!(literal_matches_type(&Literal::None, &optional));
    assert!(literal_matches_type(&Literal::Integer("26".to_string()), &optional));
  }

  #[test]
  fn integer_literals_use_int_by_default_but_accept_explicit_bool_context()
  {
    let literal = Literal::Integer("2".to_string());
    assert_eq!(literal_default_type(&literal),
               Some(Type::Primitive(PrimitiveType::Int)));
    assert!(literal_matches_type(&literal, &Type::Primitive(PrimitiveType::Bool)));
  }
}
