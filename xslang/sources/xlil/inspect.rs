/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

use super::{Block, BlockId, Instruction, Terminator, ValueId};

impl Block
{
    /// Returns whether this block has a final control-flow record.
    #[must_use]
    pub const fn is_terminated(&self) -> bool
    {
        self.terminator.is_some()
    }

    /// Returns successor block ids in semantic order.
    #[must_use]
    pub fn successors(&self) -> Vec<BlockId>
    {
        self.terminator.as_ref().map_or_else(Vec::new, Terminator::successors)
    }
}

impl Terminator
{
    /// Returns successor block ids in semantic order.
    #[must_use]
    pub fn successors(&self) -> Vec<BlockId>
    {
        match *self
        {
            Self::Return(_) | Self::Panic => Vec::new(),
            Self::Branch(target) => vec![target],
            Self::BranchIf {
                then_block,
                else_block,
                ..
            } if then_block == else_block =>
            {
                vec![then_block]
            }
            Self::BranchIf {
                then_block,
                else_block,
                ..
            } => vec![then_block, else_block],
        }
    }

    /// Returns the register read directly by this terminator, if any.
    #[must_use]
    pub const fn operand(&self) -> Option<ValueId>
    {
        match *self
        {
            Self::Return(value) => value,
            Self::BranchIf {
                condition, ..
            } => Some(condition),
            Self::Branch(_) | Self::Panic => None,
        }
    }
}

impl Instruction
{
    /// Returns the canonical instruction-family mnemonic.
    #[must_use]
    pub const fn opcode(&self) -> &'static str
    {
        match self
        {
            Self::ConstI64 {
                ..
            } => "const.i64",
            Self::ConstI32 {
                ..
            } => "const.i32",
            Self::ConstU16 {
                ..
            } => "const.u16",
            Self::ConstInteger {
                ..
            } => "const.integer",
            Self::ConstF32 {
                ..
            } => "const.f32",
            Self::ConstF64 {
                ..
            } => "const.f64",
            Self::ConstStr {
                ..
            } => "const.str",
            Self::ConstBool {
                ..
            } => "const.bool",
            Self::BinaryInteger {
                ..
            } => "binary.integer",
            Self::BinaryFloat {
                ..
            } => "binary.float",
            Self::CompareFloat {
                ..
            } => "compare.float",
            Self::CompareStr {
                ..
            } => "compare.str",
            Self::AddI64 {
                ..
            } => "add.i64",
            Self::SubI64 {
                ..
            } => "sub.i64",
            Self::MulI64 {
                ..
            } => "mul.i64",
            Self::EqI64 {
                ..
            } => "eq.i64",
            Self::BinaryI64 {
                ..
            } => "binary.i64",
            Self::CompareI64 {
                ..
            } => "compare.i64",
            Self::AddI32 {
                ..
            } => "add.i32",
            Self::SubI32 {
                ..
            } => "sub.i32",
            Self::MulI32 {
                ..
            } => "mul.i32",
            Self::BinaryI32 {
                ..
            } => "binary.i32",
            Self::EqI32 {
                ..
            } => "eq.i32",
            Self::LtI32 {
                ..
            } => "lt.i32",
            Self::LeI32 {
                ..
            } => "le.i32",
            Self::GtI32 {
                ..
            } => "gt.i32",
            Self::GeI32 {
                ..
            } => "ge.i32",
            Self::NotBool {
                ..
            } => "not.bool",
            Self::Call {
                ..
            } => "call",
            Self::Aggregate {
                ..
            } => "aggregate",
            Self::Extract {
                ..
            } => "extract",
            Self::ArrayGet {
                ..
            } => "array.get",
            Self::ArraySet {
                ..
            } => "array.set",
            Self::ArrayLength {
                ..
            } => "array.length",
            Self::Load {
                ..
            } => "load",
            Self::Store {
                ..
            } => "store",
        }
    }

    /// Returns the register defined by this instruction, if it defines one.
    #[must_use]
    pub const fn result(&self) -> Option<ValueId>
    {
        match *self
        {
            Self::ConstI64 {
                result, ..
            } |
            Self::ConstI32 {
                result, ..
            } |
            Self::ConstU16 {
                result, ..
            } |
            Self::ConstInteger {
                result, ..
            } |
            Self::ConstF32 {
                result, ..
            } |
            Self::ConstF64 {
                result, ..
            } |
            Self::ConstBool {
                result, ..
            } |
            Self::BinaryInteger {
                result, ..
            } |
            Self::BinaryFloat {
                result, ..
            } |
            Self::CompareFloat {
                result, ..
            } |
            Self::CompareStr {
                result, ..
            } |
            Self::AddI64 {
                result, ..
            } |
            Self::SubI64 {
                result, ..
            } |
            Self::MulI64 {
                result, ..
            } |
            Self::EqI64 {
                result, ..
            } |
            Self::BinaryI64 {
                result, ..
            } |
            Self::CompareI64 {
                result, ..
            } |
            Self::AddI32 {
                result, ..
            } |
            Self::SubI32 {
                result, ..
            } |
            Self::MulI32 {
                result, ..
            } |
            Self::BinaryI32 {
                result, ..
            } |
            Self::EqI32 {
                result, ..
            } |
            Self::LtI32 {
                result, ..
            } |
            Self::LeI32 {
                result, ..
            } |
            Self::GtI32 {
                result, ..
            } |
            Self::GeI32 {
                result, ..
            } |
            Self::NotBool {
                result, ..
            } |
            Self::Aggregate {
                result, ..
            } |
            Self::Extract {
                result, ..
            } |
            Self::ArrayGet {
                result, ..
            } |
            Self::ArraySet {
                result, ..
            } |
            Self::ArrayLength {
                result, ..
            } |
            Self::Load {
                result, ..
            } |
            Self::ConstStr {
                result, ..
            } => Some(result),
            Self::Call {
                result, ..
            } => result,
            Self::Store {
                ..
            } => None,
        }
    }
}
