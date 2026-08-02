/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use std::collections::HashMap;

use crate::mir::{BasicBlock, Function, Statement};
use crate::xlil::I32BinaryOperation;

pub(super) fn fold_const_i32_binary(function: &mut Function) -> usize
{
    function.blocks.iter_mut().map(fold_block).sum()
}

fn fold_block(block: &mut BasicBlock) -> usize
{
    let mut constants = HashMap::new();
    let mut folded = 0;
    for statement in &mut block.statements
    {
        match statement
        {
            Statement::ConstI32 {
                local,
                value,
                ..
            } =>
            {
                constants.insert(*local, *value);
            }
            Statement::BinaryI32 {
                operation,
                result,
                left,
                right,
                span,
            } =>
            {
                let result = *result;
                let span = *span;
                let Some(value) = constants
                    .get(left)
                    .copied()
                    .zip(constants.get(right).copied())
                    .and_then(|(left, right)| evaluate(*operation, left, right))
                else
                {
                    constants.remove(&result);
                    continue;
                };
                *statement = Statement::ConstI32 {
                    local: result,
                    value,
                    span,
                };
                constants.insert(result, value);
                folded += 1;
            }
            Statement::AddI32 {
                result, ..
            } |
            Statement::SubI32 {
                result, ..
            } |
            Statement::MulI32 {
                result, ..
            } |
            Statement::EqI32 {
                result, ..
            } |
            Statement::LtI32 {
                result, ..
            } |
            Statement::LeI32 {
                result, ..
            } |
            Statement::GtI32 {
                result, ..
            } |
            Statement::GeI32 {
                result, ..
            } |
            Statement::NotBool {
                result, ..
            } |
            Statement::LoadLocal {
                result, ..
            } |
            Statement::Call {
                result: Some(result), ..
            } =>
            {
                constants.remove(result);
            }
            _ =>
            {}
        }
    }
    folded
}

fn evaluate(operation: I32BinaryOperation, left: i32, right: i32) -> Option<i32>
{
    match operation
    {
        I32BinaryOperation::Div => left.checked_div(right),
        I32BinaryOperation::Rem => left.checked_rem(right),
        I32BinaryOperation::BitAnd => Some(left & right),
        I32BinaryOperation::BitOr => Some(left | right),
        I32BinaryOperation::BitXor => Some(left ^ right),
        I32BinaryOperation::ShiftLeft => u32::try_from(right).ok().and_then(|amount| left.checked_shl(amount)),
        I32BinaryOperation::ShiftRight => u32::try_from(right).ok().and_then(|amount| left.checked_shr(amount)),
    }
}
