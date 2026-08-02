/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::*;

pub(super) fn primitive(name: &str) -> Option<PrimitiveType>
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

pub(super) fn first_child_kind<'a>(tree: &'a SyntaxTree, parent: &'a SyntaxNode, kind: u32) -> Option<&'a SyntaxNode>
{
    parent
        .children
        .iter()
        .filter_map(|index| tree.nodes.get(*index))
        .find(|child| child.kind == kind)
}

pub(super) fn path_text(tree: &SyntaxTree, value: &SyntaxNode) -> String
{
    if value.kind == IDENTIFIER
    {
        return value.text.clone();
    }
    let path = if value.kind == PATH
    {
        value
    }
    else
    {
        first_child_kind(tree, value, PATH).unwrap_or(value)
    };
    path.children
        .iter()
        .filter_map(|index| tree.nodes.get(*index))
        .filter(|child| child.kind == IDENTIFIER)
        .map(|child| child.text.as_str())
        .collect::<Vec<_>>()
        .join("::")
}
