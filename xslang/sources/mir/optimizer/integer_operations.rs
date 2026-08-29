/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

use std::collections::HashMap;

use crate::mir::{Function, IntegerConstant, LocalId, Statement};
use crate::xlil::{IntegerBinaryOperation, Type};

pub(super) fn fold_const_integer_operations(function: &mut Function) -> usize
{
    let mut folded = 0;
    for block in &mut function.blocks
    {
        let mut constants = HashMap::<LocalId, IntegerConstant>::new();
        for statement in &mut block.statements
        {
            match *statement
            {
                Statement::ConstInteger {
                    local,
                    value,
                    ..
                } =>
                {
                    constants.insert(local, value);
                }
                Statement::BinaryInteger {
                    operation,
                    value_type,
                    result,
                    left,
                    right,
                    span,
                } =>
                {
                    let values = constants.get(&left).copied().zip(constants.get(&right).copied());
                    if operation.is_comparison()
                    {
                        if let Some(value) =
                            values.and_then(|(left, right)| compare(operation, value_type, left, right))
                        {
                            *statement = Statement::ConstBool {
                                local: result,
                                value,
                                span,
                            };
                            folded += 1;
                        }
                        continue;
                    }
                    if let Some(value) = values.and_then(|(left, right)| evaluate(operation, value_type, left, right))
                    {
                        *statement = Statement::ConstInteger {
                            local: result,
                            value,
                            span,
                        };
                        constants.insert(result, value);
                        folded += 1;
                    }
                    else
                    {
                        constants.remove(&result);
                    }
                }
                Statement::LoadLocal {
                    result, ..
                } |
                Statement::Call {
                    result: Some(result), ..
                } =>
                {
                    constants.remove(&result);
                }
                _ =>
                {}
            }
        }
    }
    folded
}

fn evaluate(
    operation: IntegerBinaryOperation,
    value_type: Type,
    left: IntegerConstant,
    right: IntegerConstant,
) -> Option<IntegerConstant>
{
    if left.value_type() != value_type || right.value_type() != value_type
    {
        return None;
    }
    let width = value_type.integer_width()?;
    let mask = if width == 128
    {
        u128::MAX
    }
    else
    {
        (1_u128 << width) - 1
    };
    let left_bits = left.bits();
    let right_bits = right.bits();
    let bits = match operation
    {
        IntegerBinaryOperation::Add => left_bits.wrapping_add(right_bits) & mask,
        IntegerBinaryOperation::Sub => left_bits.wrapping_sub(right_bits) & mask,
        IntegerBinaryOperation::Mul => left_bits.wrapping_mul(right_bits) & mask,
        IntegerBinaryOperation::BitAnd => left_bits & right_bits,
        IntegerBinaryOperation::BitOr => left_bits | right_bits,
        IntegerBinaryOperation::BitXor => left_bits ^ right_bits,
        IntegerBinaryOperation::ShiftLeft => left_bits.checked_shl(shift_amount(right_bits, width)?)? & mask,
        IntegerBinaryOperation::ShiftRight if value_type.is_signed_integer() =>
        {
            (signed_value(left_bits, width) >> shift_amount(right_bits, width)?) as u128 & mask
        }
        IntegerBinaryOperation::ShiftRight => left_bits.checked_shr(shift_amount(right_bits, width)?)?,
        IntegerBinaryOperation::Div | IntegerBinaryOperation::Rem if value_type.is_signed_integer() =>
        {
            let left = signed_value(left_bits, width);
            let right = signed_value(right_bits, width);
            if right == 0 || (left == signed_min(width) && right == -1)
            {
                return None;
            }
            let value = if operation == IntegerBinaryOperation::Div
            {
                left / right
            }
            else
            {
                left % right
            };
            value as u128 & mask
        }
        IntegerBinaryOperation::Div => left_bits.checked_div(right_bits)?,
        IntegerBinaryOperation::Rem => left_bits.checked_rem(right_bits)?,
        _ => return None,
    };
    IntegerConstant::from_bits(value_type, bits)
}

fn compare(
    operation: IntegerBinaryOperation,
    value_type: Type,
    left: IntegerConstant,
    right: IntegerConstant,
) -> Option<bool>
{
    if left.value_type() != value_type || right.value_type() != value_type
    {
        return None;
    }
    if !value_type.is_signed_integer()
    {
        let (left, right) = (left.bits(), right.bits());
        return Some(match operation
        {
            IntegerBinaryOperation::Equal => left == right,
            IntegerBinaryOperation::NotEqual => left != right,
            IntegerBinaryOperation::Less => left < right,
            IntegerBinaryOperation::LessEqual => left <= right,
            IntegerBinaryOperation::Greater => left > right,
            IntegerBinaryOperation::GreaterEqual => left >= right,
            _ => return None,
        });
    }
    let (left, right) = {
        let width = value_type.integer_width()?;
        (signed_value(left.bits(), width), signed_value(right.bits(), width))
    };
    Some(match operation
    {
        IntegerBinaryOperation::Equal => left == right,
        IntegerBinaryOperation::NotEqual => left != right,
        IntegerBinaryOperation::Less => left < right,
        IntegerBinaryOperation::LessEqual => left <= right,
        IntegerBinaryOperation::Greater => left > right,
        IntegerBinaryOperation::GreaterEqual => left >= right,
        _ => return None,
    })
}

fn shift_amount(bits: u128, width: u32) -> Option<u32>
{
    let amount = u32::try_from(bits).ok()?;
    (amount < width).then_some(amount)
}

const fn signed_value(bits: u128, width: u32) -> i128
{
    if width == 128
    {
        bits as i128
    }
    else
    {
        let shift = 128 - width;
        ((bits << shift) as i128) >> shift
    }
}

const fn signed_min(width: u32) -> i128
{
    if width == 128
    {
        i128::MIN
    }
    else
    {
        -(1_i128 << (width - 1))
    }
}

#[cfg(test)]
mod tests
{
    use crate::hir::Span;
    use crate::mir::{BasicBlock, BlockId, Function, Local, LocalId, Terminator};

    use super::*;

    #[test]
    fn folds_modular_u8_arithmetic_and_full_u128_comparison()
    {
        let span = Span::new(1, 0, 1);
        let locals = [
            (0, Type::U8),
            (1, Type::U8),
            (2, Type::U8),
            (3, Type::U128),
            (4, Type::U128),
            (5, Type::BOOL),
        ]
        .into_iter()
        .map(|(id, value_type)| Local {
            id: LocalId(id),
            name: format!("local_{id}"),
            value_type: Some(value_type),
            mutable: false,
            span,
        })
        .collect();
        let mut function = Function {
            name: "fold_integer".to_string(),
            return_type: Type::BOOL,
            parameters: vec![],
            locals,
            blocks: vec![BasicBlock {
                id: BlockId(0),
                statements: vec![
                    Statement::ConstInteger {
                        local: LocalId(0),
                        value: IntegerConstant::U8(u8::MAX),
                        span,
                    },
                    Statement::ConstInteger {
                        local: LocalId(1),
                        value: IntegerConstant::U8(1),
                        span,
                    },
                    Statement::BinaryInteger {
                        operation: IntegerBinaryOperation::Add,
                        value_type: Type::U8,
                        result: LocalId(2),
                        left: LocalId(0),
                        right: LocalId(1),
                        span,
                    },
                    Statement::ConstInteger {
                        local: LocalId(3),
                        value: IntegerConstant::U128(u128::MAX),
                        span,
                    },
                    Statement::ConstInteger {
                        local: LocalId(4),
                        value: IntegerConstant::U128(0),
                        span,
                    },
                    Statement::BinaryInteger {
                        operation: IntegerBinaryOperation::Greater,
                        value_type: Type::U128,
                        result: LocalId(5),
                        left: LocalId(3),
                        right: LocalId(4),
                        span,
                    },
                ],
                terminator: Some(Terminator::Return(Some(LocalId(5)))),
                span,
            }],
        };

        assert_eq!(fold_const_integer_operations(&mut function), 2);
        assert!(matches!(function.blocks[0].statements[2], Statement::ConstInteger {
            value: IntegerConstant::U8(0),
            ..
        }));
        assert!(matches!(function.blocks[0].statements[5], Statement::ConstBool {
            value: true,
            ..
        }));
    }
}
