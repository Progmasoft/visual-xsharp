/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
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
                self.report(
                    DiagnosticCode::InvalidAggregateType,
                    "XLIL aggregate type ids must be sequential and names must not be empty",
                );
            }
            if !aggregate_names.insert(aggregate.name.as_str())
            {
                self.report(
                    DiagnosticCode::DuplicateAggregateName,
                    "XLIL aggregate type names must be unique",
                );
            }
            for field in &aggregate.fields
            {
                if *field == Type::VOID || !Self::valid_type(module, *field)
                {
                    self.report(
                        DiagnosticCode::InvalidAggregateType,
                        "XLIL aggregate fields must reference known non-void types",
                    );
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
                self.report(
                    DiagnosticCode::InvalidArrayType,
                    "XLIL array registry requires sequential ids, a non-void element type, and nonzero fixed length",
                );
            }
        }
        for function in &module.functions
        {
            if !names.insert(function.name.as_str())
            {
                self.report(
                    DiagnosticCode::DuplicateFunctionName,
                    "XLIL function names must be unique",
                );
            }
            if !Self::valid_type(module, function.return_type) ||
                function
                    .parameters
                    .iter()
                    .any(|value_type| !Self::valid_type(module, *value_type)) ||
                function
                    .values
                    .iter()
                    .any(|value| !Self::valid_type(module, value.value_type)) ||
                function
                    .slots
                    .iter()
                    .any(|slot| !Self::valid_type(module, slot.value_type))
            {
                self.report(
                    DiagnosticCode::InvalidAggregateType,
                    "XLIL function types must reference the module type registry",
                );
            }
            self.function(function, module);
        }
    }

    fn valid_type(module: &Module, value_type: Type) -> bool
    {
        match value_type.kind
        {
            TypeKind::Aggregate => module
                .aggregate_types
                .get(value_type.registry_id as usize)
                .is_some_and(|entry| entry.id == value_type.registry_id),
            TypeKind::Array => module
                .array_types
                .get(value_type.registry_id as usize)
                .is_some_and(|entry| entry.id == value_type.registry_id),
            _ => true,
        }
    }

    fn function(&mut self, function: &Function, module: &Module)
    {
        if function.name.is_empty()
        {
            self.report(
                DiagnosticCode::EmptyFunctionName,
                "XLIL function name must not be empty",
            );
        }
        if !function.is_definition && !function.blocks.is_empty()
        {
            self.report(
                DiagnosticCode::DeclarationHasBody,
                "XLIL function declaration must not contain blocks",
            );
        }
        if function.is_definition && function.blocks.is_empty()
        {
            self.report(
                DiagnosticCode::DefinitionHasNoBlocks,
                "XLIL function definition must contain at least one block",
            );
        }
        for (index, slot) in function.slots.iter().enumerate()
        {
            if slot.id != SlotId(index as u32) || slot.value_type == Type::VOID
            {
                self.report(
                    DiagnosticCode::StackSlotInvalid,
                    "XLIL stack slots must be sequential and have non-void types",
                );
            }
        }
        let blocks = function.blocks.iter().map(|block| block.id).collect::<HashSet<_>>();
        let mut seen_blocks = HashSet::new();
        for block in &function.blocks
        {
            if !seen_blocks.insert(block.id)
            {
                self.report(
                    DiagnosticCode::DuplicateBlockId,
                    "XLIL block ids must be unique within a function",
                );
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
            Instruction::ConstI64 {
                result, ..
            } => self.i64_value(function, result, "XLIL const result"),
            Instruction::ConstI32 {
                result, ..
            } => self.i32_value(function, result, "XLIL const.i32 result"),
            Instruction::ConstU16 {
                result, ..
            } => self.typed_value(function, result, Type::U16, "XLIL const.u16 result"),
            Instruction::ConstInteger {
                result,
                value,
            } => self.typed_value(
                function,
                result,
                value.value_type,
                &format!("XLIL const.{} result", crate::xlil::type_name(value.value_type)),
            ),
            Instruction::ConstF32 {
                result, ..
            } => self.typed_value(function, result, Type::F32, "XLIL const.f32 result"),
            Instruction::ConstF64 {
                result, ..
            } => self.typed_value(function, result, Type::F64, "XLIL const.f64 result"),
            Instruction::ConstStr {
                result, ..
            } => self.typed_value(function, result, Type::STR, "XLIL const.str result"),
            Instruction::ConstBool {
                result, ..
            } => self.bool_value(function, result, "XLIL const.bool result"),
            Instruction::BinaryInteger {
                operation,
                value_type,
                result,
                left,
                right,
            } =>
            {
                let name = format!("{}.{}", operation.text_stem(), crate::xlil::type_name(value_type));
                if !value_type.is_integer()
                {
                    self.report(
                        DiagnosticCode::InstructionResultUnknown,
                        &format!("XLIL {name} declares a non-integer operand type"),
                    );
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
            Instruction::BinaryFloat {
                operation,
                value_type,
                result,
                left,
                right,
            } =>
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
            Instruction::CompareFloat {
                operation,
                value_type,
                result,
                left,
                right,
            } =>
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
            Instruction::CompareStr {
                operation,
                result,
                left,
                right,
            } =>
            {
                let name = format!("{}.str", operation.text_stem());
                self.bool_value(function, result, &format!("XLIL {name} result"));
                self.typed_value(function, left, Type::STR, &format!("XLIL {name} left operand"));
                self.typed_value(function, right, Type::STR, &format!("XLIL {name} right operand"));
            }
            Instruction::AddI64 {
                result,
                left,
                right,
            } =>
            {
                self.i64_value(function, result, "XLIL add.i64 result");
                self.i64_value(function, left, "XLIL add.i64 left operand");
                self.i64_value(function, right, "XLIL add.i64 right operand");
            }
            Instruction::SubI64 {
                result,
                left,
                right,
            } =>
            {
                self.i64_value(function, result, "XLIL sub.i64 result");
                self.i64_value(function, left, "XLIL sub.i64 left operand");
                self.i64_value(function, right, "XLIL sub.i64 right operand");
            }
            Instruction::MulI64 {
                result,
                left,
                right,
            } =>
            {
                self.i64_value(function, result, "XLIL mul.i64 result");
                self.i64_value(function, left, "XLIL mul.i64 left operand");
                self.i64_value(function, right, "XLIL mul.i64 right operand");
            }
            Instruction::EqI64 {
                result,
                left,
                right,
            } =>
            {
                self.bool_value(function, result, "XLIL eq.i64 result");
                self.i64_value(function, left, "XLIL eq.i64 left operand");
                self.i64_value(function, right, "XLIL eq.i64 right operand");
            }
            Instruction::BinaryI64 {
                operation,
                result,
                left,
                right,
            } =>
            {
                self.i64_value(function, result, &format!("XLIL {} result", operation.text_name()));
                self.i64_value(function, left, &format!("XLIL {} left operand", operation.text_name()));
                self.i64_value(
                    function,
                    right,
                    &format!("XLIL {} right operand", operation.text_name()),
                );
            }
            Instruction::CompareI64 {
                operation,
                result,
                left,
                right,
            } =>
            {
                self.bool_value(function, result, &format!("XLIL {} result", operation.text_name()));
                self.i64_value(function, left, &format!("XLIL {} left operand", operation.text_name()));
                self.i64_value(
                    function,
                    right,
                    &format!("XLIL {} right operand", operation.text_name()),
                );
            }
            Instruction::AddI32 {
                result,
                left,
                right,
            } =>
            {
                self.i32_value(function, result, "XLIL add.i32 result");
                self.i32_value(function, left, "XLIL add.i32 left operand");
                self.i32_value(function, right, "XLIL add.i32 right operand");
            }
            Instruction::SubI32 {
                result,
                left,
                right,
            } =>
            {
                self.i32_value(function, result, "XLIL sub.i32 result");
                self.i32_value(function, left, "XLIL sub.i32 left operand");
                self.i32_value(function, right, "XLIL sub.i32 right operand");
            }
            Instruction::MulI32 {
                result,
                left,
                right,
            } =>
            {
                self.i32_value(function, result, "XLIL mul.i32 result");
                self.i32_value(function, left, "XLIL mul.i32 left operand");
                self.i32_value(function, right, "XLIL mul.i32 right operand");
            }
            Instruction::BinaryI32 {
                operation,
                result,
                left,
                right,
            } =>
            {
                let name = operation.text_name();
                self.i32_value(function, result, &format!("XLIL {name} result"));
                self.i32_value(function, left, &format!("XLIL {name} left operand"));
                self.i32_value(function, right, &format!("XLIL {name} right operand"));
            }
            Instruction::EqI32 {
                result,
                left,
                right,
            } |
            Instruction::LtI32 {
                result,
                left,
                right,
            } |
            Instruction::LeI32 {
                result,
                left,
                right,
            } |
            Instruction::GtI32 {
                result,
                left,
                right,
            } |
            Instruction::GeI32 {
                result,
                left,
                right,
            } =>
            {
                self.bool_value(function, result, "XLIL i32 comparison result");
                self.i32_value(function, left, "XLIL i32 comparison left operand");
                self.i32_value(function, right, "XLIL i32 comparison right operand");
            }
            Instruction::NotBool {
                result,
                operand,
            } =>
            {
                self.bool_value(function, result, "XLIL not.bool result");
                self.bool_value(function, operand, "XLIL not.bool operand");
            }
            Instruction::Call {
                result,
                function: ref callee_name,
                ref arguments,
                return_type,
                ..
            } =>
            {
                let Some(callee) = module.functions.iter().find(|candidate| candidate.name == *callee_name)
                else
                {
                    self.report(
                        DiagnosticCode::CallTargetUnknown,
                        "XLIL call target must be declared in the module",
                    );
                    return;
                };
                if arguments.len() != callee.parameters.len()
                {
                    self.report(
                        DiagnosticCode::CallArgumentCountMismatch,
                        "XLIL call argument count must match the target signature",
                    );
                }
                for (argument, parameter) in arguments.iter().zip(&callee.parameters)
                {
                    if value_type(function, *argument) != Some(*parameter)
                    {
                        self.report(
                            DiagnosticCode::CallArgumentTypeMismatch,
                            "XLIL call argument type must match the target signature",
                        );
                    }
                }
                if let Some(result) = result
                {
                    if callee.return_type == crate::xlil::Type::VOID
                    {
                        self.report(
                            DiagnosticCode::CallVoidResultMismatch,
                            "void XLIL call cannot produce a result value",
                        );
                    }
                    if return_type != callee.return_type
                    {
                        self.report(
                            DiagnosticCode::CallResultTypeMismatch,
                            "XLIL call result type must match the target signature",
                        );
                    }
                    match value_type(function, result)
                    {
                        Some(value_type) if value_type == return_type =>
                        {}
                        _ => self.report(
                            DiagnosticCode::InstructionResultUnknown,
                            "XLIL call result must reference a declared value with matching type",
                        ),
                    }
                }
                else if callee.return_type != crate::xlil::Type::VOID
                {
                    self.report(
                        DiagnosticCode::CallVoidResultMismatch,
                        "non-void XLIL call must produce a result value",
                    );
                }
                for argument in arguments
                {
                    if value_type(function, *argument).is_none()
                    {
                        self.report(
                            DiagnosticCode::InstructionResultUnknown,
                            "XLIL call argument must reference a declared value",
                        );
                    }
                }
            }
            Instruction::Aggregate {
                result,
                value_type: aggregate_type,
                ref fields,
            } => self.composite(function, module, result, aggregate_type, fields),
            Instruction::Extract {
                result,
                aggregate,
                field,
            } => self.extract(function, module, result, aggregate, field),
            Instruction::ArrayGet {
                result,
                array,
                index,
            } => self.array_access(function, module, result, array, index, None),
            Instruction::ArraySet {
                result,
                array,
                index,
                value,
            } => self.array_access(function, module, result, array, index, Some(value)),
            Instruction::ArrayLength {
                result,
                array,
            } =>
            {
                self.typed_value(function, result, Type::I64, "XLIL len.array result");
                if value_type(function, array)
                    .and_then(|ty| module.array_type(ty))
                    .is_none()
                {
                    self.report(
                        DiagnosticCode::InvalidArrayType,
                        "XLIL len.array source must use a known array registry type",
                    );
                }
            }
            Instruction::Load {
                result,
                slot,
            } => self.memory(function, slot, result, "XLIL load"),
            Instruction::Store {
                slot,
                value,
            } => self.memory(function, slot, value, "XLIL store"),
        }
    }

    fn memory(&mut self, function: &Function, slot: SlotId, value: ValueId, label: &str)
    {
        let Some(slot_type) = function
            .slots
            .get(slot.0 as usize)
            .filter(|entry| entry.id == slot)
            .map(|entry| entry.value_type)
        else
        {
            self.report(
                DiagnosticCode::StackSlotInvalid,
                &format!("{label} references an unknown stack slot"),
            );
            return;
        };
        if value_type(function, value) != Some(slot_type)
        {
            self.report(
                DiagnosticCode::MemoryTypeMismatch,
                &format!("{label} value type must match the stack slot type"),
            );
        }
    }

    fn terminator(
        &mut self,
        function: &Function,
        terminator: Option<&Terminator>,
        blocks: &HashSet<crate::xlil::BlockId>,
    )
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
                    self.report(
                        DiagnosticCode::NonVoidReturnMissingValue,
                        "non-void XLIL function must return a value",
                    );
                }
            }
            Terminator::Branch(target) =>
            {
                if !blocks.contains(target)
                {
                    self.report(
                        DiagnosticCode::BranchTargetUnknown,
                        "XLIL branch target must reference an existing block",
                    );
                }
            }
            Terminator::BranchIf {
                condition,
                then_block,
                else_block,
            } =>
            {
                self.bool_value(function, *condition, "XLIL br_if condition");
                if !blocks.contains(then_block)
                {
                    self.report(
                        DiagnosticCode::BranchTargetUnknown,
                        "XLIL br_if then target must reference an existing block",
                    );
                }
                if !blocks.contains(else_block)
                {
                    self.report(
                        DiagnosticCode::BranchTargetUnknown,
                        "XLIL br_if else target must reference an existing block",
                    );
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
            self.report(
                DiagnosticCode::ReturnValueUnknown,
                "XLIL return references an unknown value",
            );
            return;
        };
        if function.return_type.kind == crate::xlil::TypeKind::Void
        {
            self.report(
                DiagnosticCode::VoidReturnValue,
                "void XLIL function cannot return a value",
            );
            return;
        }
        if value_type != function.return_type
        {
            self.report(
                DiagnosticCode::ReturnValueTypeMismatch,
                "XLIL return value type must match function return type",
            );
        }
    }

    fn i64_value(&mut self, function: &Function, value: ValueId, label: &str)
    {
        match value_type(function, value)
        {
            Some(crate::xlil::Type::I64) =>
            {}
            Some(_) | None => self.report(
                DiagnosticCode::InstructionResultUnknown,
                &format!("{label} must reference an i64 value"),
            ),
        }
    }

    fn i32_value(&mut self, function: &Function, value: ValueId, label: &str)
    {
        match value_type(function, value)
        {
            Some(crate::xlil::Type::I32) =>
            {}
            Some(_) | None => self.report(
                DiagnosticCode::InstructionResultUnknown,
                &format!("{label} must reference an i32 value"),
            ),
        }
    }

    fn bool_value(&mut self, function: &Function, value: ValueId, label: &str)
    {
        match value_type(function, value)
        {
            Some(crate::xlil::Type::BOOL) =>
            {}
            Some(_) | None => self.report(
                DiagnosticCode::InstructionResultUnknown,
                &format!("{label} must reference a bool value"),
            ),
        }
    }

    fn typed_value(&mut self, function: &Function, value: ValueId, expected: Type, label: &str)
    {
        if value_type(function, value) != Some(expected)
        {
            self.report(
                DiagnosticCode::InstructionResultUnknown,
                &format!("{label} must reference an {} value", type_name(expected)),
            );
        }
    }

    fn floating_operation_type(&mut self, value_type: Type, instruction: &str) -> bool
    {
        if matches!(value_type, Type::F32 | Type::F64)
        {
            return true;
        }
        self.report(
            DiagnosticCode::InstructionResultUnknown,
            &format!("XLIL {instruction} must declare f32 or f64 operand type"),
        );
        false
    }

    fn report(&mut self, code: DiagnosticCode, message: &str)
    {
        self.diagnostics.push(Diagnostic {
            code,
            message: message.to_string(),
        });
    }
}

fn value_type(function: &Function, value: ValueId) -> Option<crate::xlil::Type>
{
    function
        .values
        .iter()
        .find(|candidate| candidate.id == value)
        .map(|value| value.value_type)
}

include!("verify/tests.rs");
