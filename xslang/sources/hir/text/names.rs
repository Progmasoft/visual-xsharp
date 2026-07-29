/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use crate::hir::MatchPattern;
use crate::hir::symbols::{SymbolKind, Visibility};
use crate::hir::type_check::{
  BinaryOperator, FieldPath, Literal, PrimitiveType, Type, UnaryOperator, UpdateOperator, UpdatePosition,
};

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

pub(super) fn match_pattern_name(pattern: &MatchPattern) -> String
{
  match pattern
  {
    MatchPattern::Literal(literal) => format!("literal {}", literal_name(literal)),
    MatchPattern::ResultVariant { success,
                                  binding,
                                  payload_type, } =>
    {
      format!("result {} binding {} : {}",
              if *success
              {
                "Ok"
              }
              else
              {
                "Error"
              },
              binding.as_deref().unwrap_or("else"),
              type_name(payload_type))
    }
    MatchPattern::Else => "else".to_string(),
  }
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
    Type::Result { success,
                   error, } => format!("Result<{}, {}>", type_name(success), type_name(error)),
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

pub(super) const fn binary_operator_name(operator: BinaryOperator) -> &'static str
{
  match operator
  {
    BinaryOperator::Add => "add",
    BinaryOperator::Sub => "sub",
    BinaryOperator::Mul => "mul",
    BinaryOperator::Div => "div",
    BinaryOperator::Rem => "rem",
    BinaryOperator::BitAnd => "bit_and",
    BinaryOperator::BitOr => "bit_or",
    BinaryOperator::BitXor => "bit_xor",
    BinaryOperator::LogicalAnd => "logical_and",
    BinaryOperator::LogicalOr => "logical_or",
    BinaryOperator::Coalesce => "coalesce",
    BinaryOperator::ShiftLeft => "shift_left",
    BinaryOperator::ShiftRight => "shift_right",
    BinaryOperator::Equal => "eq",
    BinaryOperator::NotEqual => "ne",
    BinaryOperator::Less => "lt",
    BinaryOperator::LessEqual => "le",
    BinaryOperator::Greater => "gt",
    BinaryOperator::GreaterEqual => "ge",
  }
}

pub(super) const fn unary_operator_name(operator: UnaryOperator) -> &'static str
{
  match operator
  {
    UnaryOperator::LogicalNot => "logical_not",
    UnaryOperator::Positive => "positive",
    UnaryOperator::Negative => "negative",
  }
}

pub(super) const fn update_operator_name(operator: UpdateOperator) -> &'static str
{
  match operator
  {
    UpdateOperator::Increment => "increment",
    UpdateOperator::Decrement => "decrement",
  }
}

pub(super) const fn update_position_name(position: UpdatePosition) -> &'static str
{
  match position
  {
    UpdatePosition::Prefix => "prefix",
    UpdatePosition::Postfix => "postfix",
  }
}
