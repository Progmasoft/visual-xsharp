/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::{Type, TypeKind};

/// Returns the canonical primitive or registry-kind name.
#[must_use]
pub const fn type_name(value_type: Type) -> &'static str
{
  match value_type.kind
  {
    TypeKind::Void => "void",
    TypeKind::Bool => "bool",
    TypeKind::U8 => "u8",
    TypeKind::I8 => "i8",
    TypeKind::U16 => "u16",
    TypeKind::I16 => "i16",
    TypeKind::U32 => "u32",
    TypeKind::I32 => "i32",
    TypeKind::U64 => "u64",
    TypeKind::I64 => "i64",
    TypeKind::U128 => "u128",
    TypeKind::I128 => "i128",
    TypeKind::F16 => "f16",
    TypeKind::F32 => "f32",
    TypeKind::F64 => "f64",
    TypeKind::F128 => "f128",
    TypeKind::Str => "str",
    TypeKind::String => "string",
    TypeKind::Aggregate => "aggregate",
    TypeKind::Array => "array",
  }
}

/// Parses a canonical primitive type name.
#[must_use]
pub fn type_from_name(name: &str) -> Option<Type>
{
  let kind = match name
  {
    "void" => TypeKind::Void,
    "bool" => TypeKind::Bool,
    "u8" => TypeKind::U8,
    "i8" => TypeKind::I8,
    "u16" => TypeKind::U16,
    "i16" => TypeKind::I16,
    "u32" => TypeKind::U32,
    "i32" => TypeKind::I32,
    "u64" => TypeKind::U64,
    "i64" => TypeKind::I64,
    "u128" => TypeKind::U128,
    "i128" => TypeKind::I128,
    "f16" => TypeKind::F16,
    "f32" => TypeKind::F32,
    "f64" => TypeKind::F64,
    "f128" => TypeKind::F128,
    "str" => TypeKind::Str,
    "string" => TypeKind::String,
    _ => return None,
  };
  Some(Type { kind,
              registry_id: 0 })
}

/// Returns canonical text for a primitive or registry-referenced type.
#[must_use]
pub fn type_text(value_type: Type) -> String
{
  if value_type.kind == TypeKind::Aggregate
  {
    format!("%t{}", value_type.registry_id)
  }
  else if value_type.kind == TypeKind::Array
  {
    format!("%a{}", value_type.registry_id)
  }
  else
  {
    type_name(value_type).to_string()
  }
}
