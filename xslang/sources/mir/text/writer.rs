/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

use std::fmt::Write;

use crate::mir::{BasicBlock, Function, LocalId, Parameter, Statement, Terminator};
use crate::xlil::{Type, type_text};

#[must_use]
pub fn function_to_xmir(function: &Function) -> String
{
    let mut output = String::new();
    let _ = writeln!(output, ".xmir version 1");
    let _ = writeln!(output, "function {}", function.name);
    let _ = writeln!(output, "returns {}", type_text(function.return_type));
    write_parameters(&mut output, function);
    write_locals(&mut output, function);
    write_blocks(&mut output, function);
    let _ = writeln!(output, ".program end");
    output
}

fn write_parameters(output: &mut String, function: &Function)
{
    if function.parameters.is_empty()
    {
        return;
    }
    let _ = writeln!(output);
    let _ = writeln!(output, "parameters");
    for parameter in &function.parameters
    {
        write_parameter(output, parameter);
    }
    let _ = writeln!(output, ".end");
}

fn write_parameter(output: &mut String, parameter: &Parameter)
{
    let _ = writeln!(output, "  parameter {}", parameter.name);
    let _ = writeln!(output, "    local {}", parameter.local.0);
    let _ = writeln!(output, "    type {}", type_text(parameter.value_type));
}

fn write_locals(output: &mut String, function: &Function)
{
    if function.locals.is_empty()
    {
        return;
    }
    let _ = writeln!(output);
    let _ = writeln!(output, "locals");
    for local in &function.locals
    {
        let mutability = if local.mutable
        {
            "mutable"
        }
        else
        {
            "immutable"
        };
        let _ = writeln!(output, "  local {}", local.id.0);
        let _ = writeln!(output, "    name {}", local.name);
        if let Some(value_type) = local.value_type
        {
            let _ = writeln!(output, "    type {}", type_text(value_type));
        }
        let _ = writeln!(output, "    mutability {mutability}");
    }
    let _ = writeln!(output, ".end");
}

fn write_blocks(output: &mut String, function: &Function)
{
    if function.blocks.is_empty()
    {
        return;
    }
    let _ = writeln!(output);
    let _ = writeln!(output, "control_flow");
    for block in &function.blocks
    {
        write_block(output, block);
    }
    let _ = writeln!(output, ".end");
}

fn write_block(output: &mut String, block: &BasicBlock)
{
    let _ = writeln!(output, "  block {}", block.id.0);
    if !block.statements.is_empty()
    {
        let _ = writeln!(output, "    statements");
        for statement in &block.statements
        {
            write_statement(output, statement);
        }
    }
    match &block.terminator
    {
        Some(terminator) => write_terminator(output, terminator),
        None =>
        {
            let _ = writeln!(output, "    terminator missing");
        }
    }
}

fn write_statement(output: &mut String, statement: &Statement)
{
    match statement
    {
        Statement::Use {
            local, ..
        } => write_local_statement(output, "use", *local),
        Statement::Move {
            local, ..
        } => write_local_statement(output, "move", *local),
        Statement::BorrowShared {
            local, ..
        } => write_local_statement(output, "borrow shared", *local),
        Statement::BorrowMutable {
            local, ..
        } => write_local_statement(output, "borrow mutable", *local),
        Statement::EndBorrow {
            local, ..
        } => write_local_statement(output, "borrow end", *local),
        Statement::ConstI64 {
            local,
            value,
            ..
        } =>
        {
            let _ = writeln!(output, "      statement const.i64");
            let _ = writeln!(output, "        target local {}", local.0);
            let _ = writeln!(output, "        value {value}");
        }
        Statement::ConstI32 {
            local,
            value,
            ..
        } =>
        {
            let _ = writeln!(output, "      statement const.i32");
            let _ = writeln!(output, "        target local {}", local.0);
            let _ = writeln!(output, "        value {value}");
        }
        Statement::ConstU16 {
            local,
            value,
            ..
        } =>
        {
            let _ = writeln!(output, "      statement const.u16");
            let _ = writeln!(output, "        target local {}", local.0);
            let _ = writeln!(output, "        value 0x{value:04x}");
        }
        Statement::ConstInteger {
            local,
            value,
            ..
        } =>
        {
            let _ = writeln!(output, "      statement const.{}", value.text_name());
            let _ = writeln!(output, "        target local {}", local.0);
            let _ = writeln!(output, "        value {}", value.decimal());
        }
        Statement::ConstF32 {
            local,
            bits,
            ..
        } =>
        {
            let _ = writeln!(output, "      statement const.f32");
            let _ = writeln!(output, "        target local {}", local.0);
            let _ = writeln!(output, "        bits 0x{bits:08x}");
        }
        Statement::ConstF64 {
            local,
            bits,
            ..
        } =>
        {
            let _ = writeln!(output, "      statement const.f64");
            let _ = writeln!(output, "        target local {}", local.0);
            let _ = writeln!(output, "        bits 0x{bits:016x}");
        }
        Statement::ConstStr {
            local,
            units,
            ..
        } =>
        {
            let _ = writeln!(output, "      statement const.str");
            let _ = writeln!(output, "        target local {}", local.0);
            let _ = writeln!(output, "        value {}", crate::text::format_units(units));
        }
        Statement::ConstBool {
            local,
            value,
            ..
        } =>
        {
            let _ = writeln!(output, "      statement const.bool");
            let _ = writeln!(output, "        target local {}", local.0);
            let _ = writeln!(output, "        value {value}");
        }
        Statement::BinaryInteger {
            operation,
            value_type,
            result,
            left,
            right,
            ..
        } => write_binary(
            output,
            &format!("{}.{}", operation.text_stem(), type_text(*value_type)),
            *result,
            *left,
            *right,
        ),
        Statement::BinaryFloat {
            operation,
            value_type,
            result,
            left,
            right,
            ..
        } => write_binary(
            output,
            &format!("{}.{}", operation.text_stem(), type_text(*value_type)),
            *result,
            *left,
            *right,
        ),
        Statement::CompareFloat {
            operation,
            value_type,
            result,
            left,
            right,
            ..
        } => write_binary(
            output,
            &format!("{}.{}", operation.text_stem(), type_text(*value_type)),
            *result,
            *left,
            *right,
        ),
        Statement::CompareStr {
            operation,
            result,
            left,
            right,
            ..
        } => write_binary(
            output,
            &format!("{}.str", operation.text_stem()),
            *result,
            *left,
            *right,
        ),
        Statement::StoreLocal {
            local,
            value,
            ..
        } =>
        {
            let _ = writeln!(output, "      statement store.local");
            let _ = writeln!(output, "        target local {}", local.0);
            let _ = writeln!(output, "        value local {}", value.0);
        }
        Statement::LoadLocal {
            result,
            local,
            ..
        } =>
        {
            let _ = writeln!(output, "      statement load.local");
            let _ = writeln!(output, "        result local {}", result.0);
            let _ = writeln!(output, "        source local {}", local.0);
        }
        Statement::Aggregate {
            result,
            value_type,
            fields,
            field_types,
            ..
        } =>
        {
            let _ = writeln!(output, "      statement aggregate");
            let _ = writeln!(output, "        result local {}", result.0);
            let _ = writeln!(output, "        type {}", type_text(*value_type));
            for (field, field_type) in fields.iter().zip(field_types)
            {
                let _ = writeln!(
                    output,
                    "        field local {} type {}",
                    field.0,
                    type_text(*field_type)
                );
            }
        }
        Statement::Extract {
            result,
            aggregate,
            field,
            field_type,
            ..
        } =>
        {
            let _ = writeln!(output, "      statement extract");
            let _ = writeln!(output, "        result local {}", result.0);
            let _ = writeln!(output, "        aggregate local {}", aggregate.0);
            let _ = writeln!(output, "        field {field}");
            let _ = writeln!(output, "        type {}", type_text(*field_type));
        }
        Statement::ArrayGet {
            result,
            array,
            index,
            array_type,
            element_type,
            ..
        } =>
        {
            let _ = writeln!(output, "      statement array.get");
            write_array_access(output, *result, *array, *index, *array_type, *element_type);
        }
        Statement::ArraySet {
            result,
            array,
            index,
            value,
            array_type,
            element_type,
            ..
        } =>
        {
            let _ = writeln!(output, "      statement array.set");
            write_array_access(output, *result, *array, *index, *array_type, *element_type);
            let _ = writeln!(output, "        value local {}", value.0);
        }
        Statement::ArrayLength {
            result,
            array,
            array_type,
            ..
        } =>
        {
            let _ = writeln!(output, "      statement array.length");
            let _ = writeln!(output, "        result local {}", result.0);
            let _ = writeln!(output, "        array local {}", array.0);
            let _ = writeln!(output, "        array_type {}", type_text(*array_type));
        }
        Statement::AddI64 {
            result,
            left,
            right,
            ..
        } => write_binary(output, "add.i64", *result, *left, *right),
        Statement::SubI64 {
            result,
            left,
            right,
            ..
        } => write_binary(output, "sub.i64", *result, *left, *right),
        Statement::MulI64 {
            result,
            left,
            right,
            ..
        } => write_binary(output, "mul.i64", *result, *left, *right),
        Statement::EqI64 {
            result,
            left,
            right,
            ..
        } => write_binary(output, "eq.i64", *result, *left, *right),
        Statement::BinaryI64 {
            operation,
            result,
            left,
            right,
            ..
        } => write_binary(output, operation.text_name(), *result, *left, *right),
        Statement::CompareI64 {
            operation,
            result,
            left,
            right,
            ..
        } => write_binary(output, operation.text_name(), *result, *left, *right),
        Statement::AddI32 {
            result,
            left,
            right,
            ..
        } => write_binary(output, "add.i32", *result, *left, *right),
        Statement::SubI32 {
            result,
            left,
            right,
            ..
        } => write_binary(output, "sub.i32", *result, *left, *right),
        Statement::MulI32 {
            result,
            left,
            right,
            ..
        } => write_binary(output, "mul.i32", *result, *left, *right),
        Statement::BinaryI32 {
            operation,
            result,
            left,
            right,
            ..
        } => write_binary(output, operation.text_name(), *result, *left, *right),
        Statement::EqI32 {
            result,
            left,
            right,
            ..
        } => write_binary(output, "eq.i32", *result, *left, *right),
        Statement::LtI32 {
            result,
            left,
            right,
            ..
        } => write_binary(output, "lt.i32", *result, *left, *right),
        Statement::LeI32 {
            result,
            left,
            right,
            ..
        } => write_binary(output, "le.i32", *result, *left, *right),
        Statement::GtI32 {
            result,
            left,
            right,
            ..
        } => write_binary(output, "gt.i32", *result, *left, *right),
        Statement::GeI32 {
            result,
            left,
            right,
            ..
        } => write_binary(output, "ge.i32", *result, *left, *right),
        Statement::NotBool {
            result,
            operand,
            ..
        } =>
        {
            let _ = writeln!(output, "      statement not.bool");
            let _ = writeln!(output, "        result local {}", result.0);
            let _ = writeln!(output, "        operand local {}", operand.0);
        }
        Statement::Call {
            result,
            function,
            arguments,
            return_type,
            ..
        } =>
        {
            let _ = writeln!(output, "      statement call");
            let _ = writeln!(output, "        function {function}");
            let _ = writeln!(output, "        returns {}", type_text(*return_type));
            write_call_result(output, *result);
            for argument in arguments
            {
                let _ = writeln!(output, "        argument local {}", argument.0);
            }
        }
        Statement::Drop {
            local, ..
        } => write_local_statement(output, "drop", *local),
    }
}

fn write_binary(output: &mut String, name: &str, result: LocalId, left: LocalId, right: LocalId)
{
    let _ = writeln!(output, "      statement {name}");
    let _ = writeln!(output, "        result local {}", result.0);
    let _ = writeln!(output, "        left local {}", left.0);
    let _ = writeln!(output, "        right local {}", right.0);
}

fn write_array_access(
    output: &mut String,
    result: LocalId,
    array: LocalId,
    index: LocalId,
    array_type: Type,
    element_type: Type,
)
{
    let _ = writeln!(output, "        result local {}", result.0);
    let _ = writeln!(output, "        array local {}", array.0);
    let _ = writeln!(output, "        index local {}", index.0);
    let _ = writeln!(output, "        array_type {}", type_text(array_type));
    let _ = writeln!(output, "        element_type {}", type_text(element_type));
}

fn write_call_result(output: &mut String, result: Option<LocalId>)
{
    if let Some(result) = result
    {
        let _ = writeln!(output, "        result local {}", result.0);
    }
    else
    {
        let _ = writeln!(output, "        result discard");
    }
}

fn write_local_statement(output: &mut String, name: &str, local: LocalId)
{
    let _ = writeln!(output, "      statement {name}");
    let _ = writeln!(output, "        local {}", local.0);
}

fn write_terminator(output: &mut String, terminator: &Terminator)
{
    match terminator
    {
        Terminator::Return(Some(local)) =>
        {
            let _ = writeln!(output, "    terminator return");
            let _ = writeln!(output, "      value local {}", local.0);
        }
        Terminator::Return(None) =>
        {
            let _ = writeln!(output, "    terminator return");
        }
        Terminator::Goto(block) =>
        {
            let _ = writeln!(output, "    terminator goto");
            let _ = writeln!(output, "      target block {}", block.0);
        }
        Terminator::BranchIf {
            condition,
            then_block,
            else_block,
        } =>
        {
            let _ = writeln!(output, "    terminator branch_if");
            let _ = writeln!(output, "      condition local {}", condition.0);
            let _ = writeln!(output, "      then block {}", then_block.0);
            let _ = writeln!(output, "      else block {}", else_block.0);
        }
        Terminator::Panic =>
        {
            let _ = writeln!(output, "    terminator panic");
        }
        Terminator::Unreachable =>
        {
            let _ = writeln!(output, "    terminator unreachable");
        }
    }
}
