/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

use crate::xlil::{
    FloatBinaryOperation, FloatComparisonOperation, I32BinaryOperation, I64BinaryOperation, I64ComparisonOperation,
    IntegerBinaryOperation,
};

use super::{IntegerConstant, SlotId, Type, Utf32Encoding, ValueId};

/// Executable XLIL v0 instruction record.
#[derive(Clone, Debug, Eq, PartialEq)]
pub enum Instruction
{
    /// Produces an `i64` constant.
    ConstI64
    {
        /// Result register.
        result: ValueId,
        /// Signed constant value.
        value: i64,
    },
    /// Produces an `i32` constant.
    ConstI32
    {
        /// Result register.
        result: ValueId,
        /// Signed constant value.
        value: i32,
    },
    /// Produces a `u16` constant.
    ConstU16
    {
        /// Result register.
        result: ValueId,
        /// Unsigned constant value.
        value: u16,
    },
    /// Produces an arbitrary supported integer-width constant.
    ConstInteger
    {
        /// Result register.
        result: ValueId,
        /// Exact-width integer bit pattern.
        value: IntegerConstant,
    },
    /// Produces an `f32` from IEEE bits.
    ConstF32
    {
        /// Result register.
        result: ValueId,
        /// IEEE-754 bit pattern.
        bits: u32,
    },
    /// Produces an `f64` from IEEE bits.
    ConstF64
    {
        /// Result register.
        result: ValueId,
        /// IEEE-754 bit pattern.
        bits: u64,
    },
    /// Produces a UTF-32 string registry value.
    ConstStr
    {
        /// Result register.
        result: ValueId,
        /// Source code-point byte order.
        encoding: Utf32Encoding,
        /// Unicode scalar-value code points.
        units: Vec<u32>,
    },
    /// Produces a verified boolean.
    ConstBool
    {
        /// Result register.
        result: ValueId,
        /// Logical constant value.
        value: bool,
    },
    /// Performs exact-width integer arithmetic, bitwise work, shifts, or comparison.
    BinaryInteger
    {
        /// Operation selector.
        operation: IntegerBinaryOperation,
        /// Operand type.
        value_type: Type,
        /// Result register.
        result: ValueId,
        /// Left operand.
        left: ValueId,
        /// Right operand.
        right: ValueId,
    },
    /// Performs floating-point arithmetic.
    BinaryFloat
    {
        /// Operation selector.
        operation: FloatBinaryOperation,
        /// Operand and result type.
        value_type: Type,
        /// Result register.
        result: ValueId,
        /// Left operand.
        left: ValueId,
        /// Right operand.
        right: ValueId,
    },
    /// Compares two floating-point values.
    CompareFloat
    {
        /// Comparison selector.
        operation: FloatComparisonOperation,
        /// Operand type.
        value_type: Type,
        /// Boolean result register.
        result: ValueId,
        /// Left operand.
        left: ValueId,
        /// Right operand.
        right: ValueId,
    },
    /// Compares two UTF-32 string values.
    CompareStr
    {
        /// Equality selector.
        operation: crate::xlil::StrComparisonOperation,
        /// Boolean result register.
        result: ValueId,
        /// Left string.
        left: ValueId,
        /// Right string.
        right: ValueId,
    },
    /// Adds two legacy `i64` values.
    AddI64
    {
        /// Result register.
        result: ValueId,
        /// Left operand.
        left: ValueId,
        /// Right operand.
        right: ValueId,
    },
    /// Subtracts two legacy `i64` values.
    SubI64
    {
        /// Result register.
        result: ValueId,
        /// Left operand.
        left: ValueId,
        /// Right operand.
        right: ValueId,
    },
    /// Multiplies two legacy `i64` values.
    MulI64
    {
        /// Result register.
        result: ValueId,
        /// Left operand.
        left: ValueId,
        /// Right operand.
        right: ValueId,
    },
    /// Compares two legacy `i64` values for equality.
    EqI64
    {
        /// Boolean result register.
        result: ValueId,
        /// Left operand.
        left: ValueId,
        /// Right operand.
        right: ValueId,
    },
    /// Performs a legacy `i64` operation.
    BinaryI64
    {
        /// Operation selector.
        operation: I64BinaryOperation,
        /// Result register.
        result: ValueId,
        /// Left operand.
        left: ValueId,
        /// Right operand.
        right: ValueId,
    },
    /// Performs a legacy ordered `i64` comparison.
    CompareI64
    {
        /// Comparison selector.
        operation: I64ComparisonOperation,
        /// Boolean result register.
        result: ValueId,
        /// Left operand.
        left: ValueId,
        /// Right operand.
        right: ValueId,
    },
    /// Adds two legacy `i32` values.
    AddI32
    {
        /// Result register.
        result: ValueId,
        /// Left operand.
        left: ValueId,
        /// Right operand.
        right: ValueId,
    },
    /// Subtracts two legacy `i32` values.
    SubI32
    {
        /// Result register.
        result: ValueId,
        /// Left operand.
        left: ValueId,
        /// Right operand.
        right: ValueId,
    },
    /// Multiplies two legacy `i32` values.
    MulI32
    {
        /// Result register.
        result: ValueId,
        /// Left operand.
        left: ValueId,
        /// Right operand.
        right: ValueId,
    },
    /// Performs a legacy `i32` operation.
    BinaryI32
    {
        /// Operation selector.
        operation: I32BinaryOperation,
        /// Result register.
        result: ValueId,
        /// Left operand.
        left: ValueId,
        /// Right operand.
        right: ValueId,
    },
    /// Tests legacy `i32` equality.
    EqI32
    {
        /// Boolean result register.
        result: ValueId,
        /// Left operand.
        left: ValueId,
        /// Right operand.
        right: ValueId,
    },
    /// Tests signed legacy `i32` less-than.
    LtI32
    {
        /// Boolean result register.
        result: ValueId,
        /// Left operand.
        left: ValueId,
        /// Right operand.
        right: ValueId,
    },
    /// Tests signed legacy `i32` less-than-or-equal.
    LeI32
    {
        /// Boolean result register.
        result: ValueId,
        /// Left operand.
        left: ValueId,
        /// Right operand.
        right: ValueId,
    },
    /// Tests signed legacy `i32` greater-than.
    GtI32
    {
        /// Boolean result register.
        result: ValueId,
        /// Left operand.
        left: ValueId,
        /// Right operand.
        right: ValueId,
    },
    /// Tests signed legacy `i32` greater-than-or-equal.
    GeI32
    {
        /// Boolean result register.
        result: ValueId,
        /// Left operand.
        left: ValueId,
        /// Right operand.
        right: ValueId,
    },
    /// Negates a boolean value.
    NotBool
    {
        /// Boolean result register.
        result: ValueId,
        /// Boolean operand.
        operand: ValueId,
    },
    /// Calls a module-registry function directly.
    Call
    {
        /// Optional result register.
        result: Option<ValueId>,
        /// Callee symbol name.
        function: String,
        /// Argument registers.
        arguments: Vec<ValueId>,
        /// Declared result type.
        return_type: Type,
    },
    /// Constructs an aggregate or array value.
    Aggregate
    {
        /// Result register.
        result: ValueId,
        /// Aggregate or array registry type.
        value_type: Type,
        /// Field or element registers.
        fields: Vec<ValueId>,
    },
    /// Extracts an aggregate field or fixed-array element.
    Extract
    {
        /// Result register.
        result: ValueId,
        /// Source aggregate.
        aggregate: ValueId,
        /// Constant field index.
        field: u32,
    },
    /// Reads a runtime-indexed array element.
    ArrayGet
    {
        /// Result register.
        result: ValueId,
        /// Array register.
        array: ValueId,
        /// Runtime index register.
        index: ValueId,
    },
    /// Replaces a runtime-indexed array element.
    ArraySet
    {
        /// Result array register.
        result: ValueId,
        /// Input array register.
        array: ValueId,
        /// Runtime index register.
        index: ValueId,
        /// Replacement value register.
        value: ValueId,
    },
    /// Reads a runtime array length.
    ArrayLength
    {
        /// Length result register.
        result: ValueId,
        /// Array register.
        array: ValueId,
    },
    /// Loads a stack slot.
    Load
    {
        /// Result register.
        result: ValueId,
        /// Source stack slot.
        slot: SlotId,
    },
    /// Stores a value into a stack slot.
    Store
    {
        /// Destination stack slot.
        slot: SlotId,
        /// Source value register.
        value: ValueId,
    },
}
