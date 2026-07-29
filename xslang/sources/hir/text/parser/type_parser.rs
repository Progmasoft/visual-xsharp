/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::{Local, Type, span};
use crate::hir::type_check::PrimitiveType;

pub(super) fn primitive_type(name: &str) -> Option<PrimitiveType>
{
  Some(match name
  {
    "Bool" => PrimitiveType::Bool,
    "Byte" => PrimitiveType::Byte,
    "SByte" => PrimitiveType::SByte,
    "Char" => PrimitiveType::Char,
    "Short" => PrimitiveType::Short,
    "Long" => PrimitiveType::Long,
    "Int" => PrimitiveType::Int,
    "Integer" => PrimitiveType::Integer,
    "UShort" => PrimitiveType::UShort,
    "ULong" => PrimitiveType::ULong,
    "UInt" => PrimitiveType::UInt,
    "UInteger" => PrimitiveType::UInteger,
    "SFloat" => PrimitiveType::SFloat,
    "LFloat" => PrimitiveType::LFloat,
    "Float" => PrimitiveType::Float,
    "Double" => PrimitiveType::Double,
    "Str" => PrimitiveType::Str,
    "String" => PrimitiveType::String,
    _ => return None,
  })
}

pub(super) fn is_named_type(name: &str) -> bool
{
  let name = name.trim();
  if !name.starts_with(|value: char| value == '_' || value.is_alphabetic()) ||
     name.ends_with([',', '<', ':', '.']) ||
     ["<>", "<,", ",>", ",,"].iter().any(|pattern| name.contains(pattern))
  {
    return false;
  }
  let mut generic_depth = 0_u32;
  let mut saw_identifier = false;
  for character in name.chars()
  {
    match character
    {
      '<' => generic_depth += 1,
      '>' if generic_depth > 0 => generic_depth -= 1,
      '>' | ',' if generic_depth == 0 => return false,
      '_' | '.' | ':' | ',' | ' ' | '(' | ')' =>
      {}
      value if value.is_alphanumeric() => saw_identifier = true,
      _ => return false,
    }
  }
  saw_identifier && generic_depth == 0
}

pub(super) fn split_type_list(text: &str) -> Vec<&str>
{
  let mut result = Vec::new();
  let mut depth = 0_u32;
  let mut start = 0;
  for (index, character) in text.char_indices()
  {
    match character
    {
      '<' | '(' | '[' => depth += 1,
      '>' | ')' | ']' => depth = depth.saturating_sub(1),
      ',' if depth == 0 =>
      {
        result.push(&text[start..index]);
        start = index + character.len_utf8();
      }
      _ =>
      {}
    }
  }
  result.push(&text[start..]);
  result
}

pub(crate) fn parse_type_text(name: &str) -> Option<Type>
{
  if let Some(referent) = name.strip_prefix("&mut ")
  {
    return Some(Type::Reference { referent: Box::new(parse_type_text(referent.trim())?),
                                  mutable: true });
  }
  if let Some(referent) = name.strip_prefix('&')
  {
    return Some(Type::Reference { referent: Box::new(parse_type_text(referent.trim())?),
                                  mutable: false });
  }
  if name == "()"
  {
    return Some(Type::Unit);
  }
  if let Some(primitive) = primitive_type(name)
  {
    return Some(Type::Primitive(primitive));
  }
  if let Some(element) = name.strip_prefix("Optional<").and_then(|value| value.strip_suffix('>'))
  {
    return Some(Type::Optional { element: Box::new(parse_type_text(element.trim())?) });
  }
  if let Some(body) = name.strip_prefix("Result<").and_then(|value| value.strip_suffix('>'))
  {
    let parts = split_type_list(body);
    if let [success, error] = parts.as_slice()
    {
      return Some(Type::Result { success: Box::new(parse_type_text(success.trim())?),
                                 error: Box::new(parse_type_text(error.trim())?) });
    }
    if parts.as_slice() == ["()"]
    {
      return Some(Type::Result { success: Box::new(Type::Unit),
                                 error: Box::new(Type::Named("Error".to_string())) });
    }
    return None;
  }
  if let Some(body) = name.strip_prefix('(').and_then(|value| value.strip_suffix(')'))
  {
    let fields = if body.trim().is_empty()
    {
      Vec::new()
    }
    else
    {
      split_type_list(body).into_iter()
                           .map(|field| {
                             let field = field.trim();
                             let (name, ty) = split_tuple_field(field).map_or((None, field), |(name, ty)| {
                                                                        (Some(name.trim().to_string()), ty.trim())
                                                                      });
                             Some(crate::hir::type_check::TupleFieldType { name,
                                                                           ty: parse_type_text(ty)? })
                           })
                           .collect::<Option<Vec<_>>>()?
    };
    return Some(Type::Tuple { fields });
  }
  if let Some(body) = name.strip_prefix("set [").and_then(|value| value.strip_suffix(']'))
  {
    return Some(Type::Set { element: Box::new(parse_type_text(body.trim())?) });
  }
  if let Some(body) = name.strip_prefix('[').and_then(|value| value.strip_suffix(']'))
  {
    if let Some((key, value)) = split_collection_type(body, ':')
    {
      return Some(Type::Map { key: Box::new(parse_type_text(key.trim())?),
                              value: Box::new(parse_type_text(value.trim())?) });
    }
    if let Some((element, length)) = split_collection_type(body, ';')
    {
      return Some(Type::Array { element: Box::new(parse_type_text(element.trim())?),
                                length: Some(length.trim().parse().ok()?) });
    }
    return Some(Type::Array { element: Box::new(parse_type_text(body.trim())?),
                              length: None });
  }
  is_named_type(name).then(|| Type::Named(name.to_string()))
}

fn split_tuple_field(value: &str) -> Option<(&str, &str)>
{
  let mut depth = 0_u32;
  for (index, character) in value.char_indices()
  {
    match character
    {
      '<' | '(' | '[' => depth += 1,
      '>' | ')' | ']' => depth = depth.saturating_sub(1),
      ':' if depth == 0 => return Some((&value[..index], &value[index + 1..])),
      _ =>
      {}
    }
  }
  None
}

pub(super) fn parse_local_record(text: &str) -> Option<Local>
{
  let (name, rest) = text.split_once(": ")?;
  let (ty, mutability) = rest.rsplit_once(' ')?;
  Some(Local { name: name.to_string(),
               ty: parse_type_text(ty)?,
               mutable: mutability == "mutable",
               span: span() })
}

fn split_collection_type(value: &str, separator: char) -> Option<(&str, &str)>
{
  let mut angle_depth = 0_u32;
  let mut bracket_depth = 0_u32;
  for (index, character) in value.char_indices()
  {
    match character
    {
      '<' => angle_depth += 1,
      '>' => angle_depth = angle_depth.saturating_sub(1),
      '[' => bracket_depth += 1,
      ']' => bracket_depth = bracket_depth.saturating_sub(1),
      current if current == separator && angle_depth == 0 && bracket_depth == 0 =>
      {
        return Some((&value[..index], &value[index + current.len_utf8()..]));
      }
      _ =>
      {}
    }
  }
  None
}
