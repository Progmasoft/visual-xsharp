/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::*;

impl Parser<'_>
{
    pub(super) fn literal(&mut self, text: &str) -> Literal
    {
        if let Some(value) = text.strip_prefix("bool ")
        {
            return Literal::Bool(value == "true");
        }
        if let Some(value) = text.strip_prefix("integer ")
        {
            return Literal::Integer(value.to_string());
        }
        if let Some(value) = text.strip_prefix("float ")
        {
            return Literal::Float(value.to_string());
        }
        if let Some(value) = text.strip_prefix("char ")
        {
            return Literal::Char(crate::text::decode_character(value).unwrap_or(0));
        }
        if let Some(value) = text.strip_prefix("string ")
        {
            return Literal::String(value.trim_matches('"').to_string());
        }
        if let Some(value) = text.strip_prefix("enum ") &&
            let Some((path, tag)) = value.rsplit_once(" tag ") &&
            let Some((enum_type, variant)) = path.rsplit_once("::") &&
            let Ok(tag) = tag.parse::<u32>()
        {
            return Literal::EnumVariant {
                enum_type: enum_type.to_string(),
                variant: variant.to_string(),
                tag,
            };
        }
        Literal::None
    }
}
