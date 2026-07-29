/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use std::collections::HashSet;

use crate::xlil::{Function, Instruction, Module, SlotId, Terminator, Type, TypeKind, ValueId, type_name};

mod composite;
mod diagnostic;

pub use diagnostic::{Diagnostic, DiagnosticCode};

/// Checks all registry, type, instruction, call, and control-flow invariants.
#[must_use]
pub fn verify_module(module: &Module) -> Vec<Diagnostic>
{
  let mut verifier = Verifier::default();
  verifier.module(module);
  verifier.diagnostics
}

#[derive(Default)]
struct Verifier
{
  diagnostics: Vec<Diagnostic>,
}

impl Verifier
{
  fn module(&mut self, module: &Module)
  {
    if module.name.is_empty()
    {
      self.report(DiagnosticCode::EmptyModuleName, "XLIL module name must not be empty");
    }
    let mut names = HashSet::new();
    let mut aggregate_names = HashSet::new();
    for (index, aggregate) in module.aggregate_types.iter().enumerate()
    {
      if aggregate.id as usize != index || aggregate.name.is_empty()
      {
        self.report(DiagnosticCode::InvalidAggregateType,
                    "XLIL aggregate type ids must be sequential and names must not be empty");
      }
      if !aggregate_names.insert(aggregate.name.as_str())
      {
        self.report(DiagnosticCode::DuplicateAggregateName,
                    "XLIL aggregate type names must be unique");
      }
      for field in &aggregate.fields
      {
        if *field == Type::VOID || !Self::valid_type(module, *field)
        {
          self.report(DiagnosticCode::InvalidAggregateType,
                      "XLIL aggregate fields must reference known non-void types");
        }
      }
    }
    for (index, array) in module.array_types.iter().enumerate()
    {
      if array.id as usize != index ||
         array.length == Some(0) ||
         array.element_type == Type::VOID ||
         !Self::valid_type(module, array.element_type)
      {
        self.report(DiagnosticCode::InvalidArrayType,
                    "XLIL array registry requires sequential ids, a non-void element type, and nonzero fixed length");
      }
    }
    for function in &module.functions
    {
      if !names.insert(function.name.as_str())
      {
        self.report(DiagnosticCode::DuplicateFunctionName,
                    "XLIL function names must be unique");
      }
      if !Self::valid_type(module, function.return_type) ||
         function.parameters
                 .iter()
                 .any(|value_type| !Self::valid_type(module, *value_type)) ||
         function.values
                 .iter()
                 .any(|value| !Self::valid_type(module, value.value_type)) ||
         function.slots
                 .iter()
                 .any(|slot| !Self::valid_type(module, slot.value_type))
      {
        self.report(DiagnosticCode::InvalidAggregateType,
                    "XLIL function types must reference the module type registry");
      }
      self.function(function, module);
    }
  }

  fn valid_type(module: &Module, value_type: Type) -> bool
  {
    match value_type.kind
    {
      TypeKind::Aggregate => module.aggregate_types
                                   .get(value_type.registry_id as usize)
                                   .is_some_and(|entry| entry.id == value_type.registry_id),
      TypeKind::Array => module.array_types
                               .get(value_type.registry_id as usize)
                               .is_some_and(|entry| entry.id == value_type.registry_id),
      _ => true,
    }
  }

  fn function(&mut self, function: &Function, module: &Module)
  {
    if function.name.is_empty()
    {
      self.report(DiagnosticCode::EmptyFunctionName,
                  "XLIL function name must not be empty");
    }
    if !function.is_definition && !function.blocks.is_empty()
    {
      self.report(DiagnosticCode::DeclarationHasBody,
                  "XLIL function declaration must not contain blocks");
    }
    if function.is_definition && function.blocks.is_empty()
    {
      self.report(DiagnosticCode::DefinitionHasNoBlocks,
                  "XLIL function definition must contain at least one block");
    }
    for (index, slot) in function.slots.iter().enumerate()
    {
      if slot.id != SlotId(index as u32) || slot.value_type == Type::VOID
      {
        self.report(DiagnosticCode::StackSlotInvalid,
                    "XLIL stack slots must be sequential and have non-void types");
      }
    }
    let blocks = function.blocks.iter().map(|block| block.id).collect::<HashSet<_>>();
    let mut seen_blocks = HashSet::new();
    for block in &function.blocks
    {
      if !seen_blocks.insert(block.id)
      {
        self.report(DiagnosticCode::DuplicateBlockId,
                    "XLIL block ids must be unique within a function");
      }
      if block.label.is_empty()
      {
        self.report(DiagnosticCode::EmptyBlockLabel, "XLIL block label must not be empty");
      }
      for instruction in &block.instructions
      {
        self.instruction(function, instruction, module);
      }
      self.terminator(function, block.terminator.as_ref(), &blocks);
    }
  }

  fn instruction(&mut self, function: &Function, instruction: &Instruction, module: &Module)
  {
    match *instruction
    {
      Instruction::ConstI64 { result, .. } => self.i64_value(function, result, "XLIL const result"),
      Instruction::ConstI32 { result, .. } => self.i32_value(function, result, "XLIL const.i32 result"),
      Instruction::ConstU16 { result, .. } => self.typed_value(function, result, Type::U16, "XLIL const.u16 result"),
      Instruction::ConstInteger { result,
                                  value, } => self.typed_value(function,
                                                               result,
                                                               value.value_type,
                                                               &format!("XLIL const.{} result",
                                                                        crate::xlil::type_name(value.value_type))),
      Instruction::ConstF32 { result, .. } => self.typed_value(function, result, Type::F32, "XLIL const.f32 result"),
      Instruction::ConstF64 { result, .. } => self.typed_value(function, result, Type::F64, "XLIL const.f64 result"),
      Instruction::ConstStr { result, .. } => self.typed_value(function, result, Type::STR, "XLIL const.str result"),
      Instruction::ConstBool { result, .. } => self.bool_value(function, result, "XLIL const.bool result"),
      Instruction::BinaryInteger { operation,
                                   value_type,
                                   result,
                                   left,
                                   right, } =>
      {
        let name = format!("{}.{}", operation.text_stem(), crate::xlil::type_name(value_type));
        if !value_type.is_integer()
        {
          self.report(DiagnosticCode::InstructionResultUnknown,
                      &format!("XLIL {name} declares a non-integer operand type"));
          return;
        }
        if operation.is_comparison()
        {
          self.bool_value(function, result, &format!("XLIL {name} result"));
        }
        else
        {
          self.typed_value(function, result, value_type, &format!("XLIL {name} result"));
        }
        self.typed_value(function, left, value_type, &format!("XLIL {name} left operand"));
        self.typed_value(function, right, value_type, &format!("XLIL {name} right operand"));
      }
      Instruction::BinaryFloat { operation,
                                 value_type,
                                 result,
                                 left,
                                 right, } =>
      {
        let name = format!("{}.{}", operation.text_stem(), crate::xlil::type_name(value_type));
        if !self.floating_operation_type(value_type, &name)
        {
          return;
        }
        self.typed_value(function, result, value_type, &format!("XLIL {name} result"));
        self.typed_value(function, left, value_type, &format!("XLIL {name} left operand"));
        self.typed_value(function, right, value_type, &format!("XLIL {name} right operand"));
      }
      Instruction::CompareFloat { operation,
                                  value_type,
                                  result,
                                  left,
                                  right, } =>
      {
        let name = format!("{}.{}", operation.text_stem(), crate::xlil::type_name(value_type));
        if !self.floating_operation_type(value_type, &name)
        {
          return;
        }
        self.bool_value(function, result, &format!("XLIL {name} result"));
        self.typed_value(function, left, value_type, &format!("XLIL {name} left operand"));
        self.typed_value(function, right, value_type, &format!("XLIL {name} right operand"));
      }
      Instruction::CompareStr { operation,
                                result,
                                left,
                                right, } =>
      {
        let name = format!("{}.str", operation.text_stem());
        self.bool_value(function, result, &format!("XLIL {name} result"));
        self.typed_value(function, left, Type::STR, &format!("XLIL {name} left operand"));
        self.typed_value(function, right, Type::STR, &format!("XLIL {name} right operand"));
      }
      Instruction::AddI64 { result,
                            left,
                            right, } =>
      {
        self.i64_value(function, result, "XLIL add.i64 result");
        self.i64_value(function, left, "XLIL add.i64 left operand");
        self.i64_value(function, right, "XLIL add.i64 right operand");
      }
      Instruction::SubI64 { result,
                            left,
                            right, } =>
      {
        self.i64_value(function, result, "XLIL sub.i64 result");
        self.i64_value(function, left, "XLIL sub.i64 left operand");
        self.i64_value(function, right, "XLIL sub.i64 right operand");
      }
      Instruction::MulI64 { result,
                            left,
                            right, } =>
      {
        self.i64_value(function, result, "XLIL mul.i64 result");
        self.i64_value(function, left, "XLIL mul.i64 left operand");
        self.i64_value(function, right, "XLIL mul.i64 right operand");
      }
      Instruction::EqI64 { result,
                           left,
                           right, } =>
      {
        self.bool_value(function, result, "XLIL eq.i64 result");
        self.i64_value(function, left, "XLIL eq.i64 left operand");
        self.i64_value(function, right, "XLIL eq.i64 right operand");
      }
      Instruction::BinaryI64 { operation,
                               result,
                               left,
                               right, } =>
      {
        self.i64_value(function, result, &format!("XLIL {} result", operation.text_name()));
        self.i64_value(function, left, &format!("XLIL {} left operand", operation.text_name()));
        self.i64_value(function,
                       right,
                       &format!("XLIL {} right operand", operation.text_name()));
      }
      Instruction::CompareI64 { operation,
                                result,
                                left,
                                right, } =>
      {
        self.bool_value(function, result, &format!("XLIL {} result", operation.text_name()));
        self.i64_value(function, left, &format!("XLIL {} left operand", operation.text_name()));
        self.i64_value(function,
                       right,
                       &format!("XLIL {} right operand", operation.text_name()));
      }
      Instruction::AddI32 { result,
                            left,
                            right, } =>
      {
        self.i32_value(function, result, "XLIL add.i32 result");
        self.i32_value(function, left, "XLIL add.i32 left operand");
        self.i32_value(function, right, "XLIL add.i32 right operand");
      }
      Instruction::SubI32 { result,
                            left,
                            right, } =>
      {
        self.i32_value(function, result, "XLIL sub.i32 result");
        self.i32_value(function, left, "XLIL sub.i32 left operand");
        self.i32_value(function, right, "XLIL sub.i32 right operand");
      }
      Instruction::MulI32 { result,
                            left,
                            right, } =>
      {
        self.i32_value(function, result, "XLIL mul.i32 result");
        self.i32_value(function, left, "XLIL mul.i32 left operand");
        self.i32_value(function, right, "XLIL mul.i32 right operand");
      }
      Instruction::BinaryI32 { operation,
                               result,
                               left,
                               right, } =>
      {
        let name = operation.text_name();
        self.i32_value(function, result, &format!("XLIL {name} result"));
        self.i32_value(function, left, &format!("XLIL {name} left operand"));
        self.i32_value(function, right, &format!("XLIL {name} right operand"));
      }
      Instruction::EqI32 { result,
                           left,
                           right, } |
      Instruction::LtI32 { result,
                           left,
                           right, } |
      Instruction::LeI32 { result,
                           left,
                           right, } |
      Instruction::GtI32 { result,
                           left,
                           right, } |
      Instruction::GeI32 { result,
                           left,
                           right, } =>
      {
        self.bool_value(function, result, "XLIL i32 comparison result");
        self.i32_value(function, left, "XLIL i32 comparison left operand");
        self.i32_value(function, right, "XLIL i32 comparison right operand");
      }
      Instruction::NotBool { result,
                             operand, } =>
      {
        self.bool_value(function, result, "XLIL not.bool result");
        self.bool_value(function, operand, "XLIL not.bool operand");
      }
      Instruction::Call { result,
                          function: ref callee_name,
                          ref arguments,
                          return_type,
                          .. } =>
      {
        let Some(callee) = module.functions.iter().find(|candidate| candidate.name == *callee_name)
        else
        {
          self.report(DiagnosticCode::CallTargetUnknown,
                      "XLIL call target must be declared in the module");
          return;
        };
        if arguments.len() != callee.parameters.len()
        {
          self.report(DiagnosticCode::CallArgumentCountMismatch,
                      "XLIL call argument count must match the target signature");
        }
        for (argument, parameter) in arguments.iter().zip(&callee.parameters)
        {
          if value_type(function, *argument) != Some(*parameter)
          {
            self.report(DiagnosticCode::CallArgumentTypeMismatch,
                        "XLIL call argument type must match the target signature");
          }
        }
        if let Some(result) = result
        {
          if callee.return_type == crate::xlil::Type::VOID
          {
            self.report(DiagnosticCode::CallVoidResultMismatch,
                        "void XLIL call cannot produce a result value");
          }
          if return_type != callee.return_type
          {
            self.report(DiagnosticCode::CallResultTypeMismatch,
                        "XLIL call result type must match the target signature");
          }
          match value_type(function, result)
          {
            Some(value_type) if value_type == return_type =>
            {}
            _ => self.report(DiagnosticCode::InstructionResultUnknown,
                             "XLIL call result must reference a declared value with matching type"),
          }
        }
        else if callee.return_type != crate::xlil::Type::VOID
        {
          self.report(DiagnosticCode::CallVoidResultMismatch,
                      "non-void XLIL call must produce a result value");
        }
        for argument in arguments
        {
          if value_type(function, *argument).is_none()
          {
            self.report(DiagnosticCode::InstructionResultUnknown,
                        "XLIL call argument must reference a declared value");
          }
        }
      }
      Instruction::Aggregate { result,
                               value_type: aggregate_type,
                               ref fields, } => self.composite(function, module, result, aggregate_type, fields),
      Instruction::Extract { result,
                             aggregate,
                             field, } => self.extract(function, module, result, aggregate, field),
      Instruction::ArrayGet { result,
                              array,
                              index, } => self.array_access(function, module, result, array, index, None),
      Instruction::ArraySet { result,
                              array,
                              index,
                              value, } => self.array_access(function, module, result, array, index, Some(value)),
      Instruction::ArrayLength { result,
                                 array, } =>
      {
        self.typed_value(function, result, Type::I64, "XLIL len.array result");
        if value_type(function, array).and_then(|ty| module.array_type(ty))
                                      .is_none()
        {
          self.report(DiagnosticCode::InvalidArrayType,
                      "XLIL len.array source must use a known array registry type");
        }
      }
      Instruction::Load { result,
                          slot, } => self.memory(function, slot, result, "XLIL load"),
      Instruction::Store { slot,
                           value, } => self.memory(function, slot, value, "XLIL store"),
    }
  }

  fn memory(&mut self, function: &Function, slot: SlotId, value: ValueId, label: &str)
  {
    let Some(slot_type) = function.slots
                                  .get(slot.0 as usize)
                                  .filter(|entry| entry.id == slot)
                                  .map(|entry| entry.value_type)
    else
    {
      self.report(DiagnosticCode::StackSlotInvalid,
                  &format!("{label} references an unknown stack slot"));
      return;
    };
    if value_type(function, value) != Some(slot_type)
    {
      self.report(DiagnosticCode::MemoryTypeMismatch,
                  &format!("{label} value type must match the stack slot type"));
    }
  }

  fn terminator(&mut self, function: &Function, terminator: Option<&Terminator>, blocks: &HashSet<crate::xlil::BlockId>)
  {
    let Some(terminator) = terminator
    else
    {
      self.report(DiagnosticCode::MissingTerminator, "XLIL block must have a terminator");
      return;
    };
    match terminator
    {
      Terminator::Return(Some(value)) => self.return_value(function, *value),
      Terminator::Return(None) =>
      {
        if function.return_type.kind != crate::xlil::TypeKind::Void
        {
          self.report(DiagnosticCode::NonVoidReturnMissingValue,
                      "non-void XLIL function must return a value");
        }
      }
      Terminator::Branch(target) =>
      {
        if !blocks.contains(target)
        {
          self.report(DiagnosticCode::BranchTargetUnknown,
                      "XLIL branch target must reference an existing block");
        }
      }
      Terminator::BranchIf { condition,
                             then_block,
                             else_block, } =>
      {
        self.bool_value(function, *condition, "XLIL br_if condition");
        if !blocks.contains(then_block)
        {
          self.report(DiagnosticCode::BranchTargetUnknown,
                      "XLIL br_if then target must reference an existing block");
        }
        if !blocks.contains(else_block)
        {
          self.report(DiagnosticCode::BranchTargetUnknown,
                      "XLIL br_if else target must reference an existing block");
        }
      }
      Terminator::Panic =>
      {}
    }
  }

  fn return_value(&mut self, function: &Function, value: ValueId)
  {
    let Some(value_type) = value_type(function, value)
    else
    {
      self.report(DiagnosticCode::ReturnValueUnknown,
                  "XLIL return references an unknown value");
      return;
    };
    if function.return_type.kind == crate::xlil::TypeKind::Void
    {
      self.report(DiagnosticCode::VoidReturnValue,
                  "void XLIL function cannot return a value");
      return;
    }
    if value_type != function.return_type
    {
      self.report(DiagnosticCode::ReturnValueTypeMismatch,
                  "XLIL return value type must match function return type");
    }
  }

  fn i64_value(&mut self, function: &Function, value: ValueId, label: &str)
  {
    match value_type(function, value)
    {
      Some(crate::xlil::Type::I64) =>
      {}
      Some(_) | None => self.report(DiagnosticCode::InstructionResultUnknown,
                                    &format!("{label} must reference an i64 value")),
    }
  }

  fn i32_value(&mut self, function: &Function, value: ValueId, label: &str)
  {
    match value_type(function, value)
    {
      Some(crate::xlil::Type::I32) =>
      {}
      Some(_) | None => self.report(DiagnosticCode::InstructionResultUnknown,
                                    &format!("{label} must reference an i32 value")),
    }
  }

  fn bool_value(&mut self, function: &Function, value: ValueId, label: &str)
  {
    match value_type(function, value)
    {
      Some(crate::xlil::Type::BOOL) =>
      {}
      Some(_) | None => self.report(DiagnosticCode::InstructionResultUnknown,
                                    &format!("{label} must reference a bool value")),
    }
  }

  fn typed_value(&mut self, function: &Function, value: ValueId, expected: Type, label: &str)
  {
    if value_type(function, value) != Some(expected)
    {
      self.report(DiagnosticCode::InstructionResultUnknown,
                  &format!("{label} must reference an {} value", type_name(expected)));
    }
  }

  fn floating_operation_type(&mut self, value_type: Type, instruction: &str) -> bool
  {
    if matches!(value_type, Type::F32 | Type::F64)
    {
      return true;
    }
    self.report(DiagnosticCode::InstructionResultUnknown,
                &format!("XLIL {instruction} must declare f32 or f64 operand type"));
    false
  }

  fn report(&mut self, code: DiagnosticCode, message: &str)
  {
    self.diagnostics.push(Diagnostic { code,
                                       message: message.to_string() });
  }
}

fn value_type(function: &Function, value: ValueId) -> Option<crate::xlil::Type>
{
  function.values
          .iter()
          .find(|candidate| candidate.id == value)
          .map(|value| value.value_type)
}

#[cfg(test)]
mod tests
{
  use super::*;
  use crate::xlil::{Block, BlockId, Function, Instruction, Module, Terminator, Type, Value, ValueId};

  fn valid_module() -> Module
  {
    let mut module = Module::new("App");
    let mut function = Function::definition("xs$App$Main", Type::VOID, vec![]);
    let block = function.append_block("entry");
    assert!(function.set_return(block, None));
    module.add_function(function);
    module
  }

  #[test]
  fn accepts_valid_void_function()
  {
    assert!(verify_module(&valid_module()).is_empty());
  }

  #[test]
  fn rejects_duplicate_function_names()
  {
    let mut module = Module::new("App");
    module.add_function(Function::declaration("same", Type::VOID, vec![]));
    module.add_function(Function::declaration("same", Type::VOID, vec![]));

    let diagnostics = verify_module(&module);

    assert!(diagnostics.iter()
                       .any(|diagnostic| diagnostic.code == DiagnosticCode::DuplicateFunctionName));
  }

  #[test]
  fn rejects_definition_without_blocks()
  {
    let mut module = Module::new("App");
    module.add_function(Function::definition("empty", Type::VOID, vec![]));

    let diagnostics = verify_module(&module);

    assert_eq!(diagnostics[0].code, DiagnosticCode::DefinitionHasNoBlocks);
  }

  #[test]
  fn rejects_missing_block_terminator()
  {
    let mut module = Module::new("App");
    let mut function = Function::definition("bad", Type::VOID, vec![]);
    function.append_block("entry");
    module.add_function(function);

    let diagnostics = verify_module(&module);

    assert_eq!(diagnostics[0].code, DiagnosticCode::MissingTerminator);
  }

  #[test]
  fn rejects_unknown_return_value()
  {
    let mut module = Module::new("App");
    let mut function = Function::definition("bad", Type::I64, vec![]);
    function.blocks.push(Block { id: BlockId(0),
                                 label: "entry".to_string(),
                                 instructions: vec![],
                                 terminator: Some(Terminator::Return(Some(ValueId(99)))) });
    module.add_function(function);

    let diagnostics = verify_module(&module);

    assert_eq!(diagnostics[0].code, DiagnosticCode::ReturnValueUnknown);
  }

  #[test]
  fn rejects_instruction_result_without_value_record()
  {
    let mut module = Module::new("App");
    let mut function = Function::definition("bad", Type::VOID, vec![]);
    function.blocks.push(Block { id: BlockId(0),
                                 label: "entry".to_string(),
                                 instructions: vec![Instruction::ConstI64 { result: ValueId(0),
                                                                            value: 42 }],
                                 terminator: Some(Terminator::Return(None)) });
    module.add_function(function);

    let diagnostics = verify_module(&module);

    assert_eq!(diagnostics[0].code, DiagnosticCode::InstructionResultUnknown);
  }

  #[test]
  fn accepts_add_i64_instruction()
  {
    let mut module = Module::new("App");
    let mut function = Function::definition("add", Type::I64, vec![]);
    let block = function.append_block("entry");
    let left = function.add_const_i64(block, 2).expect("left const should be added");
    let right = function.add_const_i64(block, 3).expect("right const should be added");
    let result = function.add_i64(block, left, right).expect("add should be added");
    assert!(function.set_return(block, Some(result)));
    module.add_function(function);

    assert!(verify_module(&module).is_empty());
  }

  #[test]
  fn accepts_sub_i64_instruction()
  {
    let mut module = Module::new("App");
    let mut function = Function::definition("sub", Type::I64, vec![]);
    let block = function.append_block("entry");
    let left = function.add_const_i64(block, 8).expect("left const should be added");
    let right = function.add_const_i64(block, 3).expect("right const should be added");
    let result = function.sub_i64(block, left, right).expect("sub should be added");
    assert!(function.set_return(block, Some(result)));
    module.add_function(function);

    assert!(verify_module(&module).is_empty());
  }

  #[test]
  fn accepts_mul_i64_instruction()
  {
    let mut module = Module::new("App");
    let mut function = Function::definition("mul", Type::I64, vec![]);
    let block = function.append_block("entry");
    let left = function.add_const_i64(block, 6).expect("left const should be added");
    let right = function.add_const_i64(block, 7).expect("right const should be added");
    let result = function.mul_i64(block, left, right).expect("mul should be added");
    assert!(function.set_return(block, Some(result)));
    module.add_function(function);

    assert!(verify_module(&module).is_empty());
  }

  #[test]
  fn accepts_eq_i64_instruction()
  {
    let mut module = Module::new("App");
    let mut function = Function::definition("eq", Type::BOOL, vec![]);
    let block = function.append_block("entry");
    let left = function.add_const_i64(block, 7).expect("left const should be added");
    let right = function.add_const_i64(block, 7).expect("right const should be added");
    let result = function.eq_i64(block, left, right).expect("eq should be added");
    assert!(function.set_return(block, Some(result)));
    module.add_function(function);

    assert!(verify_module(&module).is_empty());
  }

  #[test]
  fn accepts_i32_instruction_family()
  {
    let mut module = Module::new("App");
    let mut function = Function::definition("i32_ops", crate::xlil::Type::BOOL, vec![]);
    let block = function.append_block("entry");
    let left = function.add_const_i32(block, 2).expect("left const should be added");
    let right = function.add_const_i32(block, 3).expect("right const should be added");
    assert!(function.add_i32(block, left, right).is_some());
    assert!(function.sub_i32(block, left, right).is_some());
    assert!(function.mul_i32(block, left, right).is_some());
    assert!(function.eq_i32(block, left, right).is_some());
    assert!(function.lt_i32(block, left, right).is_some());
    assert!(function.le_i32(block, left, right).is_some());
    assert!(function.gt_i32(block, left, right).is_some());
    let result = function.ge_i32(block, left, right);
    assert!(function.set_return(block, result));
    module.add_function(function);

    assert!(verify_module(&module).is_empty());
  }

  #[test]
  fn accepts_forward_call_with_matching_signature()
  {
    let mut module = Module::new("App");
    let mut caller = Function::definition("Caller", Type::I64, vec![]);
    let block = caller.append_block("entry");
    let argument = caller.add_const_i64(block, 7).expect("argument should be created");
    let result = caller.add_call(block, "Callee", vec![argument], Type::I64)
                       .expect("call should be created");
    assert!(caller.set_return(block, result));
    module.add_function(caller);
    module.add_function(Function::declaration("Callee", Type::I64, vec![Type::I64]));

    assert!(verify_module(&module).is_empty());
  }

  #[test]
  fn rejects_unknown_call_target()
  {
    let mut module = Module::new("App");
    let mut caller = Function::definition("Caller", Type::VOID, vec![]);
    let block = caller.append_block("entry");
    assert!(caller.add_call(block, "Missing", vec![], Type::VOID).is_some());
    assert!(caller.set_return(block, None));
    module.add_function(caller);

    let diagnostics = verify_module(&module);

    assert!(diagnostics.iter()
                       .any(|diagnostic| diagnostic.code == DiagnosticCode::CallTargetUnknown));
  }

  #[test]
  fn rejects_call_argument_and_return_type_mismatches()
  {
    let mut module = Module::new("App");
    let mut caller = Function::definition("Caller", Type::VOID, vec![]);
    let block = caller.append_block("entry");
    let argument = caller.add_const_bool(block, true).expect("argument should be created");
    assert!(caller.add_call(block, "Callee", vec![argument], Type::BOOL).is_some());
    assert!(caller.set_return(block, None));
    module.add_function(caller);
    module.add_function(Function::declaration("Callee", Type::I64, vec![Type::I64]));

    let diagnostics = verify_module(&module);

    assert!(diagnostics.iter()
                       .any(|diagnostic| diagnostic.code == DiagnosticCode::CallArgumentTypeMismatch));
    assert!(diagnostics.iter()
                       .any(|diagnostic| diagnostic.code == DiagnosticCode::CallResultTypeMismatch));
  }

  #[test]
  fn rejects_void_call_with_result()
  {
    let mut module = Module::new("App");
    let mut caller = Function::definition("Caller", Type::I64, vec![]);
    let block = caller.append_block("entry");
    let result = caller.add_call(block, "Sink", vec![], Type::I64)
                       .expect("call should be created");
    assert!(caller.set_return(block, result));
    module.add_function(caller);
    module.add_function(Function::declaration("Sink", Type::VOID, vec![]));

    let diagnostics = verify_module(&module);

    assert!(diagnostics.iter()
                       .any(|diagnostic| diagnostic.code == DiagnosticCode::CallVoidResultMismatch));
  }

  #[test]
  fn accepts_branch_if_with_bool_condition()
  {
    let mut module = Module::new("App");
    let mut function = Function::definition("branch_if", Type::VOID, vec![]);
    let entry = function.append_block("entry");
    let then_block = function.append_block("then");
    let else_block = function.append_block("else");
    let condition = function.add_const_bool(entry, true)
                            .expect("bool const should be added");
    assert!(function.set_branch_if(entry, condition, then_block, else_block));
    assert!(function.set_return(then_block, None));
    assert!(function.set_return(else_block, None));
    module.add_function(function);

    assert!(verify_module(&module).is_empty());
  }

  #[test]
  fn accepts_panic_terminator()
  {
    let mut module = Module::new("App");
    let mut function = Function::definition("panic", Type::VOID, vec![]);
    let entry = function.append_block("entry");
    assert!(function.set_panic(entry));
    module.add_function(function);

    assert!(verify_module(&module).is_empty());
  }

  #[test]
  fn rejects_branch_if_non_bool_and_unknown_targets()
  {
    let mut module = Module::new("App");
    let mut function = Function::definition("bad_branch_if", Type::VOID, vec![]);
    function.values.push(Value { id: ValueId(0),
                                 value_type: Type::I64 });
    function.blocks.push(Block { id: BlockId(0),
                                 label: "entry".to_string(),
                                 instructions: vec![],
                                 terminator: Some(Terminator::BranchIf { condition: ValueId(0),
                                                                         then_block: BlockId(1),
                                                                         else_block: BlockId(2) }) });
    module.add_function(function);

    let diagnostics = verify_module(&module);

    assert_eq!(diagnostics[0].code, DiagnosticCode::InstructionResultUnknown);
    assert_eq!(diagnostics[1].code, DiagnosticCode::BranchTargetUnknown);
    assert_eq!(diagnostics[2].code, DiagnosticCode::BranchTargetUnknown);
  }

  #[test]
  fn rejects_return_type_mismatch()
  {
    let mut module = Module::new("App");
    let mut function = Function::definition("bad", Type::VOID, vec![]);
    function.values.push(Value { id: ValueId(0),
                                 value_type: Type::I64 });
    function.blocks.push(Block { id: BlockId(0),
                                 label: "entry".to_string(),
                                 instructions: vec![],
                                 terminator: Some(Terminator::Return(Some(ValueId(0)))) });
    module.add_function(function);

    let diagnostics = verify_module(&module);

    assert_eq!(diagnostics[0].code, DiagnosticCode::VoidReturnValue);
  }

  #[test]
  fn rejects_unknown_branch_target()
  {
    let mut module = Module::new("App");
    let mut function = Function::definition("bad", Type::VOID, vec![]);
    function.blocks.push(Block { id: BlockId(0),
                                 label: "entry".to_string(),
                                 instructions: vec![],
                                 terminator: Some(Terminator::Branch(BlockId(99))) });
    module.add_function(function);

    let diagnostics = verify_module(&module);

    assert_eq!(diagnostics[0].code, DiagnosticCode::BranchTargetUnknown);
  }
}
