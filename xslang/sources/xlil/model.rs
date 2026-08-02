/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

mod array;
mod instruction;
mod integer;
mod types;

use super::I32BinaryOperation;

pub use instruction::Instruction;
pub use integer::IntegerConstant;
pub use types::{Type, TypeKind, Utf32Encoding};

/// Latest XLIL text/model version emitted by this crate release.
pub const SUPPORTED_XLIL_VERSION: u32 = 1;

/// Returns whether `version` is accepted by the XLIL reader.
///
/// Version 0 remains readable for compatibility while canonical output uses
/// [`SUPPORTED_XLIL_VERSION`].
#[must_use]
pub const fn is_supported_xlil_version(version: u32) -> bool
{
  matches!(version, 0 | SUPPORTED_XLIL_VERSION)
}

/// Sequential function-local SSA value register identifier.
#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
pub struct ValueId(pub u32);

/// Sequential function-local basic block identifier.
#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
pub struct BlockId(pub u32);

/// Sequential function-local stack slot identifier.
#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
pub struct SlotId(pub u32);

/// Typed value-table entry.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct Value
{
  /// Register identifier.
  pub id: ValueId,
  /// Exact XLIL value type.
  pub value_type: Type,
}

/// Typed stack-slot table entry.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct Slot
{
  /// Slot identifier.
  pub id: SlotId,
  /// Type accepted by loads and stores.
  pub value_type: Type,
}

/// Control-flow record that ends a basic block.
#[derive(Clone, Debug, Eq, PartialEq)]
pub enum Terminator
{
  /// Returns no value or one function result value.
  Return(Option<ValueId>),
  /// Transfers control unconditionally.
  Branch(BlockId),
  /// Selects one of two targets from a boolean register.
  BranchIf
  {
    /// Boolean condition register.
    condition: ValueId,
    /// Target selected when the condition is true.
    then_block: BlockId,
    /// Target selected when the condition is false.
    else_block: BlockId,
  },
  /// Terminates execution through the runtime panic path.
  Panic,
}

/// Ordered instruction list and terminator for one basic block.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct Block
{
  /// Function-local block identifier.
  pub id: BlockId,
  /// Human-readable block label.
  pub label: String,
  /// Instructions in execution order.
  pub instructions: Vec<Instruction>,
  /// Required final control-flow record for a definition block.
  pub terminator: Option<Terminator>,
}

/// XLIL function declaration or definition.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct Function
{
  /// Module-level symbol name.
  pub name: String,
  /// Function result type, or [`Type::VOID`].
  pub return_type: Type,
  /// Parameter types in ABI order.
  pub parameters: Vec<Type>,
  /// Sequential value registry; definition parameters occupy the prefix.
  pub values: Vec<Value>,
  /// Function-local stack-slot registry.
  pub slots: Vec<Slot>,
  /// Definition body in registry order.
  pub blocks: Vec<Block>,
  /// `true` for `.func` and `false` for `.extern`.
  pub is_definition: bool,
}

impl Function
{
  #[must_use]
  pub(crate) fn declaration(name: impl Into<String>, return_type: Type, parameters: Vec<Type>) -> Self
  {
    Self { name: name.into(),
           return_type,
           parameters,
           values: vec![],
           slots: vec![],
           blocks: vec![],
           is_definition: false }
  }

  #[must_use]
  pub(crate) fn definition(name: impl Into<String>, return_type: Type, parameters: Vec<Type>) -> Self
  {
    let values = parameters.iter()
                           .enumerate()
                           .map(|(index, value_type)| Value { id: ValueId(index as u32),
                                                              value_type: *value_type })
                           .collect();
    Self { name: name.into(),
           return_type,
           parameters,
           values,
           slots: vec![],
           blocks: vec![],
           is_definition: true }
  }

  /// Returns the value register corresponding to a parameter.
  #[must_use]
  pub fn parameter_value(&self, parameter: usize) -> Option<ValueId>
  {
    self.parameters.get(parameter)?;
    Some(ValueId(parameter as u32))
  }

  pub(crate) fn append_block(&mut self, label: impl Into<String>) -> BlockId
  {
    let id = BlockId(self.blocks.len() as u32);
    self.blocks.push(Block { id,
                             label: label.into(),
                             instructions: vec![],
                             terminator: None });
    id
  }

  pub(crate) fn add_const_i64(&mut self, block: BlockId, value: i64) -> Option<ValueId>
  {
    let result = ValueId(self.values.len() as u32);
    self.values.push(Value { id: result,
                             value_type: Type::I64 });
    self.block_mut(block)?.instructions.push(Instruction::ConstI64 { result,
                                                                     value });
    Some(result)
  }

  pub(crate) fn add_const_i32(&mut self, block: BlockId, value: i32) -> Option<ValueId>
  {
    let result = ValueId(self.values.len() as u32);
    self.values.push(Value { id: result,
                             value_type: Type::I32 });
    self.block_mut(block)?.instructions.push(Instruction::ConstI32 { result,
                                                                     value });
    Some(result)
  }

  pub(crate) fn add_const_u16(&mut self, block: BlockId, value: u16) -> Option<ValueId>
  {
    let result = ValueId(self.values.len() as u32);
    self.values.push(Value { id: result,
                             value_type: Type::U16 });
    self.block_mut(block)?.instructions.push(Instruction::ConstU16 { result,
                                                                     value });
    Some(result)
  }

  pub(crate) fn add_const_integer(&mut self, block: BlockId, value: IntegerConstant) -> Option<ValueId>
  {
    let result = ValueId(self.values.len() as u32);
    self.values.push(Value { id: result,
                             value_type: value.value_type });
    self.block_mut(block)?
        .instructions
        .push(Instruction::ConstInteger { result,
                                          value });
    Some(result)
  }

  pub(crate) fn add_const_f32_bits(&mut self, block: BlockId, bits: u32) -> Option<ValueId>
  {
    let result = ValueId(self.values.len() as u32);
    self.values.push(Value { id: result,
                             value_type: Type::F32 });
    self.block_mut(block)?.instructions.push(Instruction::ConstF32 { result,
                                                                     bits });
    Some(result)
  }

  pub(crate) fn add_const_f64_bits(&mut self, block: BlockId, bits: u64) -> Option<ValueId>
  {
    let result = ValueId(self.values.len() as u32);
    self.values.push(Value { id: result,
                             value_type: Type::F64 });
    self.block_mut(block)?.instructions.push(Instruction::ConstF64 { result,
                                                                     bits });
    Some(result)
  }

  pub(crate) fn add_const_str(&mut self,
                              block: BlockId,
                              encoding: Utf32Encoding,
                              units: impl Into<Vec<u32>>)
                              -> Option<ValueId>
  {
    let result = ValueId(self.values.len() as u32);
    self.values.push(Value { id: result,
                             value_type: Type::STR });
    self.block_mut(block)?.instructions.push(Instruction::ConstStr { result,
                                                                     encoding,
                                                                     units: units.into() });
    Some(result)
  }

  pub(crate) fn add_const_bool(&mut self, block: BlockId, value: bool) -> Option<ValueId>
  {
    let result = ValueId(self.values.len() as u32);
    self.values.push(Value { id: result,
                             value_type: Type::BOOL });
    self.block_mut(block)?
        .instructions
        .push(Instruction::ConstBool { result,
                                       value });
    Some(result)
  }

  pub(crate) fn add_i64(&mut self, block: BlockId, left: ValueId, right: ValueId) -> Option<ValueId>
  {
    self.block(block)?;
    if !self.value(left).is_some_and(|value| value.value_type == Type::I64) ||
       !self.value(right).is_some_and(|value| value.value_type == Type::I64)
    {
      return None;
    }
    let result = ValueId(self.values.len() as u32);
    self.values.push(Value { id: result,
                             value_type: Type::I64 });
    self.block_mut(block)?.instructions.push(Instruction::AddI64 { result,
                                                                   left,
                                                                   right });
    Some(result)
  }

  pub(crate) fn sub_i64(&mut self, block: BlockId, left: ValueId, right: ValueId) -> Option<ValueId>
  {
    self.block(block)?;
    if !self.value(left).is_some_and(|value| value.value_type == Type::I64) ||
       !self.value(right).is_some_and(|value| value.value_type == Type::I64)
    {
      return None;
    }
    let result = ValueId(self.values.len() as u32);
    self.values.push(Value { id: result,
                             value_type: Type::I64 });
    self.block_mut(block)?.instructions.push(Instruction::SubI64 { result,
                                                                   left,
                                                                   right });
    Some(result)
  }

  pub(crate) fn mul_i64(&mut self, block: BlockId, left: ValueId, right: ValueId) -> Option<ValueId>
  {
    self.block(block)?;
    if !self.value(left).is_some_and(|value| value.value_type == Type::I64) ||
       !self.value(right).is_some_and(|value| value.value_type == Type::I64)
    {
      return None;
    }
    let result = ValueId(self.values.len() as u32);
    self.values.push(Value { id: result,
                             value_type: Type::I64 });
    self.block_mut(block)?.instructions.push(Instruction::MulI64 { result,
                                                                   left,
                                                                   right });
    Some(result)
  }

  pub(crate) fn eq_i64(&mut self, block: BlockId, left: ValueId, right: ValueId) -> Option<ValueId>
  {
    self.block(block)?;
    if !self.value(left).is_some_and(|value| value.value_type == Type::I64) ||
       !self.value(right).is_some_and(|value| value.value_type == Type::I64)
    {
      return None;
    }
    let result = ValueId(self.values.len() as u32);
    self.values.push(Value { id: result,
                             value_type: Type::BOOL });
    self.block_mut(block)?.instructions.push(Instruction::EqI64 { result,
                                                                  left,
                                                                  right });
    Some(result)
  }

  pub(crate) fn add_i32(&mut self, block: BlockId, left: ValueId, right: ValueId) -> Option<ValueId>
  {
    self.add_i32_like(block, left, right, Type::I32, I32Op::Add)
  }

  pub(crate) fn sub_i32(&mut self, block: BlockId, left: ValueId, right: ValueId) -> Option<ValueId>
  {
    self.add_i32_like(block, left, right, Type::I32, I32Op::Sub)
  }

  pub(crate) fn mul_i32(&mut self, block: BlockId, left: ValueId, right: ValueId) -> Option<ValueId>
  {
    self.add_i32_like(block, left, right, Type::I32, I32Op::Mul)
  }

  pub(crate) fn binary_i32(&mut self,
                           block: BlockId,
                           left: ValueId,
                           right: ValueId,
                           operation: I32BinaryOperation)
                           -> Option<ValueId>
  {
    self.block(block)?;
    if !self.value(left).is_some_and(|value| value.value_type == Type::I32) ||
       !self.value(right).is_some_and(|value| value.value_type == Type::I32)
    {
      return None;
    }
    let result = ValueId(self.values.len() as u32);
    self.values.push(Value { id: result,
                             value_type: Type::I32 });
    self.block_mut(block)?
        .instructions
        .push(Instruction::BinaryI32 { operation,
                                       result,
                                       left,
                                       right });
    Some(result)
  }

  pub(crate) fn eq_i32(&mut self, block: BlockId, left: ValueId, right: ValueId) -> Option<ValueId>
  {
    self.add_i32_like(block, left, right, Type::BOOL, I32Op::Eq)
  }

  pub(crate) fn lt_i32(&mut self, block: BlockId, left: ValueId, right: ValueId) -> Option<ValueId>
  {
    self.add_i32_like(block, left, right, Type::BOOL, I32Op::Lt)
  }

  pub(crate) fn le_i32(&mut self, block: BlockId, left: ValueId, right: ValueId) -> Option<ValueId>
  {
    self.add_i32_like(block, left, right, Type::BOOL, I32Op::Le)
  }

  pub(crate) fn gt_i32(&mut self, block: BlockId, left: ValueId, right: ValueId) -> Option<ValueId>
  {
    self.add_i32_like(block, left, right, Type::BOOL, I32Op::Gt)
  }

  pub(crate) fn ge_i32(&mut self, block: BlockId, left: ValueId, right: ValueId) -> Option<ValueId>
  {
    self.add_i32_like(block, left, right, Type::BOOL, I32Op::Ge)
  }

  fn add_i32_like(&mut self,
                  block: BlockId,
                  left: ValueId,
                  right: ValueId,
                  result_type: Type,
                  op: I32Op)
                  -> Option<ValueId>
  {
    self.block(block)?;
    if !self.value(left).is_some_and(|value| value.value_type == Type::I32) ||
       !self.value(right).is_some_and(|value| value.value_type == Type::I32)
    {
      return None;
    }
    let result = ValueId(self.values.len() as u32);
    self.values.push(Value { id: result,
                             value_type: result_type });
    self.block_mut(block)?
        .instructions
        .push(i32_instruction(op, result, left, right));
    Some(result)
  }

  pub(crate) fn add_call(&mut self,
                         block: BlockId,
                         function: impl Into<String>,
                         arguments: Vec<ValueId>,
                         return_type: Type)
                         -> Option<Option<ValueId>>
  {
    self.block(block)?;
    if arguments.iter().any(|argument| self.value(*argument).is_none())
    {
      return None;
    }
    let result = if return_type == Type::VOID
    {
      None
    }
    else
    {
      let result = ValueId(self.values.len() as u32);
      self.values.push(Value { id: result,
                               value_type: return_type });
      Some(result)
    };
    self.block_mut(block)?.instructions.push(Instruction::Call { result,
                                                                 function: function.into(),
                                                                 arguments,
                                                                 return_type });
    Some(result)
  }

  pub(crate) fn add_aggregate(&mut self, block: BlockId, value_type: Type, fields: Vec<ValueId>) -> Option<ValueId>
  {
    self.add_composite(block, value_type, fields, TypeKind::Aggregate)
  }

  pub(crate) fn add_array(&mut self, block: BlockId, value_type: Type, elements: Vec<ValueId>) -> Option<ValueId>
  {
    self.add_composite(block, value_type, elements, TypeKind::Array)
  }

  fn add_composite(&mut self,
                   block: BlockId,
                   value_type: Type,
                   fields: Vec<ValueId>,
                   expected_kind: TypeKind)
                   -> Option<ValueId>
  {
    if value_type.kind != expected_kind || fields.iter().any(|field| self.value(*field).is_none())
    {
      return None;
    }
    let result = ValueId(self.values.len() as u32);
    self.values.push(Value { id: result,
                             value_type });
    self.block_mut(block)?
        .instructions
        .push(Instruction::Aggregate { result,
                                       value_type,
                                       fields });
    Some(result)
  }

  pub(crate) fn add_extract(&mut self,
                            block: BlockId,
                            aggregate: ValueId,
                            field: u32,
                            field_type: Type)
                            -> Option<ValueId>
  {
    if field_type == Type::VOID ||
       !matches!(self.value(aggregate)?.value_type.kind,
                 TypeKind::Aggregate | TypeKind::Array)
    {
      return None;
    }
    let result = ValueId(self.values.len() as u32);
    self.values.push(Value { id: result,
                             value_type: field_type });
    self.block_mut(block)?.instructions.push(Instruction::Extract { result,
                                                                    aggregate,
                                                                    field });
    Some(result)
  }

  pub(crate) fn add_slot(&mut self, value_type: Type) -> Option<SlotId>
  {
    if value_type == Type::VOID
    {
      return None;
    }
    let id = SlotId(self.slots.len() as u32);
    self.slots.push(Slot { id,
                           value_type });
    Some(id)
  }

  pub(crate) fn add_load(&mut self, block: BlockId, slot: SlotId) -> Option<ValueId>
  {
    let value_type = self.slot(slot)?.value_type;
    let result = ValueId(self.values.len() as u32);
    self.values.push(Value { id: result,
                             value_type });
    self.block_mut(block)?.instructions.push(Instruction::Load { result,
                                                                 slot });
    Some(result)
  }

  pub(crate) fn add_store(&mut self, block: BlockId, slot: SlotId, value: ValueId) -> bool
  {
    let Some(slot_type) = self.slot(slot).map(|slot| slot.value_type)
    else
    {
      return false;
    };
    if !self.value(value).is_some_and(|value| value.value_type == slot_type)
    {
      return false;
    }
    let Some(block) = self.block_mut(block)
    else
    {
      return false;
    };
    block.instructions.push(Instruction::Store { slot,
                                                 value });
    true
  }

  pub(crate) fn set_return(&mut self, block: BlockId, value: Option<ValueId>) -> bool
  {
    let return_type_matches = match value
    {
      Some(value) => self.values
                         .get(value.0 as usize)
                         .is_some_and(|value| value.value_type == self.return_type),
      None => self.return_type == Type::VOID,
    };
    if !return_type_matches
    {
      return false;
    }
    let Some(block) = self.block_mut(block)
    else
    {
      return false;
    };
    if block.terminator.is_some()
    {
      return false;
    }
    block.terminator = Some(Terminator::Return(value));
    true
  }

  pub(crate) fn set_branch(&mut self, block: BlockId, target: BlockId) -> bool
  {
    if self.block(target).is_none()
    {
      return false;
    }
    let Some(block) = self.block_mut(block)
    else
    {
      return false;
    };
    if block.terminator.is_some()
    {
      return false;
    }
    block.terminator = Some(Terminator::Branch(target));
    true
  }

  pub(crate) fn set_branch_if(&mut self,
                              block: BlockId,
                              condition: ValueId,
                              then_block: BlockId,
                              else_block: BlockId)
                              -> bool
  {
    if self.block(then_block).is_none() || self.block(else_block).is_none()
    {
      return false;
    }
    if !self.value(condition)
            .is_some_and(|value| value.value_type == Type::BOOL)
    {
      return false;
    }
    let Some(block) = self.block_mut(block)
    else
    {
      return false;
    };
    if block.terminator.is_some()
    {
      return false;
    }
    block.terminator = Some(Terminator::BranchIf { condition,
                                                   then_block,
                                                   else_block });
    true
  }

  pub(crate) fn set_panic(&mut self, block: BlockId) -> bool
  {
    let Some(block) = self.block_mut(block)
    else
    {
      return false;
    };
    if block.terminator.is_some()
    {
      return false;
    }
    block.terminator = Some(Terminator::Panic);
    true
  }

  /// Finds a basic block by id.
  #[must_use]
  pub fn block(&self, block: BlockId) -> Option<&Block>
  {
    self.blocks
        .get(block.0 as usize)
        .filter(|candidate| candidate.id == block)
  }

  pub(super) fn block_mut(&mut self, block: BlockId) -> Option<&mut Block>
  {
    self.blocks
        .get_mut(block.0 as usize)
        .filter(|candidate| candidate.id == block)
  }

  /// Finds a value by its function-local register id.
  #[must_use]
  pub fn value(&self, value: ValueId) -> Option<&Value>
  {
    self.values
        .get(value.0 as usize)
        .filter(|candidate| candidate.id == value)
  }

  fn slot(&self, slot: SlotId) -> Option<&Slot>
  {
    self.slots.get(slot.0 as usize).filter(|candidate| candidate.id == slot)
  }

  /// Finds a stack slot by its function-local id.
  #[must_use]
  pub fn stack_slot(&self, slot: SlotId) -> Option<&Slot>
  {
    self.slot(slot)
  }

  /// Finds a basic block by its human-readable label.
  #[must_use]
  pub fn block_named(&self, label: &str) -> Option<&Block>
  {
    self.blocks.iter().find(|candidate| candidate.label == label)
  }
}

#[derive(Clone, Copy)]
enum I32Op
{
  Add,
  Sub,
  Mul,
  Eq,
  Lt,
  Le,
  Gt,
  Ge,
}

fn i32_instruction(op: I32Op, result: ValueId, left: ValueId, right: ValueId) -> Instruction
{
  match op
  {
    I32Op::Add => Instruction::AddI32 { result,
                                        left,
                                        right },
    I32Op::Sub => Instruction::SubI32 { result,
                                        left,
                                        right },
    I32Op::Mul => Instruction::MulI32 { result,
                                        left,
                                        right },
    I32Op::Eq => Instruction::EqI32 { result,
                                      left,
                                      right },
    I32Op::Lt => Instruction::LtI32 { result,
                                      left,
                                      right },
    I32Op::Le => Instruction::LeI32 { result,
                                      left,
                                      right },
    I32Op::Gt => Instruction::GtI32 { result,
                                      left,
                                      right },
    I32Op::Ge => Instruction::GeI32 { result,
                                      left,
                                      right },
  }
}

/// Complete XLIL v0 module registry.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct Module
{
  /// Module registry name.
  pub name: String,
  /// Named aggregate layouts.
  pub aggregate_types: Vec<AggregateType>,
  /// Fixed and runtime-length array layouts.
  pub array_types: Vec<ArrayType>,
  /// Function declarations and definitions.
  pub functions: Vec<Function>,
}

/// Named aggregate layout entry.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct AggregateType
{
  /// Sequential registry identifier.
  pub id: u32,
  /// Stable text name.
  pub name: String,
  /// Field types in layout order.
  pub fields: Vec<Type>,
}

/// Array layout entry.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct ArrayType
{
  /// Sequential registry identifier.
  pub id: u32,
  /// Element type.
  pub element_type: Type,
  /// Fixed length, or `None` for a runtime-length array.
  pub length: Option<u64>,
}

impl Module
{
  /// Finds a function declaration or definition by symbol name.
  #[must_use]
  pub fn function(&self, name: &str) -> Option<&Function>
  {
    self.functions.iter().find(|function| function.name == name)
  }

  /// Finds a named aggregate layout.
  #[must_use]
  pub fn aggregate_named(&self, name: &str) -> Option<&AggregateType>
  {
    self.aggregate_types.iter().find(|aggregate| aggregate.name == name)
  }

  /// Resolves an aggregate registry reference.
  #[must_use]
  pub fn aggregate(&self, value_type: Type) -> Option<&AggregateType>
  {
    self.aggregate_type(value_type)
  }

  /// Resolves an array registry reference.
  #[must_use]
  pub fn array(&self, value_type: Type) -> Option<&ArrayType>
  {
    self.array_type(value_type)
  }

  #[must_use]
  pub(crate) fn new(name: impl Into<String>) -> Self
  {
    Self { name: name.into(),
           aggregate_types: vec![],
           array_types: vec![],
           functions: vec![] }
  }

  pub(crate) fn add_aggregate_type(&mut self, name: impl Into<String>, fields: Vec<Type>) -> Option<Type>
  {
    let name = name.into();
    if name.is_empty() || self.aggregate_types.iter().any(|entry| entry.name == name)
    {
      return None;
    }
    let id = u32::try_from(self.aggregate_types.len()).ok()?;
    self.aggregate_types.push(AggregateType { id,
                                              name,
                                              fields });
    Some(Type::aggregate(id))
  }

  #[must_use]
  pub(crate) fn aggregate_type(&self, value_type: Type) -> Option<&AggregateType>
  {
    (value_type.kind == TypeKind::Aggregate).then_some(())?;
    self.aggregate_types.get(value_type.registry_id as usize)
  }

  pub(crate) fn add_array_type(&mut self, element_type: Type, length: u64) -> Option<Type>
  {
    self.add_array_layout(element_type, Some(length))
  }

  pub(crate) fn add_dynamic_array_type(&mut self, element_type: Type) -> Option<Type>
  {
    self.add_array_layout(element_type, None)
  }

  fn add_array_layout(&mut self, element_type: Type, length: Option<u64>) -> Option<Type>
  {
    if element_type == Type::VOID ||
       length == Some(0) ||
       self.array_types
           .iter()
           .any(|entry| entry.element_type == element_type && entry.length == length)
    {
      return None;
    }
    let id = u32::try_from(self.array_types.len()).ok()?;
    self.array_types.push(ArrayType { id,
                                      element_type,
                                      length });
    Some(Type::array(id))
  }

  #[must_use]
  pub(crate) fn array_type(&self, value_type: Type) -> Option<&ArrayType>
  {
    (value_type.kind == TypeKind::Array).then_some(())?;
    self.array_types.get(value_type.registry_id as usize)
  }

  pub(crate) fn add_function(&mut self, function: Function)
  {
    self.functions.push(function);
  }
}

#[cfg(test)]
mod tests;
