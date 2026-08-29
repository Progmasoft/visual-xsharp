/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

use std::{error::Error, fmt};

use super::{
    BlockId, FloatBinaryOperation, FloatComparisonOperation, Function, I32BinaryOperation, IntegerBinaryOperation,
    IntegerConstant, Module, SlotId, StrComparisonOperation, Type, Utf32Encoding, ValueId, verify,
};

/// Failure produced while incrementally constructing or finishing a module.
#[derive(Clone, Debug, Eq, PartialEq)]
pub enum BuildError
{
    /// A declaration or definition reused an existing module-level name.
    DuplicateFunction(String),
    /// A checked call named no function in the module registry.
    UnknownFunction(String),
    /// A checked call supplied a different number of arguments than its signature.
    ArgumentCount
    {
        /// Called function name.
        function: String,
        /// Signature arity.
        expected: usize,
        /// Supplied arity.
        actual: usize,
    },
    /// A checked call supplied an unknown or incorrectly typed argument register.
    ArgumentType
    {
        /// Called function name.
        function: String,
        /// Zero-based argument position.
        index: usize,
        /// Signature parameter type.
        expected: Type,
        /// Actual type, or `None` when the register does not exist.
        actual: Option<Type>,
    },
    /// A value-producing helper was used for a void function, or conversely.
    UnexpectedCallResult
    {
        /// Called function name.
        function: String,
        /// Registered return type.
        return_type: Type,
    },
    /// An instruction was requested before beginning a function definition.
    NoCurrentFunction,
    /// An instruction was requested before selecting a basic block.
    NoInsertionBlock,
    /// The requested operation violated a local builder invariant.
    InvalidOperation(&'static str),
    /// Final module verification reported one or more diagnostics.
    Verification(Vec<verify::Diagnostic>),
}

impl fmt::Display for BuildError
{
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result
    {
        match self
        {
            Self::DuplicateFunction(name) => write!(formatter, "XLIL function '{name}' already exists"),
            Self::UnknownFunction(name) => write!(formatter, "XLIL function '{name}' does not exist"),
            Self::ArgumentCount {
                function,
                expected,
                actual,
            } => write!(
                formatter,
                "XLIL call to '{function}' expects {expected} argument(s), got {actual}"
            ),
            Self::ArgumentType {
                function,
                index,
                expected,
                actual,
            } => write!(
                formatter,
                "XLIL call to '{function}' argument {index} expects {expected:?}, got {actual:?}"
            ),
            Self::UnexpectedCallResult {
                function,
                return_type,
            } => write!(
                formatter,
                "XLIL call to '{function}' has incompatible result type {return_type:?}"
            ),
            Self::NoCurrentFunction => formatter.write_str("XLIL builder has no current function"),
            Self::NoInsertionBlock => formatter.write_str("XLIL builder has no insertion block"),
            Self::InvalidOperation(operation) => write!(formatter, "XLIL builder rejected {operation}"),
            Self::Verification(diagnostics) => write!(
                formatter,
                "XLIL module verification failed with {} diagnostic(s)",
                diagnostics.len()
            ),
        }
    }
}

impl Error for BuildError {}

/// Stateful XLIL producer with a current function and insertion block.
///
/// Identifiers are allocated sequentially. Local misuse is rejected immediately,
/// and [`Self::finish`] performs whole-module verification.
#[derive(Clone, Debug)]
pub struct Builder
{
    module: Module,
    current_function: Option<usize>,
    insertion_block: Option<BlockId>,
}

impl Builder
{
    /// Creates an empty XLIL v0 module builder.
    #[must_use]
    pub fn new(module_name: impl Into<String>) -> Self
    {
        Self {
            module: Module::new(module_name),
            current_function: None,
            insertion_block: None,
        }
    }

    /// Returns the module assembled so far.
    #[must_use]
    pub const fn module(&self) -> &Module
    {
        &self.module
    }

    /// Returns mutable model access for specialized producers.
    ///
    /// [`Self::finish`] verifies invariants after direct changes.
    pub fn module_mut(&mut self) -> &mut Module
    {
        &mut self.module
    }

    /// Registers a named aggregate layout.
    pub fn add_aggregate_type(&mut self, name: impl Into<String>, fields: Vec<Type>) -> Result<Type, BuildError>
    {
        self.module
            .add_aggregate_type(name, fields)
            .ok_or(BuildError::InvalidOperation("aggregate type declaration"))
    }

    /// Registers a fixed-length array layout.
    pub fn add_array_type(&mut self, element_type: Type, length: u64) -> Result<Type, BuildError>
    {
        self.module
            .add_array_type(element_type, length)
            .ok_or(BuildError::InvalidOperation("fixed array type declaration"))
    }

    /// Registers a runtime-length array layout.
    pub fn add_dynamic_array_type(&mut self, element_type: Type) -> Result<Type, BuildError>
    {
        self.module
            .add_dynamic_array_type(element_type)
            .ok_or(BuildError::InvalidOperation("dynamic array type declaration"))
    }

    /// Adds a body-less function declaration.
    pub fn declare_function(
        &mut self,
        name: impl Into<String>,
        return_type: Type,
        parameters: Vec<Type>,
    ) -> Result<(), BuildError>
    {
        let name = name.into();
        self.reject_duplicate(&name)?;
        self.module
            .add_function(Function::declaration(name, return_type, parameters));
        Ok(())
    }

    /// Adds a definition and makes it the current function.
    pub fn begin_function(
        &mut self,
        name: impl Into<String>,
        return_type: Type,
        parameters: Vec<Type>,
    ) -> Result<(), BuildError>
    {
        let name = name.into();
        self.reject_duplicate(&name)?;
        self.module
            .add_function(Function::definition(name, return_type, parameters));
        self.current_function = Some(self.module.functions.len() - 1);
        self.insertion_block = None;
        Ok(())
    }

    /// Ends the current function and clears its insertion point.
    pub fn end_function(&mut self) -> Result<(), BuildError>
    {
        self.function()?;
        self.current_function = None;
        self.insertion_block = None;
        Ok(())
    }

    /// Returns the current function, when a definition is open.
    #[must_use]
    pub fn current_function(&self) -> Option<&Function>
    {
        self.current_function.and_then(|index| self.module.functions.get(index))
    }

    /// Returns the selected insertion block identifier.
    #[must_use]
    pub const fn insertion_block(&self) -> Option<BlockId>
    {
        self.insertion_block
    }

    /// Appends a block and selects it as the insertion block.
    pub fn append_block(&mut self, label: impl Into<String>) -> Result<BlockId, BuildError>
    {
        let block = self.function_mut()?.append_block(label);
        self.insertion_block = Some(block);
        Ok(block)
    }

    /// Returns the value register assigned to a current-function parameter.
    pub fn parameter(&self, index: usize) -> Result<ValueId, BuildError>
    {
        self.function()?
            .parameter_value(index)
            .ok_or(BuildError::InvalidOperation("parameter lookup"))
    }

    /// Selects an existing current-function block for subsequent instructions.
    pub fn position_at_end(&mut self, block: BlockId) -> Result<(), BuildError>
    {
        let exists = self.function()?.blocks.iter().any(|candidate| candidate.id == block);
        if !exists
        {
            return Err(BuildError::InvalidOperation("position_at_end"));
        }
        self.insertion_block = Some(block);
        Ok(())
    }

    /// Allocates a typed stack slot.
    pub fn add_slot(&mut self, value_type: Type) -> Result<SlotId, BuildError>
    {
        self.function_mut()?
            .add_slot(value_type)
            .ok_or(BuildError::InvalidOperation("add_slot"))
    }

    /// Appends an `i32` constant.
    pub fn const_i32(&mut self, value: i32) -> Result<ValueId, BuildError>
    {
        let block = self.block()?;
        self.function_mut()?
            .add_const_i32(block, value)
            .ok_or(BuildError::InvalidOperation("const.i32"))
    }

    /// Appends an `i64` constant.
    pub fn const_i64(&mut self, value: i64) -> Result<ValueId, BuildError>
    {
        let block = self.block()?;
        self.function_mut()?
            .add_const_i64(block, value)
            .ok_or(BuildError::InvalidOperation("const.i64"))
    }

    /// Appends a `u16` constant.
    pub fn const_u16(&mut self, value: u16) -> Result<ValueId, BuildError>
    {
        let block = self.block()?;
        self.function_mut()?
            .add_const_u16(block, value)
            .ok_or(BuildError::InvalidOperation("const.u16"))
    }

    /// Appends an exact-width integer constant.
    pub fn const_integer(&mut self, value: IntegerConstant) -> Result<ValueId, BuildError>
    {
        let block = self.block()?;
        self.function_mut()?
            .add_const_integer(block, value)
            .ok_or(BuildError::InvalidOperation("integer constant"))
    }

    /// Appends a verified boolean constant.
    pub fn const_bool(&mut self, value: bool) -> Result<ValueId, BuildError>
    {
        let block = self.block()?;
        self.function_mut()?
            .add_const_bool(block, value)
            .ok_or(BuildError::InvalidOperation("const.bool"))
    }

    /// Appends an `f32` constant from its IEEE bit pattern.
    pub fn const_f32_bits(&mut self, bits: u32) -> Result<ValueId, BuildError>
    {
        let block = self.block()?;
        self.function_mut()?
            .add_const_f32_bits(block, bits)
            .ok_or(BuildError::InvalidOperation("const.f32"))
    }

    /// Appends an `f64` constant from its IEEE bit pattern.
    pub fn const_f64_bits(&mut self, bits: u64) -> Result<ValueId, BuildError>
    {
        let block = self.block()?;
        self.function_mut()?
            .add_const_f64_bits(block, bits)
            .ok_or(BuildError::InvalidOperation("const.f64"))
    }

    /// Appends a UTF-32 string constant.
    pub fn const_str(&mut self, encoding: Utf32Encoding, units: impl Into<Vec<u32>>) -> Result<ValueId, BuildError>
    {
        let block = self.block()?;
        self.function_mut()?
            .add_const_str(block, encoding, units)
            .ok_or(BuildError::InvalidOperation("const.str"))
    }

    /// Appends exact-width integer arithmetic, bitwise, shift, or comparison.
    pub fn binary_integer(
        &mut self,
        operation: IntegerBinaryOperation,
        value_type: Type,
        left: ValueId,
        right: ValueId,
    ) -> Result<ValueId, BuildError>
    {
        let block = self.block()?;
        self.function_mut()?
            .binary_integer(block, operation, value_type, left, right)
            .ok_or(BuildError::InvalidOperation("integer operation"))
    }

    /// Appends a legacy XLIL v0 `i32` operation.
    pub fn binary_i32(
        &mut self,
        operation: I32BinaryOperation,
        left: ValueId,
        right: ValueId,
    ) -> Result<ValueId, BuildError>
    {
        let block = self.block()?;
        self.function_mut()?
            .binary_i32(block, left, right, operation)
            .ok_or(BuildError::InvalidOperation("i32 operation"))
    }

    /// Appends an `f32` or `f64` arithmetic operation.
    pub fn binary_float(
        &mut self,
        operation: FloatBinaryOperation,
        value_type: Type,
        left: ValueId,
        right: ValueId,
    ) -> Result<ValueId, BuildError>
    {
        let block = self.block()?;
        self.function_mut()?
            .binary_float(block, operation, value_type, left, right)
            .ok_or(BuildError::InvalidOperation("float operation"))
    }

    /// Appends an ordered floating-point comparison.
    pub fn compare_float(
        &mut self,
        operation: FloatComparisonOperation,
        value_type: Type,
        left: ValueId,
        right: ValueId,
    ) -> Result<ValueId, BuildError>
    {
        let block = self.block()?;
        self.function_mut()?
            .compare_float(block, operation, value_type, left, right)
            .ok_or(BuildError::InvalidOperation("float comparison"))
    }

    /// Appends UTF-32 string equality or inequality.
    pub fn compare_str(
        &mut self,
        operation: StrComparisonOperation,
        left: ValueId,
        right: ValueId,
    ) -> Result<ValueId, BuildError>
    {
        let block = self.block()?;
        self.function_mut()?
            .compare_str(block, operation, left, right)
            .ok_or(BuildError::InvalidOperation("str comparison"))
    }

    /// Appends boolean negation.
    pub fn not_bool(&mut self, operand: ValueId) -> Result<ValueId, BuildError>
    {
        let block = self.block()?;
        self.function_mut()?
            .not_bool(block, operand)
            .ok_or(BuildError::InvalidOperation("not.bool"))
    }

    /// Appends a direct module-registry call.
    ///
    /// Void calls return `None`; value-producing calls return their register.
    pub fn call(
        &mut self,
        function: impl Into<String>,
        arguments: Vec<ValueId>,
        return_type: Type,
    ) -> Result<Option<ValueId>, BuildError>
    {
        let block = self.block()?;
        self.function_mut()?
            .add_call(block, function, arguments, return_type)
            .ok_or(BuildError::InvalidOperation("call"))
    }

    /// Appends a call whose arity, argument types, and result are derived from
    /// the registered callee signature.
    pub fn call_checked(
        &mut self,
        function: impl Into<String>,
        arguments: Vec<ValueId>,
    ) -> Result<Option<ValueId>, BuildError>
    {
        let function = function.into();
        let (parameters, return_type) = self
            .module
            .function(&function)
            .map(|callee| (callee.parameters.clone(), callee.return_type))
            .ok_or_else(|| BuildError::UnknownFunction(function.clone()))?;
        if parameters.len() != arguments.len()
        {
            return Err(BuildError::ArgumentCount {
                function,
                expected: parameters.len(),
                actual: arguments.len(),
            });
        }
        for (index, (argument, expected)) in arguments.iter().zip(parameters).enumerate()
        {
            let actual = self.function()?.value(*argument).map(|value| value.value_type);
            if actual != Some(expected)
            {
                return Err(BuildError::ArgumentType {
                    function,
                    index,
                    expected,
                    actual,
                });
            }
        }
        self.call(function, arguments, return_type)
    }

    /// Appends a checked value-producing call and returns its register.
    pub fn call_value(&mut self, function: impl Into<String>, arguments: Vec<ValueId>) -> Result<ValueId, BuildError>
    {
        let function = function.into();
        self.call_checked(function.clone(), arguments)?
            .ok_or(BuildError::UnexpectedCallResult {
                function,
                return_type: Type::VOID,
            })
    }

    /// Appends a checked void call.
    pub fn call_void(&mut self, function: impl Into<String>, arguments: Vec<ValueId>) -> Result<(), BuildError>
    {
        let function = function.into();
        let return_type = self
            .module
            .function(&function)
            .map(|callee| callee.return_type)
            .ok_or_else(|| BuildError::UnknownFunction(function.clone()))?;
        if return_type != Type::VOID
        {
            return Err(BuildError::UnexpectedCallResult {
                function,
                return_type,
            });
        }
        self.call_checked(function, arguments).map(|_| ())
    }

    /// Appends aggregate construction.
    pub fn aggregate(&mut self, value_type: Type, fields: Vec<ValueId>) -> Result<ValueId, BuildError>
    {
        let block = self.block()?;
        self.function_mut()?
            .add_aggregate(block, value_type, fields)
            .ok_or(BuildError::InvalidOperation("aggregate"))
    }

    /// Appends array construction.
    pub fn array(&mut self, value_type: Type, elements: Vec<ValueId>) -> Result<ValueId, BuildError>
    {
        let block = self.block()?;
        self.function_mut()?
            .add_array(block, value_type, elements)
            .ok_or(BuildError::InvalidOperation("array"))
    }

    /// Extracts a field or fixed-array element by constant index.
    pub fn extract(&mut self, aggregate: ValueId, field: u32, field_type: Type) -> Result<ValueId, BuildError>
    {
        let block = self.block()?;
        self.function_mut()?
            .add_extract(block, aggregate, field, field_type)
            .ok_or(BuildError::InvalidOperation("extract"))
    }

    /// Reads an array value's runtime length.
    pub fn array_length(&mut self, array: ValueId) -> Result<ValueId, BuildError>
    {
        let block = self.block()?;
        self.function_mut()?
            .add_array_length(block, array)
            .ok_or(BuildError::InvalidOperation("array length"))
    }

    /// Reads a runtime-indexed array element.
    pub fn array_get(&mut self, array: ValueId, index: ValueId, element_type: Type) -> Result<ValueId, BuildError>
    {
        let block = self.block()?;
        self.function_mut()?
            .add_array_get(block, array, index, element_type)
            .ok_or(BuildError::InvalidOperation("array get"))
    }

    /// Produces an array value with one runtime-indexed element replaced.
    pub fn array_set(&mut self, array: ValueId, index: ValueId, value: ValueId) -> Result<ValueId, BuildError>
    {
        let block = self.block()?;
        self.function_mut()?
            .add_array_set(block, array, index, value)
            .ok_or(BuildError::InvalidOperation("array set"))
    }

    /// Loads the current value of a stack slot.
    pub fn load(&mut self, slot: SlotId) -> Result<ValueId, BuildError>
    {
        let block = self.block()?;
        self.function_mut()?
            .add_load(block, slot)
            .ok_or(BuildError::InvalidOperation("load"))
    }

    /// Stores a same-typed value into a stack slot.
    pub fn store(&mut self, slot: SlotId, value: ValueId) -> Result<(), BuildError>
    {
        let block = self.block()?;
        self.function_mut()?
            .add_store(block, slot, value)
            .then_some(())
            .ok_or(BuildError::InvalidOperation("store"))
    }

    /// Terminates the insertion block with a void or value return.
    pub fn return_value(&mut self, value: Option<ValueId>) -> Result<(), BuildError>
    {
        let block = self.block()?;
        self.function_mut()?
            .set_return(block, value)
            .then_some(())
            .ok_or(BuildError::InvalidOperation("return"))
    }

    /// Terminates the insertion block with a void return.
    pub fn return_void(&mut self) -> Result<(), BuildError>
    {
        self.return_value(None)
    }

    /// Terminates the insertion block with an unconditional branch.
    pub fn branch(&mut self, target: BlockId) -> Result<(), BuildError>
    {
        let block = self.block()?;
        self.function_mut()?
            .set_branch(block, target)
            .then_some(())
            .ok_or(BuildError::InvalidOperation("branch"))
    }

    /// Terminates the insertion block with a boolean conditional branch.
    pub fn branch_if(&mut self, condition: ValueId, then_block: BlockId, else_block: BlockId)
    -> Result<(), BuildError>
    {
        let block = self.block()?;
        self.function_mut()?
            .set_branch_if(block, condition, then_block, else_block)
            .then_some(())
            .ok_or(BuildError::InvalidOperation("conditional branch"))
    }

    /// Terminates the insertion block with an explicit panic.
    pub fn panic(&mut self) -> Result<(), BuildError>
    {
        let block = self.block()?;
        self.function_mut()?
            .set_panic(block)
            .then_some(())
            .ok_or(BuildError::InvalidOperation("panic"))
    }

    /// Verifies and returns the completed module.
    pub fn finish(self) -> Result<Module, BuildError>
    {
        let diagnostics = verify::verify_module(&self.module);
        if diagnostics.is_empty()
        {
            Ok(self.module)
        }
        else
        {
            Err(BuildError::Verification(diagnostics))
        }
    }

    fn reject_duplicate(&self, name: &str) -> Result<(), BuildError>
    {
        if self.module.functions.iter().any(|function| function.name == name)
        {
            Err(BuildError::DuplicateFunction(name.to_owned()))
        }
        else
        {
            Ok(())
        }
    }

    fn function(&self) -> Result<&Function, BuildError>
    {
        self.current_function
            .and_then(|index| self.module.functions.get(index))
            .ok_or(BuildError::NoCurrentFunction)
    }

    fn function_mut(&mut self) -> Result<&mut Function, BuildError>
    {
        self.current_function
            .and_then(|index| self.module.functions.get_mut(index))
            .ok_or(BuildError::NoCurrentFunction)
    }

    fn block(&self) -> Result<BlockId, BuildError>
    {
        self.function()?;
        self.insertion_block.ok_or(BuildError::NoInsertionBlock)
    }
}
