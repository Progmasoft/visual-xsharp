/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

/// Kind tag for an exact XLIL value type.
#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
pub enum TypeKind
{
  /// No runtime value.
  Void,
  /// Verified logical value.
  Bool,
  /// Unsigned 8-bit integer.
  U8,
  /// Signed 8-bit integer.
  I8,
  /// Unsigned 16-bit integer.
  U16,
  /// Signed 16-bit integer.
  I16,
  /// Unsigned 32-bit integer.
  U32,
  /// Signed 32-bit integer.
  I32,
  /// Unsigned 64-bit integer.
  U64,
  /// Signed 64-bit integer.
  I64,
  /// Unsigned 128-bit integer.
  U128,
  /// Signed 128-bit integer.
  I128,
  /// IEEE binary16 value.
  F16,
  /// IEEE binary32 value.
  F32,
  /// IEEE binary64 value.
  F64,
  /// IEEE binary128 value.
  F128,
  /// Borrowed UTF-32 string view.
  Str,
  /// Owned UTF-32 string.
  String,
  /// Module-registry aggregate layout.
  Aggregate,
  /// Module-registry array layout.
  Array,
}

/// Exact XLIL type descriptor.
#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
pub struct Type
{
  /// Primitive or registry type kind.
  pub kind: TypeKind,
  /// Aggregate/array registry id; zero for primitives.
  pub registry_id: u32,
}

impl Type
{
  const fn primitive(kind: TypeKind) -> Self
  {
    Self { kind,
           registry_id: 0 }
  }

  /// The XLIL `void` type.
  pub const VOID: Self = Self::primitive(TypeKind::Void);
  /// The XLIL verified boolean type.
  pub const BOOL: Self = Self::primitive(TypeKind::Bool);
  /// The XLIL unsigned 8-bit integer type.
  pub const U8: Self = Self::primitive(TypeKind::U8);
  /// The XLIL signed 8-bit integer type.
  pub const I8: Self = Self::primitive(TypeKind::I8);
  /// The XLIL unsigned 16-bit integer type.
  pub const U16: Self = Self::primitive(TypeKind::U16);
  /// The XLIL signed 16-bit integer type.
  pub const I16: Self = Self::primitive(TypeKind::I16);
  /// The XLIL unsigned 32-bit integer type.
  pub const U32: Self = Self::primitive(TypeKind::U32);
  /// The XLIL signed 32-bit integer type.
  pub const I32: Self = Self::primitive(TypeKind::I32);
  /// The XLIL unsigned 64-bit integer type.
  pub const U64: Self = Self::primitive(TypeKind::U64);
  /// The XLIL signed 64-bit integer type.
  pub const I64: Self = Self::primitive(TypeKind::I64);
  /// The XLIL unsigned 128-bit integer type.
  pub const U128: Self = Self::primitive(TypeKind::U128);
  /// The XLIL signed 128-bit integer type.
  pub const I128: Self = Self::primitive(TypeKind::I128);
  /// The XLIL IEEE binary16 type.
  pub const F16: Self = Self::primitive(TypeKind::F16);
  /// The XLIL IEEE binary32 type.
  pub const F32: Self = Self::primitive(TypeKind::F32);
  /// The XLIL IEEE binary64 type.
  pub const F64: Self = Self::primitive(TypeKind::F64);
  /// The XLIL IEEE binary128 type.
  pub const F128: Self = Self::primitive(TypeKind::F128);
  /// The XLIL borrowed UTF-32 view type.
  pub const STR: Self = Self::primitive(TypeKind::Str);
  /// The XLIL owned UTF-32 string type.
  pub const STRING: Self = Self::primitive(TypeKind::String);

  /// Creates an aggregate registry reference.
  #[must_use]
  pub const fn aggregate(registry_id: u32) -> Self
  {
    Self { kind: TypeKind::Aggregate,
           registry_id }
  }

  /// Creates an array registry reference.
  #[must_use]
  pub const fn array(registry_id: u32) -> Self
  {
    Self { kind: TypeKind::Array,
           registry_id }
  }

  /// Returns an integer width, or `None` for non-integer types.
  #[must_use]
  pub const fn integer_width(self) -> Option<u32>
  {
    match self.kind
    {
      TypeKind::U8 | TypeKind::I8 => Some(8),
      TypeKind::U16 | TypeKind::I16 => Some(16),
      TypeKind::U32 | TypeKind::I32 => Some(32),
      TypeKind::U64 | TypeKind::I64 => Some(64),
      TypeKind::U128 | TypeKind::I128 => Some(128),
      _ => None,
    }
  }

  /// Returns whether this is a fixed-width integer type.
  #[must_use]
  pub const fn is_integer(self) -> bool
  {
    self.integer_width().is_some()
  }

  /// Returns whether this is a signed integer type.
  #[must_use]
  pub const fn is_signed_integer(self) -> bool
  {
    matches!(self.kind,
             TypeKind::I8 | TypeKind::I16 | TypeKind::I32 | TypeKind::I64 | TypeKind::I128)
  }
}

/// Byte order used to serialize UTF-32 code points in target storage.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum Utf32Encoding
{
  /// Least-significant byte first.
  LittleEndian,
  /// Most-significant byte first.
  BigEndian,
}

impl Utf32Encoding
{
  /// Returns the compilation host's native byte order.
  #[must_use]
  pub const fn native() -> Self
  {
    if cfg!(target_endian = "little")
    {
      Self::LittleEndian
    }
    else
    {
      Self::BigEndian
    }
  }

  /// Returns the canonical XLIL text spelling.
  #[must_use]
  pub const fn text_name(self) -> &'static str
  {
    match self
    {
      Self::LittleEndian => "utf32le",
      Self::BigEndian => "utf32be",
    }
  }
}

impl Default for Utf32Encoding
{
  fn default() -> Self
  {
    Self::native()
  }
}
