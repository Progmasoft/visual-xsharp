/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use crate::hir::symbols::{SymbolKind, Visibility};
use crate::hir::type_check::{FieldPath, Literal, PrimitiveType, Type};

pub(super) fn literal_name(literal: &Literal) -> String
{
  match literal
  {
    Literal::Bool(value) => format!("bool {value}"),
    Literal::Integer(value) => format!("integer {value}"),
    Literal::Float(value) => format!("float {value}"),
    Literal::Char(value) => format!("char {}", crate::text::format_character(*value)),
    Literal::String(value) => format!("string {value:?}"),
    Literal::None => "None".to_string(),
    Literal::EnumVariant { enum_type,
                           variant,
                           tag, } => format!("enum {enum_type}::{variant} tag {tag}"),
  }
}

pub(super) fn field_path_name(path: &FieldPath) -> String
{
  std::iter::once(path.root.as_str()).chain(path.fields.iter().map(String::as_str))
                                     .collect::<Vec<_>>()
                                     .join(".")
}

pub(super) const fn symbol_kind_name(kind: SymbolKind) -> &'static str
{
  match kind
  {
    SymbolKind::Function => "function",
    SymbolKind::Class => "class",
    SymbolKind::Interface => "interface",
    SymbolKind::Enum => "enum",
    SymbolKind::Data => "data",
    SymbolKind::Macro => "macro",
  }
}

pub(super) const fn visibility_name(visibility: Visibility) -> &'static str
{
  match visibility
  {
    Visibility::Public => "public",
    Visibility::Internal => "internal",
    Visibility::Private => "private",
  }
}

pub(super) fn type_name(ty: &Type) -> String
{
  match ty
  {
    Type::Unit => "()".to_string(),
    Type::Primitive(primitive) => primitive_type_name(*primitive).to_string(),
    Type::Named(name) => name.clone(),
    Type::Optional { element } => format!("Optional<{}>", type_name(element)),
    Type::Reference { referent,
                      mutable: false, } => format!("&{}", type_name(referent)),
    Type::Reference { referent,
                      mutable: true, } => format!("&mut {}", type_name(referent)),
    Type::Array { element,
                  length: None, } => format!("[{}]", type_name(element)),
    Type::Array { element,
                  length: Some(length), } => format!("[{}; {length}]", type_name(element)),
    Type::Set { element } => format!("set [{}]", type_name(element)),
    Type::Map { key,
                value, } => format!("[{}: {}]", type_name(key), type_name(value)),
    Type::Tuple { fields } => format!("({})",
                                      fields.iter()
                                            .map(|field| match &field.name
                                            {
                                              Some(name) => format!("{name}: {}", type_name(&field.ty)),
                                              None => type_name(&field.ty),
                                            })
                                            .collect::<Vec<_>>()
                                            .join(", ")),
  }
}

const fn primitive_type_name(primitive: PrimitiveType) -> &'static str
{
  match primitive
  {
    PrimitiveType::Bool => "Bool",
    PrimitiveType::Byte => "Byte",
    PrimitiveType::SByte => "SByte",
    PrimitiveType::Char => "Char",
    PrimitiveType::Short => "Short",
    PrimitiveType::Long => "Long",
    PrimitiveType::Int => "Int",
    PrimitiveType::Integer => "Integer",
    PrimitiveType::UShort => "UShort",
    PrimitiveType::ULong => "ULong",
    PrimitiveType::UInt => "UInt",
    PrimitiveType::UInteger => "UInteger",
    PrimitiveType::SFloat => "SFloat",
    PrimitiveType::LFloat => "LFloat",
    PrimitiveType::Float => "Float",
    PrimitiveType::Double => "Double",
    PrimitiveType::Str => "Str",
    PrimitiveType::String => "String",
  }
}
