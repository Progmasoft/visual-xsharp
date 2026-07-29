/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use crate::hir::type_check::BinaryOperator;
use crate::xlil::{
  FloatBinaryOperation, FloatComparisonOperation, I32BinaryOperation, I64BinaryOperation, I64ComparisonOperation,
  IntegerBinaryOperation, StrComparisonOperation,
};

pub(super) const fn integer_operation(operator: BinaryOperator) -> Option<IntegerBinaryOperation>
{
  Some(match operator
  {
    BinaryOperator::Add => IntegerBinaryOperation::Add,
    BinaryOperator::Sub => IntegerBinaryOperation::Sub,
    BinaryOperator::Mul => IntegerBinaryOperation::Mul,
    BinaryOperator::Div => IntegerBinaryOperation::Div,
    BinaryOperator::Rem => IntegerBinaryOperation::Rem,
    BinaryOperator::BitAnd => IntegerBinaryOperation::BitAnd,
    BinaryOperator::BitOr => IntegerBinaryOperation::BitOr,
    BinaryOperator::BitXor => IntegerBinaryOperation::BitXor,
    BinaryOperator::ShiftLeft => IntegerBinaryOperation::ShiftLeft,
    BinaryOperator::ShiftRight => IntegerBinaryOperation::ShiftRight,
    BinaryOperator::Equal => IntegerBinaryOperation::Equal,
    BinaryOperator::NotEqual => IntegerBinaryOperation::NotEqual,
    BinaryOperator::Less => IntegerBinaryOperation::Less,
    BinaryOperator::LessEqual => IntegerBinaryOperation::LessEqual,
    BinaryOperator::Greater => IntegerBinaryOperation::Greater,
    BinaryOperator::GreaterEqual => IntegerBinaryOperation::GreaterEqual,
    BinaryOperator::LogicalAnd | BinaryOperator::LogicalOr | BinaryOperator::Coalesce => return None,
  })
}

pub(super) const fn comparison_str_operation(operator: BinaryOperator) -> Option<StrComparisonOperation>
{
  Some(match operator
  {
    BinaryOperator::Equal => StrComparisonOperation::Equal,
    BinaryOperator::NotEqual => StrComparisonOperation::NotEqual,
    _ => return None,
  })
}

pub(super) const fn binary_float_operation(operator: BinaryOperator) -> Option<FloatBinaryOperation>
{
  Some(match operator
  {
    BinaryOperator::Add => FloatBinaryOperation::Add,
    BinaryOperator::Sub => FloatBinaryOperation::Sub,
    BinaryOperator::Mul => FloatBinaryOperation::Mul,
    BinaryOperator::Div => FloatBinaryOperation::Div,
    BinaryOperator::Rem => FloatBinaryOperation::Rem,
    _ => return None,
  })
}

pub(super) const fn comparison_float_operation(operator: BinaryOperator) -> Option<FloatComparisonOperation>
{
  Some(match operator
  {
    BinaryOperator::Equal => FloatComparisonOperation::Equal,
    BinaryOperator::NotEqual => FloatComparisonOperation::NotEqual,
    BinaryOperator::Less => FloatComparisonOperation::Less,
    BinaryOperator::LessEqual => FloatComparisonOperation::LessEqual,
    BinaryOperator::Greater => FloatComparisonOperation::Greater,
    BinaryOperator::GreaterEqual => FloatComparisonOperation::GreaterEqual,
    _ => return None,
  })
}

pub(super) const fn binary_i32_operation(operator: BinaryOperator) -> Option<I32BinaryOperation>
{
  Some(match operator
  {
    BinaryOperator::Div => I32BinaryOperation::Div,
    BinaryOperator::Rem => I32BinaryOperation::Rem,
    BinaryOperator::BitAnd => I32BinaryOperation::BitAnd,
    BinaryOperator::BitOr => I32BinaryOperation::BitOr,
    BinaryOperator::BitXor => I32BinaryOperation::BitXor,
    BinaryOperator::ShiftLeft => I32BinaryOperation::ShiftLeft,
    BinaryOperator::ShiftRight => I32BinaryOperation::ShiftRight,
    _ => return None,
  })
}

pub(super) const fn binary_i64_operation(operator: BinaryOperator) -> Option<I64BinaryOperation>
{
  Some(match operator
  {
    BinaryOperator::Div => I64BinaryOperation::Div,
    BinaryOperator::Rem => I64BinaryOperation::Rem,
    BinaryOperator::BitAnd => I64BinaryOperation::BitAnd,
    BinaryOperator::BitOr => I64BinaryOperation::BitOr,
    BinaryOperator::BitXor => I64BinaryOperation::BitXor,
    BinaryOperator::ShiftLeft => I64BinaryOperation::ShiftLeft,
    BinaryOperator::ShiftRight => I64BinaryOperation::ShiftRight,
    _ => return None,
  })
}

pub(super) const fn comparison_i64_operation(operator: BinaryOperator) -> Option<I64ComparisonOperation>
{
  Some(match operator
  {
    BinaryOperator::Less => I64ComparisonOperation::Less,
    BinaryOperator::LessEqual => I64ComparisonOperation::LessEqual,
    BinaryOperator::Greater => I64ComparisonOperation::Greater,
    BinaryOperator::GreaterEqual => I64ComparisonOperation::GreaterEqual,
    _ => return None,
  })
}
