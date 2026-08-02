/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

//! Rust-side values that explicitly select XLIL value types.
//!
//! Integer and native floating aliases retain ordinary Rust arithmetic. The
//! non-native floating formats are transparent bit containers so producers do
//! not silently round through a host format.

use super::{BuildError, Builder, Type, Utf32Encoding, ValueId};

/// Implemented by Rust values with one exact XLIL value representation.
pub trait XlilType
{
  /// XLIL type selected for the Rust value.
  const XLIL_TYPE: Type;
}

macro_rules! primitive_alias {
  ($name:ident, $rust:ty, $xlil:ident, $documentation:literal) => {
    #[doc = $documentation]
    pub type $name = $rust;

    impl XlilType for $rust
    {
      const XLIL_TYPE: Type = Type::$xlil;
    }
  };
}

primitive_alias!(I8, i8, I8, "Rust value represented as XLIL `i8`.");
primitive_alias!(I16, i16, I16, "Rust value represented as XLIL `i16`.");
primitive_alias!(I32, i32, I32, "Rust value represented as XLIL `i32`.");
primitive_alias!(I64, i64, I64, "Rust value represented as XLIL `i64`.");
primitive_alias!(I128, i128, I128, "Rust value represented as XLIL `i128`.");
primitive_alias!(F32, f32, F32, "Rust value represented as XLIL IEEE binary32.");
primitive_alias!(F64, f64, F64, "Rust value represented as XLIL IEEE binary64.");

macro_rules! float_bits {
  ($name:ident, $bits:ty, $xlil:ident, $documentation:literal) => {
    #[doc = $documentation]
    #[derive(Clone, Copy, Debug, Default, Eq, Hash, PartialEq)]
    #[repr(transparent)]
    pub struct $name($bits);

    impl $name
    {
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

    impl XlilType for $name
    {
      const XLIL_TYPE: Type = Type::$xlil;
    }
  };
}

float_bits!(F16, u16, F16, "Exact bit container for an XLIL IEEE binary16 value.");
float_bits!(F128,
            u128,
            F128,
            "Exact bit container for an XLIL IEEE binary128 value.");

/// Converts Rust text into target-endian UTF-32 code points for an XLIL
/// string constant.
///
/// The Rust source text is not retained in the XLIL model. Canonical XLIL
/// output contains only the selected encoding and numeric code points.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct Utf32Builder
{
  encoding: Utf32Encoding,
  code_points: Vec<u32>,
}

impl Utf32Builder
{
  /// Converts text using the compilation host's native byte order.
  #[must_use]
  pub fn new(text: impl AsRef<str>) -> Self
  {
    Self::with_encoding(text, Utf32Encoding::native())
  }

  /// Converts text using an explicitly selected target byte order.
  #[must_use]
  pub fn with_encoding(text: impl AsRef<str>, encoding: Utf32Encoding) -> Self
  {
    Self { encoding,
           code_points: text.as_ref().chars().map(u32::from).collect() }
  }

  /// Returns the target storage byte order.
  #[must_use]
  pub const fn encoding(&self) -> Utf32Encoding
  {
    self.encoding
  }

  /// Returns the converted Unicode scalar values.
  #[must_use]
  pub fn code_points(&self) -> &[u32]
  {
    &self.code_points
  }

  /// Returns the number of Unicode scalar values.
  #[must_use]
  pub const fn len(&self) -> usize
  {
    self.code_points.len()
  }

  /// Returns whether no Unicode scalar values were supplied.
  #[must_use]
  pub const fn is_empty(&self) -> bool
  {
    self.code_points.is_empty()
  }

  /// Appends the converted value to the current XLIL builder block.
  pub fn emit(self, builder: &mut Builder) -> Result<ValueId, BuildError>
  {
    builder.const_str(self.encoding, self.code_points)
  }
}

impl From<&str> for Utf32Builder
{
  fn from(value: &str) -> Self
  {
    Self::new(value)
  }
}

#[cfg(test)]
mod tests
{
  use super::*;
  use crate::xlil::module_to_string;

  #[test]
  fn explicit_types_select_exact_xlil_descriptors()
  {
    assert_eq!(<I8 as XlilType>::XLIL_TYPE, Type::I8);
    assert_eq!(<I16 as XlilType>::XLIL_TYPE, Type::I16);
    assert_eq!(<I32 as XlilType>::XLIL_TYPE, Type::I32);
    assert_eq!(<I64 as XlilType>::XLIL_TYPE, Type::I64);
    assert_eq!(<I128 as XlilType>::XLIL_TYPE, Type::I128);
    assert_eq!(<F16 as XlilType>::XLIL_TYPE, Type::F16);
    assert_eq!(<F32 as XlilType>::XLIL_TYPE, Type::F32);
    assert_eq!(<F64 as XlilType>::XLIL_TYPE, Type::F64);
    assert_eq!(<F128 as XlilType>::XLIL_TYPE, Type::F128);
  }

  #[test]
  fn utf32_builder_emits_code_points_without_source_text()
  {
    let mut builder = Builder::new("Utf32");
    builder.begin_function("text", Type::STR, vec![]).unwrap();
    builder.append_block("entry").unwrap();
    let value = Utf32Builder::with_encoding("A🐺", Utf32Encoding::LittleEndian).emit(&mut builder)
                                                                               .unwrap();
    builder.return_value(Some(value)).unwrap();
    let text = module_to_string(&builder.finish().unwrap());
    assert!(text.contains("utf32le [0x00000041, 0x0001f43a]"));
    assert!(!text.contains("A🐺"));
  }
}
