/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use crate::xlil::{
    BlockId, Builder, FloatBinaryOperation, FloatComparisonOperation, IntegerBinaryOperation, IntegerConstant, Module,
    StrComparisonOperation, Type, Utf32Encoding,
    types::{AnyValue, FloatType, IntegerType, Utf32Builder, Value, XlilType},
};

use super::{Signature, Slot, TypedBuildError};

/// Type-checked facade over the canonical raw XLIL [`Builder`].
#[derive(Clone, Debug)]
pub struct TypedBuilder
{
    raw: Builder,
    current_return_type: Option<Type>,
}

impl TypedBuilder
{
    /// Creates an empty typed producer for one XLIL module.
    #[must_use]
    pub fn new(module_name: impl Into<String>) -> Self
    {
        Self {
            raw: Builder::new(module_name),
            current_return_type: None,
        }
    }

    /// Wraps a raw builder and reconstructs its current result type.
    #[must_use]
    pub fn from_raw(raw: Builder) -> Self
    {
        let current_return_type = raw.current_function().map(|function| function.return_type);
        Self {
            raw,
            current_return_type,
        }
    }

    /// Returns read-only access to the canonical raw builder.
    #[must_use]
    pub const fn raw(&self) -> &Builder
    {
        &self.raw
    }

    /// Consumes the facade without finishing or verifying its module.
    #[must_use]
    pub fn into_raw(self) -> Builder
    {
        self.raw
    }

    /// Adds a body-less function declaration.
    pub fn declare(&mut self, signature: Signature) -> Result<(), TypedBuildError>
    {
        let (name, return_type, parameters) = signature.into_parts();
        self.raw.declare_function(name, return_type, parameters)?;
        Ok(())
    }

    /// Begins a function definition and records its typed return contract.
    pub fn begin(&mut self, signature: Signature) -> Result<(), TypedBuildError>
    {
        let (name, return_type, parameters) = signature.into_parts();
        self.raw.begin_function(name, return_type, parameters)?;
        self.current_return_type = Some(return_type);
        Ok(())
    }

    /// Ends the current definition.
    pub fn end(&mut self) -> Result<(), TypedBuildError>
    {
        self.raw.end_function()?;
        self.current_return_type = None;
        Ok(())
    }

    /// Appends and selects a basic block.
    pub fn append_block(&mut self, label: impl Into<String>) -> Result<BlockId, TypedBuildError>
    {
        Ok(self.raw.append_block(label)?)
    }

    /// Selects an existing basic block as insertion point.
    pub fn position_at_end(&mut self, block: BlockId) -> Result<(), TypedBuildError>
    {
        self.raw.position_at_end(block)?;
        Ok(())
    }

    /// Returns a parameter after checking its signature type against `T`.
    pub fn parameter<T: XlilType>(&self, index: usize) -> Result<Value<T>, TypedBuildError>
    {
        let function = self.raw.current_function().ok_or(TypedBuildError::NoCurrentFunction)?;
        let Some(actual) = function.parameters.get(index).copied()
        else
        {
            return Err(TypedBuildError::ParameterOutOfRange {
                index,
                count: function.parameters.len(),
            });
        };
        ensure_type("parameter", T::XLIL_TYPE, actual)?;
        Ok(Value::trusted(self.raw.parameter(index)?))
    }

    /// Emits a verified boolean constant.
    pub fn const_bool(&mut self, value: bool) -> Result<Value<bool>, TypedBuildError>
    {
        Ok(Value::trusted(self.raw.const_bool(value)?))
    }

    /// Emits an `i32` constant.
    pub fn const_i32(&mut self, value: i32) -> Result<Value<i32>, TypedBuildError>
    {
        Ok(Value::trusted(self.raw.const_i32(value)?))
    }

    /// Emits an `i64` constant.
    pub fn const_i64(&mut self, value: i64) -> Result<Value<i64>, TypedBuildError>
    {
        Ok(Value::trusted(self.raw.const_i64(value)?))
    }

    /// Emits a width-checked integer bit pattern.
    pub fn const_integer<T: IntegerType>(&mut self, bits: u128) -> Result<Value<T>, TypedBuildError>
    {
        let constant = IntegerConstant::new(T::XLIL_TYPE, bits).ok_or(TypedBuildError::IntegerConstantOutOfRange {
            value_type: T::XLIL_TYPE,
            bits,
        })?;
        Ok(Value::trusted(self.raw.const_integer(constant)?))
    }

    /// Emits an `f32` constant without decimal conversion in XLIL.
    pub fn const_f32(&mut self, value: f32) -> Result<Value<f32>, TypedBuildError>
    {
        Ok(Value::trusted(self.raw.const_f32_bits(value.to_bits())?))
    }

    /// Emits an `f64` constant without decimal conversion in XLIL.
    pub fn const_f64(&mut self, value: f64) -> Result<Value<f64>, TypedBuildError>
    {
        Ok(Value::trusted(self.raw.const_f64_bits(value.to_bits())?))
    }

    /// Emits a UTF-32 constant prepared by [`Utf32Builder`].
    pub fn const_utf32(&mut self, value: &Utf32Builder) -> Result<AnyValue, TypedBuildError>
    {
        let id = value.emit(&mut self.raw)?;
        Ok(AnyValue::new(id, Type::STR))
    }

    /// Emits UTF-32 code points using an explicit byte order.
    pub fn const_utf32_units(
        &mut self,
        encoding: Utf32Encoding,
        code_points: impl Into<Vec<u32>>,
    ) -> Result<AnyValue, TypedBuildError>
    {
        let id = self.raw.const_str(encoding, code_points)?;
        Ok(AnyValue::new(id, Type::STR))
    }

    /// Emits exact-width integer arithmetic or bitwise work.
    pub fn integer<T: IntegerType>(
        &mut self,
        operation: IntegerBinaryOperation,
        left: Value<T>,
        right: Value<T>,
    ) -> Result<Value<T>, TypedBuildError>
    {
        if operation.is_comparison()
        {
            return Err(TypedBuildError::TypeMismatch {
                operation: "integer arithmetic result",
                expected: T::XLIL_TYPE,
                actual: Type::BOOL,
            });
        }
        let id = self
            .raw
            .binary_integer(operation, T::XLIL_TYPE, left.id(), right.id())?;
        Ok(Value::trusted(id))
    }

    /// Emits an exact-width integer comparison.
    pub fn compare_integer<T: IntegerType>(
        &mut self,
        operation: IntegerBinaryOperation,
        left: Value<T>,
        right: Value<T>,
    ) -> Result<Value<bool>, TypedBuildError>
    {
        if !operation.is_comparison()
        {
            return Err(TypedBuildError::TypeMismatch {
                operation: "integer comparison result",
                expected: Type::BOOL,
                actual: T::XLIL_TYPE,
            });
        }
        let id = self
            .raw
            .binary_integer(operation, T::XLIL_TYPE, left.id(), right.id())?;
        Ok(Value::trusted(id))
    }

    /// Emits `f32` or `f64` arithmetic.
    pub fn float<T: FloatType>(
        &mut self,
        operation: FloatBinaryOperation,
        left: Value<T>,
        right: Value<T>,
    ) -> Result<Value<T>, TypedBuildError>
    {
        ensure_arithmetic_float(T::XLIL_TYPE)?;
        let id = self.raw.binary_float(operation, T::XLIL_TYPE, left.id(), right.id())?;
        Ok(Value::trusted(id))
    }

    /// Emits an ordered `f32` or `f64` comparison.
    pub fn compare_float<T: FloatType>(
        &mut self,
        operation: FloatComparisonOperation,
        left: Value<T>,
        right: Value<T>,
    ) -> Result<Value<bool>, TypedBuildError>
    {
        ensure_arithmetic_float(T::XLIL_TYPE)?;
        let id = self.raw.compare_float(operation, T::XLIL_TYPE, left.id(), right.id())?;
        Ok(Value::trusted(id))
    }

    /// Emits boolean negation.
    pub fn not(&mut self, operand: Value<bool>) -> Result<Value<bool>, TypedBuildError>
    {
        Ok(Value::trusted(self.raw.not_bool(operand.id())?))
    }

    /// Emits UTF-32 equality or inequality.
    pub fn compare_utf32(
        &mut self,
        operation: StrComparisonOperation,
        left: AnyValue,
        right: AnyValue,
    ) -> Result<Value<bool>, TypedBuildError>
    {
        ensure_type("UTF-32 comparison", Type::STR, left.value_type())?;
        ensure_type("UTF-32 comparison", Type::STR, right.value_type())?;
        Ok(Value::trusted(self.raw.compare_str(
            operation,
            left.id(),
            right.id(),
        )?))
    }

    /// Allocates a stack slot carrying `T`.
    pub fn add_slot<T: XlilType>(&mut self) -> Result<Slot<T>, TypedBuildError>
    {
        Ok(Slot::trusted(self.raw.add_slot(T::XLIL_TYPE)?))
    }

    /// Loads a typed stack slot.
    pub fn load<T: XlilType>(&mut self, slot: Slot<T>) -> Result<Value<T>, TypedBuildError>
    {
        Ok(Value::trusted(self.raw.load(slot.id())?))
    }

    /// Stores a same-typed value into a stack slot.
    pub fn store<T: XlilType>(&mut self, slot: Slot<T>, value: Value<T>) -> Result<(), TypedBuildError>
    {
        self.raw.store(slot.id(), value.id())?;
        Ok(())
    }

    /// Emits a checked call and restores its expected Rust result marker.
    pub fn call<T: XlilType>(
        &mut self,
        function: impl Into<String>,
        arguments: impl IntoIterator<Item = AnyValue>,
    ) -> Result<Value<T>, TypedBuildError>
    {
        let function = function.into();
        let actual = self
            .raw
            .module()
            .function(&function)
            .map(|callee| callee.return_type)
            .ok_or_else(|| crate::xlil::BuildError::UnknownFunction(function.clone()))?;
        ensure_type("call result", T::XLIL_TYPE, actual)?;
        let arguments = arguments.into_iter().map(AnyValue::id).collect();
        let Some(result) = self.raw.call_checked(function.clone(), arguments)?
        else
        {
            return Err(TypedBuildError::MissingCallResult {
                function,
            });
        };
        Ok(Value::trusted(result))
    }

    /// Emits a checked call to a void function.
    pub fn call_void(
        &mut self,
        function: impl Into<String>,
        arguments: impl IntoIterator<Item = AnyValue>,
    ) -> Result<(), TypedBuildError>
    {
        let function = function.into();
        let actual = self
            .raw
            .module()
            .function(&function)
            .map(|callee| callee.return_type)
            .ok_or_else(|| crate::xlil::BuildError::UnknownFunction(function.clone()))?;
        if actual != Type::VOID
        {
            return Err(TypedBuildError::UnexpectedCallResult {
                function,
                actual,
            });
        }
        self.raw
            .call_void(function, arguments.into_iter().map(AnyValue::id).collect())?;
        Ok(())
    }

    /// Terminates the current block with a typed value return.
    pub fn return_value<T: XlilType>(&mut self, value: Value<T>) -> Result<(), TypedBuildError>
    {
        let actual = self.current_return_type.ok_or(TypedBuildError::NoCurrentFunction)?;
        ensure_type("return", T::XLIL_TYPE, actual)?;
        self.raw.return_value(Some(value.id()))?;
        Ok(())
    }

    /// Terminates the current block with a void return.
    pub fn return_void(&mut self) -> Result<(), TypedBuildError>
    {
        let actual = self.current_return_type.ok_or(TypedBuildError::NoCurrentFunction)?;
        ensure_type("void return", Type::VOID, actual)?;
        self.raw.return_void()?;
        Ok(())
    }

    /// Terminates the current block with an unconditional branch.
    pub fn branch(&mut self, target: BlockId) -> Result<(), TypedBuildError>
    {
        self.raw.branch(target)?;
        Ok(())
    }

    /// Terminates the current block with a typed boolean branch.
    pub fn branch_if(
        &mut self,
        condition: Value<bool>,
        then_block: BlockId,
        else_block: BlockId,
    ) -> Result<(), TypedBuildError>
    {
        self.raw.branch_if(condition.id(), then_block, else_block)?;
        Ok(())
    }

    /// Terminates the current block with an explicit panic.
    pub fn panic(&mut self) -> Result<(), TypedBuildError>
    {
        self.raw.panic()?;
        Ok(())
    }

    /// Verifies and returns the canonical XLIL module.
    pub fn finish(self) -> Result<Module, TypedBuildError>
    {
        Ok(self.raw.finish()?)
    }
}

fn ensure_type(operation: &'static str, expected: Type, actual: Type) -> Result<(), TypedBuildError>
{
    if expected == actual
    {
        Ok(())
    }
    else
    {
        Err(TypedBuildError::TypeMismatch {
            operation,
            expected,
            actual,
        })
    }
}

fn ensure_arithmetic_float(value_type: Type) -> Result<(), TypedBuildError>
{
    if matches!(value_type, Type::F32 | Type::F64)
    {
        Ok(())
    }
    else
    {
        Err(TypedBuildError::TypeMismatch {
            operation: "floating arithmetic",
            expected: Type::F32,
            actual: value_type,
        })
    }
}
