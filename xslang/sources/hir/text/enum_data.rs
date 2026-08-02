/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use std::fmt::Write;

use super::{DesugaredExpression, Expression, Type, type_name};

pub(super) fn write_expression(output: &mut String, expression: &Expression, indent: usize)
{
    let Expression::EnumData {
        enum_type,
        owner,
        variant,
        tag,
        payload,
        payload_type,
        ..
    } = expression
    else
    {
        return;
    };
    write_record(output, enum_type, owner, variant, *tag, payload_type.as_deref(), indent);
    if let Some(payload) = payload
    {
        let pad = "  ".repeat(indent);
        let _ = writeln!(output, "{pad}  value");
        super::write_expression(output, payload, indent + 2);
    }
    let _ = writeln!(output, "{}.end", "  ".repeat(indent));
}

pub(super) fn write_desugared_expression(output: &mut String, expression: &DesugaredExpression, indent: usize)
{
    let DesugaredExpression::EnumData {
        enum_type,
        owner,
        variant,
        tag,
        payload,
        payload_type,
        ..
    } = expression
    else
    {
        return;
    };
    write_record(output, enum_type, owner, variant, *tag, payload_type.as_deref(), indent);
    if let Some(payload) = payload
    {
        let pad = "  ".repeat(indent);
        let _ = writeln!(output, "{pad}  value");
        super::write_desugared_expression(output, payload, indent + 2);
    }
    let _ = writeln!(output, "{}.end", "  ".repeat(indent));
}

fn write_record(
    output: &mut String,
    enum_type: &str,
    owner: &str,
    variant: &str,
    tag: u32,
    payload_type: Option<&Type>,
    indent: usize,
)
{
    let pad = "  ".repeat(indent);
    let payload_name = payload_type.map(type_name).unwrap_or_else(|| "()".to_string());
    let _ = writeln!(
        output,
        "{pad}enum_data {enum_type}::{variant} owner {owner} tag {tag} payload {payload_name}"
    );
}
