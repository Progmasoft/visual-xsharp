/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

/// Legacy XLIL v0 `i32` operations not represented by dedicated records.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum I32BinaryOperation
{
    /// Signed division.
    Div,
    /// Signed remainder.
    Rem,
    /// Bitwise AND.
    BitAnd,
    /// Bitwise OR.
    BitOr,
    /// Bitwise XOR.
    BitXor,
    /// Left shift.
    ShiftLeft,
    /// Arithmetic right shift.
    ShiftRight,
}

/// Uniform operation set for exact-width integer instructions.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum IntegerBinaryOperation
{
    /// Addition.
    Add,
    /// Subtraction.
    Sub,
    /// Multiplication.
    Mul,
    /// Signed or unsigned division according to the operand type.
    Div,
    /// Signed or unsigned remainder according to the operand type.
    Rem,
    /// Bitwise AND.
    BitAnd,
    /// Bitwise OR.
    BitOr,
    /// Bitwise XOR.
    BitXor,
    /// Left shift.
    ShiftLeft,
    /// Signed arithmetic or unsigned logical right shift.
    ShiftRight,
    /// Equality comparison.
    Equal,
    /// Inequality comparison.
    NotEqual,
    /// Signed or unsigned less-than comparison.
    Less,
    /// Signed or unsigned less-than-or-equal comparison.
    LessEqual,
    /// Signed or unsigned greater-than comparison.
    Greater,
    /// Signed or unsigned greater-than-or-equal comparison.
    GreaterEqual,
}

impl IntegerBinaryOperation
{
    #[must_use]
    pub(crate) const fn text_stem(self) -> &'static str
    {
        match self
        {
            Self::Add => "add",
            Self::Sub => "sub",
            Self::Mul => "mul",
            Self::Div => "div",
            Self::Rem => "rem",
            Self::BitAnd => "and",
            Self::BitOr => "or",
            Self::BitXor => "xor",
            Self::ShiftLeft => "shl",
            Self::ShiftRight => "shr",
            Self::Equal => "eq",
            Self::NotEqual => "ne",
            Self::Less => "lt",
            Self::LessEqual => "le",
            Self::Greater => "gt",
            Self::GreaterEqual => "ge",
        }
    }

    #[must_use]
    pub(crate) fn parse_text_stem(name: &str) -> Option<Self>
    {
        Some(match name
        {
            "add" => Self::Add,
            "sub" => Self::Sub,
            "mul" => Self::Mul,
            "div" => Self::Div,
            "rem" => Self::Rem,
            "and" => Self::BitAnd,
            "or" => Self::BitOr,
            "xor" => Self::BitXor,
            "shl" => Self::ShiftLeft,
            "shr" => Self::ShiftRight,
            "eq" => Self::Equal,
            "ne" => Self::NotEqual,
            "lt" => Self::Less,
            "le" => Self::LessEqual,
            "gt" => Self::Greater,
            "ge" => Self::GreaterEqual,
            _ => return None,
        })
    }

    #[must_use]
    pub(crate) const fn is_comparison(self) -> bool
    {
        matches!(
            self,
            Self::Equal | Self::NotEqual | Self::Less | Self::LessEqual | Self::Greater | Self::GreaterEqual
        )
    }
}

/// Legacy XLIL v0 `i64` operations.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum I64BinaryOperation
{
    /// Signed division.
    Div,
    /// Signed remainder.
    Rem,
    /// Bitwise AND.
    BitAnd,
    /// Bitwise OR.
    BitOr,
    /// Bitwise XOR.
    BitXor,
    /// Left shift.
    ShiftLeft,
    /// Arithmetic right shift.
    ShiftRight,
}

impl I64BinaryOperation
{
    #[must_use]
    pub(crate) const fn text_name(self) -> &'static str
    {
        match self
        {
            Self::Div => "div.i64",
            Self::Rem => "rem.i64",
            Self::BitAnd => "and.i64",
            Self::BitOr => "or.i64",
            Self::BitXor => "xor.i64",
            Self::ShiftLeft => "shl.i64",
            Self::ShiftRight => "shr.i64",
        }
    }

    #[must_use]
    pub(crate) fn parse_text(name: &str) -> Option<Self>
    {
        Some(match name
        {
            "div.i64" => Self::Div,
            "rem.i64" => Self::Rem,
            "and.i64" => Self::BitAnd,
            "or.i64" => Self::BitOr,
            "xor.i64" => Self::BitXor,
            "shl.i64" => Self::ShiftLeft,
            "shr.i64" => Self::ShiftRight,
            _ => return None,
        })
    }
}

/// Legacy signed `i64` ordering comparisons.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum I64ComparisonOperation
{
    /// Less-than.
    Less,
    /// Less-than-or-equal.
    LessEqual,
    /// Greater-than.
    Greater,
    /// Greater-than-or-equal.
    GreaterEqual,
}

/// Floating-point arithmetic operation.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum FloatBinaryOperation
{
    /// Addition.
    Add,
    /// Subtraction.
    Sub,
    /// Multiplication.
    Mul,
    /// Division.
    Div,
    /// Remainder.
    Rem,
}

/// Ordered floating-point comparison.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum FloatComparisonOperation
{
    /// Equality.
    Equal,
    /// Inequality.
    NotEqual,
    /// Less-than.
    Less,
    /// Less-than-or-equal.
    LessEqual,
    /// Greater-than.
    Greater,
    /// Greater-than-or-equal.
    GreaterEqual,
}

/// UTF-32 string comparison.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum StrComparisonOperation
{
    /// Equality.
    Equal,
    /// Inequality.
    NotEqual,
}

impl StrComparisonOperation
{
    #[must_use]
    pub(crate) const fn text_stem(self) -> &'static str
    {
        match self
        {
            Self::Equal => "eq",
            Self::NotEqual => "ne",
        }
    }

    #[must_use]
    pub(crate) fn parse_text_stem(name: &str) -> Option<Self>
    {
        match name
        {
            "eq" => Some(Self::Equal),
            "ne" => Some(Self::NotEqual),
            _ => None,
        }
    }
}

impl FloatBinaryOperation
{
    #[must_use]
    pub(crate) const fn text_stem(self) -> &'static str
    {
        match self
        {
            Self::Add => "add",
            Self::Sub => "sub",
            Self::Mul => "mul",
            Self::Div => "div",
            Self::Rem => "rem",
        }
    }

    #[must_use]
    pub(crate) fn parse_text_stem(name: &str) -> Option<Self>
    {
        Some(match name
        {
            "add" => Self::Add,
            "sub" => Self::Sub,
            "mul" => Self::Mul,
            "div" => Self::Div,
            "rem" => Self::Rem,
            _ => return None,
        })
    }
}

impl FloatComparisonOperation
{
    #[must_use]
    pub(crate) const fn text_stem(self) -> &'static str
    {
        match self
        {
            Self::Equal => "eq",
            Self::NotEqual => "ne",
            Self::Less => "lt",
            Self::LessEqual => "le",
            Self::Greater => "gt",
            Self::GreaterEqual => "ge",
        }
    }

    #[must_use]
    pub(crate) fn parse_text_stem(name: &str) -> Option<Self>
    {
        Some(match name
        {
            "eq" => Self::Equal,
            "ne" => Self::NotEqual,
            "lt" => Self::Less,
            "le" => Self::LessEqual,
            "gt" => Self::Greater,
            "ge" => Self::GreaterEqual,
            _ => return None,
        })
    }
}

impl I64ComparisonOperation
{
    #[must_use]
    pub(crate) const fn text_name(self) -> &'static str
    {
        match self
        {
            Self::Less => "lt.i64",
            Self::LessEqual => "le.i64",
            Self::Greater => "gt.i64",
            Self::GreaterEqual => "ge.i64",
        }
    }

    #[must_use]
    pub(crate) fn parse_text(name: &str) -> Option<Self>
    {
        Some(match name
        {
            "lt.i64" => Self::Less,
            "le.i64" => Self::LessEqual,
            "gt.i64" => Self::Greater,
            "ge.i64" => Self::GreaterEqual,
            _ => return None,
        })
    }
}

impl I32BinaryOperation
{
    #[must_use]
    pub(crate) const fn text_name(self) -> &'static str
    {
        match self
        {
            Self::Div => "div.i32",
            Self::Rem => "rem.i32",
            Self::BitAnd => "and.i32",
            Self::BitOr => "or.i32",
            Self::BitXor => "xor.i32",
            Self::ShiftLeft => "shl.i32",
            Self::ShiftRight => "shr.i32",
        }
    }

    #[must_use]
    pub(crate) fn parse_text(name: &str) -> Option<Self>
    {
        Some(match name
        {
            "div.i32" => Self::Div,
            "rem.i32" => Self::Rem,
            "and.i32" => Self::BitAnd,
            "or.i32" => Self::BitOr,
            "xor.i32" => Self::BitXor,
            "shl.i32" => Self::ShiftLeft,
            "shr.i32" => Self::ShiftRight,
            _ => return None,
        })
    }
}

use super::{BlockId, Function, Instruction, Type, Value, ValueId};

impl Function
{
    pub(crate) fn binary_integer(
        &mut self,
        block: BlockId,
        operation: IntegerBinaryOperation,
        value_type: Type,
        left: ValueId,
        right: ValueId,
    ) -> Option<ValueId>
    {
        self.block(block)?;
        if !value_type.is_integer() ||
            !self.value(left).is_some_and(|value| value.value_type == value_type) ||
            !self.value(right).is_some_and(|value| value.value_type == value_type)
        {
            return None;
        }
        let result_type = if operation.is_comparison()
        {
            Type::BOOL
        }
        else
        {
            value_type
        };
        let result = ValueId(self.values.len() as u32);
        self.values.push(Value {
            id: result,
            value_type: result_type,
        });
        self.block_mut(block)?.instructions.push(Instruction::BinaryInteger {
            operation,
            value_type,
            result,
            left,
            right,
        });
        Some(result)
    }

    pub(crate) fn binary_float(
        &mut self,
        block: BlockId,
        operation: FloatBinaryOperation,
        value_type: Type,
        left: ValueId,
        right: ValueId,
    ) -> Option<ValueId>
    {
        self.add_float_operation(block, value_type, value_type, left, right, |result| {
            Instruction::BinaryFloat {
                operation,
                value_type,
                result,
                left,
                right,
            }
        })
    }

    pub(crate) fn compare_float(
        &mut self,
        block: BlockId,
        operation: FloatComparisonOperation,
        value_type: Type,
        left: ValueId,
        right: ValueId,
    ) -> Option<ValueId>
    {
        self.add_float_operation(block, value_type, Type::BOOL, left, right, |result| {
            Instruction::CompareFloat {
                operation,
                value_type,
                result,
                left,
                right,
            }
        })
    }

    pub(crate) fn compare_str(
        &mut self,
        block: BlockId,
        operation: StrComparisonOperation,
        left: ValueId,
        right: ValueId,
    ) -> Option<ValueId>
    {
        self.block(block)?;
        if !self.value(left).is_some_and(|value| value.value_type == Type::STR) ||
            !self.value(right).is_some_and(|value| value.value_type == Type::STR)
        {
            return None;
        }
        let result = ValueId(self.values.len() as u32);
        self.values.push(Value {
            id: result,
            value_type: Type::BOOL,
        });
        self.block_mut(block)?.instructions.push(Instruction::CompareStr {
            operation,
            result,
            left,
            right,
        });
        Some(result)
    }

    fn add_float_operation(
        &mut self,
        block: BlockId,
        operand_type: Type,
        result_type: Type,
        left: ValueId,
        right: ValueId,
        instruction: impl FnOnce(ValueId) -> Instruction,
    ) -> Option<ValueId>
    {
        self.block(block)?;
        if !matches!(operand_type, Type::F32 | Type::F64) ||
            !self.value(left).is_some_and(|value| value.value_type == operand_type) ||
            !self.value(right).is_some_and(|value| value.value_type == operand_type)
        {
            return None;
        }
        let result = ValueId(self.values.len() as u32);
        self.values.push(Value {
            id: result,
            value_type: result_type,
        });
        self.block_mut(block)?.instructions.push(instruction(result));
        Some(result)
    }

    pub(crate) fn binary_i64(
        &mut self,
        block: BlockId,
        operation: I64BinaryOperation,
        left: ValueId,
        right: ValueId,
    ) -> Option<ValueId>
    {
        self.add_i64_operation(block, left, right, Type::I64, |result| Instruction::BinaryI64 {
            operation,
            result,
            left,
            right,
        })
    }

    pub(crate) fn compare_i64(
        &mut self,
        block: BlockId,
        operation: I64ComparisonOperation,
        left: ValueId,
        right: ValueId,
    ) -> Option<ValueId>
    {
        self.add_i64_operation(block, left, right, Type::BOOL, |result| Instruction::CompareI64 {
            operation,
            result,
            left,
            right,
        })
    }

    fn add_i64_operation(
        &mut self,
        block: BlockId,
        left: ValueId,
        right: ValueId,
        result_type: Type,
        instruction: impl FnOnce(ValueId) -> Instruction,
    ) -> Option<ValueId>
    {
        self.block(block)?;
        if !self.value(left).is_some_and(|value| value.value_type == Type::I64) ||
            !self.value(right).is_some_and(|value| value.value_type == Type::I64)
        {
            return None;
        }
        let result = ValueId(self.values.len() as u32);
        self.values.push(Value {
            id: result,
            value_type: result_type,
        });
        self.block_mut(block)?.instructions.push(instruction(result));
        Some(result)
    }

    pub(crate) fn not_bool(&mut self, block: BlockId, operand: ValueId) -> Option<ValueId>
    {
        self.block(block)?;
        if !self.value(operand).is_some_and(|value| value.value_type == Type::BOOL)
        {
            return None;
        }
        let result = ValueId(self.values.len() as u32);
        self.values.push(Value {
            id: result,
            value_type: Type::BOOL,
        });
        self.block_mut(block)?.instructions.push(Instruction::NotBool {
            result,
            operand,
        });
        Some(result)
    }
}
