/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use std::collections::HashMap;

use crate::mir::{Function, LocalId, Statement};
use crate::xlil::{I64BinaryOperation, I64ComparisonOperation};

pub(super) fn fold_const_i64_operations(function: &mut Function) -> (usize, usize)
{
  let mut folded_binary = 0;
  let mut folded_comparison = 0;
  for block in &mut function.blocks
  {
    let mut constants = HashMap::<LocalId, i64>::new();
    for statement in &mut block.statements
    {
      match *statement
      {
        Statement::ConstI64 { local,
                              value,
                              .. } =>
        {
          constants.insert(local, value);
        }
        Statement::BinaryI64 { operation,
                               result,
                               left,
                               right,
                               span, } =>
        {
          let value = constants.get(&left)
                               .copied()
                               .zip(constants.get(&right).copied())
                               .and_then(|(left, right)| evaluate_binary(operation, left, right));
          if let Some(value) = value
          {
            *statement = Statement::ConstI64 { local: result,
                                               value,
                                               span };
            constants.insert(result, value);
            folded_binary += 1;
          }
          else
          {
            constants.remove(&result);
          }
        }
        Statement::CompareI64 { operation,
                                result,
                                left,
                                right,
                                span, } =>
        {
          let value = constants.get(&left)
                               .copied()
                               .zip(constants.get(&right).copied())
                               .map(|(left, right)| evaluate_comparison(operation, left, right));
          if let Some(value) = value
          {
            *statement = Statement::ConstBool { local: result,
                                                value,
                                                span };
            folded_comparison += 1;
          }
        }
        Statement::AddI64 { result, .. } |
        Statement::SubI64 { result, .. } |
        Statement::MulI64 { result, .. } |
        Statement::LoadLocal { result, .. } |
        Statement::Call { result: Some(result), .. } =>
        {
          constants.remove(&result);
        }
        _ =>
        {}
      }
    }
  }
  (folded_binary, folded_comparison)
}

fn evaluate_binary(operation: I64BinaryOperation, left: i64, right: i64) -> Option<i64>
{
  match operation
  {
    I64BinaryOperation::Div => left.checked_div(right),
    I64BinaryOperation::Rem => left.checked_rem(right),
    I64BinaryOperation::BitAnd => Some(left & right),
    I64BinaryOperation::BitOr => Some(left | right),
    I64BinaryOperation::BitXor => Some(left ^ right),
    I64BinaryOperation::ShiftLeft => u32::try_from(right).ok().and_then(|amount| left.checked_shl(amount)),
    I64BinaryOperation::ShiftRight => u32::try_from(right).ok().and_then(|amount| left.checked_shr(amount)),
  }
}

const fn evaluate_comparison(operation: I64ComparisonOperation, left: i64, right: i64) -> bool
{
  match operation
  {
    I64ComparisonOperation::Less => left < right,
    I64ComparisonOperation::LessEqual => left <= right,
    I64ComparisonOperation::Greater => left > right,
    I64ComparisonOperation::GreaterEqual => left >= right,
  }
}
