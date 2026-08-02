/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use std::fmt;

use crate::xlil::Type;

mod private
{
  pub trait Sealed {}
}

/// Implemented by Rust values with one exact XLIL value representation.
///
/// The trait is sealed because XLIL type identity must remain consistent with
/// parser, writer, verifier, and backend behavior. Aggregate types use the
/// runtime module registry instead of this primitive marker surface.
pub trait XlilType: private::Sealed + Copy + 'static
{
  /// XLIL type selected for the Rust value.
  const XLIL_TYPE: Type;

  /// Canonical XLIL text spelling of the selected type.
  const NAME: &'static str;
}

/// Rust values accepted by exact-width XLIL integer operations.
pub trait IntegerType: XlilType {}

/// Rust values represented by an XLIL floating-point format.
pub trait FloatType: XlilType {}

macro_rules! primitive_alias {
  ($name:ident, $rust:ty, $xlil:ident, $text:literal, $documentation:literal) => {
    #[doc = $documentation]
    pub type $name = $rust;

    impl private::Sealed for $rust {}

    impl XlilType for $rust
    {
      const XLIL_TYPE: Type = Type::$xlil;
      const NAME: &'static str = $text;
    }
  };
}

primitive_alias!(I8, i8, I8, "i8", "Rust value represented as XLIL `i8`.");
primitive_alias!(I16, i16, I16, "i16", "Rust value represented as XLIL `i16`.");
primitive_alias!(I32, i32, I32, "i32", "Rust value represented as XLIL `i32`.");
primitive_alias!(I64, i64, I64, "i64", "Rust value represented as XLIL `i64`.");
primitive_alias!(I128, i128, I128, "i128", "Rust value represented as XLIL `i128`.");
primitive_alias!(F32, f32, F32, "f32", "Rust value represented as XLIL IEEE binary32.");
primitive_alias!(F64, f64, F64, "f64", "Rust value represented as XLIL IEEE binary64.");

/// Rust value represented as verified XLIL `bool`.
pub type Bool = bool;

impl private::Sealed for bool {}

impl XlilType for bool
{
  const XLIL_TYPE: Type = Type::BOOL;
  const NAME: &'static str = "bool";
}

impl IntegerType for i8 {}
impl IntegerType for i16 {}
impl IntegerType for i32 {}
impl IntegerType for i64 {}
impl IntegerType for i128 {}
impl FloatType for f32 {}
impl FloatType for f64 {}

macro_rules! float_bits {
  ($name:ident, $bits:ty, $xlil:ident, $text:literal, $documentation:literal) => {
    #[doc = $documentation]
    #[derive(Clone, Copy, Default, Eq, Hash, Ord, PartialEq, PartialOrd)]
    #[repr(transparent)]
    pub struct $name($bits);

    impl $name
    {
      /// All-zero bit pattern.
      pub const ZERO: Self = Self(0);

      /// Constructs the value without changing its floating-point bit pattern.
      #[must_use]
      pub const fn from_bits(bits: $bits) -> Self
      {
        Self(bits)
      }

      /// Returns the preserved floating-point bit pattern.
      #[must_use]
      pub const fn to_bits(self) -> $bits
      {
        self.0
      }
    }

    impl fmt::Debug for $name
    {
      fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result
      {
        write!(formatter, "{}({:#x})", stringify!($name), self.0)
      }
    }

    impl From<$bits> for $name
    {
      fn from(bits: $bits) -> Self
      {
        Self::from_bits(bits)
      }
    }

    impl From<$name> for $bits
    {
      fn from(value: $name) -> Self
      {
        value.to_bits()
      }
    }

    impl private::Sealed for $name {}

    impl XlilType for $name
    {
      const XLIL_TYPE: Type = Type::$xlil;
      const NAME: &'static str = $text;
    }
  };
}

float_bits!(F16,
            u16,
            F16,
            "f16",
            "Exact bit container for an XLIL IEEE binary16 value.");
float_bits!(F128,
            u128,
            F128,
            "f128",
            "Exact bit container for an XLIL IEEE binary128 value.");

impl FloatType for F16 {}
impl FloatType for F128 {}

#[cfg(test)]
mod tests
{
  use super::*;

  #[test]
  fn native_aliases_select_exact_types_and_names()
  {
    assert_eq!(<I8 as XlilType>::XLIL_TYPE, Type::I8);
    assert_eq!(<I16 as XlilType>::XLIL_TYPE, Type::I16);
    assert_eq!(<I32 as XlilType>::XLIL_TYPE, Type::I32);
    assert_eq!(<I64 as XlilType>::XLIL_TYPE, Type::I64);
    assert_eq!(<I128 as XlilType>::XLIL_TYPE, Type::I128);
    assert_eq!(<F32 as XlilType>::XLIL_TYPE, Type::F32);
    assert_eq!(<F64 as XlilType>::XLIL_TYPE, Type::F64);
    assert_eq!(<I128 as XlilType>::NAME, "i128");
  }

  #[test]
  fn non_native_float_containers_preserve_every_bit()
  {
    let half = F16::from_bits(0x7e01);
    let quad = F128::from_bits(0x7fff_8000_0000_0000_0000_0000_0000_0001);
    assert_eq!(half.to_bits(), 0x7e01);
    assert_eq!(quad.to_bits(), 0x7fff_8000_0000_0000_0000_0000_0000_0001);
    assert_eq!(u16::from(half), 0x7e01);
    assert_eq!(u128::from(quad), quad.to_bits());
    assert_eq!(<F16 as XlilType>::XLIL_TYPE, Type::F16);
    assert_eq!(<F128 as XlilType>::XLIL_TYPE, Type::F128);
  }
}
