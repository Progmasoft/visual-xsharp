/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use std::{error::Error, fmt, marker::PhantomData};

use crate::xlil::{Function, Type, ValueId};

use super::XlilType;

/// A register identifier paired with its verified runtime XLIL type.
#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
pub struct AnyValue
{
  id: ValueId,
  value_type: Type,
}

impl AnyValue
{
  /// Creates an erased typed value.
  #[must_use]
  pub const fn new(id: ValueId, value_type: Type) -> Self
  {
    Self { id,
           value_type }
  }

  /// Returns the underlying XLIL register identifier.
  #[must_use]
  pub const fn id(self) -> ValueId
  {
    self.id
  }

  /// Returns the preserved XLIL value type.
  #[must_use]
  pub const fn value_type(self) -> Type
  {
    self.value_type
  }

  /// Checks and restores a compile-time Rust XLIL type marker.
  pub fn downcast<T: XlilType>(self) -> Result<Value<T>, TypeMismatch>
  {
    Value::from_parts(self.id, self.value_type)
  }
}

/// A register identifier carrying a compile-time XLIL primitive type.
#[derive(Eq, Hash, PartialEq)]
pub struct Value<T: XlilType>
{
  id: ValueId,
  marker: PhantomData<fn() -> T>,
}

impl<T: XlilType> Value<T>
{
  pub(crate) const fn trusted(id: ValueId) -> Self
  {
    Self { id,
           marker: PhantomData }
  }

  /// Validates a raw register/type pair before constructing a typed value.
  pub fn from_parts(id: ValueId, actual: Type) -> Result<Self, TypeMismatch>
  {
    if actual == T::XLIL_TYPE
    {
      Ok(Self::trusted(id))
    }
    else
    {
      Err(TypeMismatch { register: id,
                         expected: T::XLIL_TYPE,
                         actual: Some(actual) })
    }
  }

  /// Looks up a register in a function and validates its type.
  pub fn from_function(function: &Function, id: ValueId) -> Result<Self, TypeMismatch>
  {
    let Some(actual) = function.value(id).map(|value| value.value_type)
    else
    {
      return Err(TypeMismatch { register: id,
                                expected: T::XLIL_TYPE,
                                actual: None });
    };
    Self::from_parts(id, actual)
  }

  /// Returns the underlying XLIL register identifier.
  #[must_use]
  pub const fn id(self) -> ValueId
  {
    self.id
  }

  /// Erases the compile-time Rust marker while preserving runtime type data.
  #[must_use]
  pub const fn erase(self) -> AnyValue
  {
    AnyValue::new(self.id, T::XLIL_TYPE)
  }
}

impl<T: XlilType> Clone for Value<T>
{
  fn clone(&self) -> Self
  {
    *self
  }
}

impl<T: XlilType> Copy for Value<T> {}

impl<T: XlilType> fmt::Debug for Value<T>
{
  fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result
  {
    formatter.debug_struct("Value")
             .field("id", &self.id)
             .field("type", &T::NAME)
             .finish()
  }
}

/// Error returned when a raw XLIL register does not match a Rust type marker.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct TypeMismatch
{
  /// Register whose type was checked.
  pub register: ValueId,
  /// Type requested by the Rust caller.
  pub expected: Type,
  /// Type found in the XLIL model.
  pub actual: Option<Type>,
}

impl fmt::Display for TypeMismatch
{
  fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result
  {
    match self.actual
    {
      Some(actual) => write!(formatter,
                             "XLIL register %r{} has type {:?}, expected {:?}",
                             self.register.0, actual, self.expected),
      None => write!(formatter,
                     "XLIL register %r{} does not exist; expected {:?}",
                     self.register.0, self.expected),
    }
  }
}

impl Error for TypeMismatch {}

#[cfg(test)]
mod tests
{
  use crate::xlil::{Builder, Type};

  use super::*;

  #[test]
  fn erased_values_downcast_only_to_the_matching_marker()
  {
    let erased = AnyValue::new(ValueId(7), Type::I32);
    assert_eq!(erased.downcast::<i32>().unwrap().id(), ValueId(7));
    let mismatch = erased.downcast::<i64>().unwrap_err();
    assert_eq!(mismatch.register, ValueId(7));
    assert_eq!(mismatch.expected, Type::I64);
    assert_eq!(mismatch.actual, Some(Type::I32));
  }

  #[test]
  fn function_lookup_checks_register_existence_and_type()
  {
    let mut builder = Builder::new("TypedLookup");
    builder.begin_function("main", Type::I32, vec![]).unwrap();
    builder.append_block("entry").unwrap();
    let register = builder.const_i32(7).unwrap();
    builder.return_value(Some(register)).unwrap();
    let module = builder.finish().unwrap();
    let function = module.function("main").unwrap();

    assert_eq!(Value::<i32>::from_function(function, register).unwrap().id(), register);
    assert!(Value::<i64>::from_function(function, register).is_err());
    assert!(Value::<i32>::from_function(function, ValueId(99)).is_err());
  }
}
