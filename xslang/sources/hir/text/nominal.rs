/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

use super::*;

pub(super) fn write_field(output: &mut String, path: &crate::hir::type_check::FieldPath, pad: &str)
{
    let mutability = if path.mutable
    {
        "mutable"
    }
    else
    {
        "immutable"
    };
    let _ = writeln!(
        output,
        "{pad}field {mutability} {} : {}",
        field_path_name(path),
        type_name(&path.ty)
    );
}

pub(super) fn write_member(
    output: &mut String,
    receiver: &Expression,
    owner: &str,
    name: &str,
    field_type: &Type,
    indent: usize,
)
{
    let pad = "  ".repeat(indent);
    let _ = writeln!(output, "{pad}member {owner}.{name} : {}", type_name(field_type));
    write_expression(output, receiver, indent + 1);
    let _ = writeln!(output, "{pad}.end");
}

pub(super) fn write_desugared_member(
    output: &mut String,
    receiver: &DesugaredExpression,
    owner: &str,
    name: &str,
    field_type: &Type,
    indent: usize,
)
{
    let pad = "  ".repeat(indent);
    let _ = writeln!(output, "{pad}member {owner}.{name} : {}", type_name(field_type));
    write_desugared_expression(output, receiver, indent + 1);
    let _ = writeln!(output, "{pad}.end");
}
