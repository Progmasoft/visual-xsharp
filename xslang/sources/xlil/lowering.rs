/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

pub(crate) use std::collections::HashMap;

use crate::hir::async_check::Span;
use crate::mir;
use crate::xlil::{
    BlockId, Function, I32BinaryOperation, IntegerBinaryOperation, IntegerConstant, SlotId, Type, Utf32Encoding,
    ValueId,
};

mod aggregate;
mod integer;
mod storage;
mod string;

use storage::storage_locals;

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum DiagnosticCode
{
    UnsupportedReturnValue,
    MissingLocalType,
    UnsupportedLocalType,
    MissingLocalValue,
    MissingMirTerminator,
    MissingBranchTarget,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct Diagnostic
{
    pub code: DiagnosticCode,
    pub message: String,
    pub span: Span,
}

#[derive(Default)]
pub struct MirToXlilLowerer
{
    diagnostics: Vec<Diagnostic>,
    utf32_encoding: Utf32Encoding,
}

impl MirToXlilLowerer
{
    #[must_use]
    pub fn new() -> Self
    {
        Self::default()
    }

    #[must_use]
    #[cfg(test)]
    pub fn with_utf32_encoding(mut self, encoding: Utf32Encoding) -> Self
    {
        self.utf32_encoding = encoding;
        self
    }

    pub fn lower_function(mut self, function: &mir::Function) -> Result<Function, Vec<Diagnostic>>
    {
        let mut local_types = local_types(function);
        let parameters = function
            .parameters
            .iter()
            .map(|parameter| parameter.value_type)
            .collect();
        let mut lowered = Function::definition(function.name.clone(), function.return_type, parameters);
        let mut blocks = HashMap::new();
        let mut values = HashMap::new();
        let mut slots = HashMap::new();
        for (index, parameter) in function.parameters.iter().enumerate()
        {
            local_types.insert(parameter.local, Some(parameter.value_type));
            if let Some(value) = lowered.parameter_value(index)
            {
                values.insert(parameter.local, value);
            }
        }
        for block in &function.blocks
        {
            let xlil_block = lowered.append_block(format!("bb{}", block.id.0));
            blocks.insert(block.id, xlil_block);
        }
        let storage = storage_locals(function);
        for local in &function.locals
        {
            if storage.contains(&local.id) &&
                let Some(value_type) = local.value_type &&
                let Some(slot) = lowered.add_slot(value_type)
            {
                slots.insert(local.id, slot);
            }
        }
        for block in &function.blocks
        {
            let xlil_block = blocks[&block.id];
            self.lower_statements(block, xlil_block, &local_types, &slots, &mut values, &mut lowered);
            self.lower_terminator(block, xlil_block, &blocks, &local_types, &values, &mut lowered);
        }
        if self.diagnostics.is_empty()
        {
            Ok(lowered)
        }
        else
        {
            Err(self.diagnostics)
        }
    }

    fn lower_statements(
        &mut self,
        block: &mir::BasicBlock,
        xlil_block: BlockId,
        local_types: &HashMap<mir::LocalId, Option<Type>>,
        slots: &HashMap<mir::LocalId, SlotId>,
        values: &mut HashMap<mir::LocalId, ValueId>,
        lowered: &mut Function,
    )
    {
        for statement in &block.statements
        {
            self.lower_aggregate_statement(statement, xlil_block, values, lowered);
            if let mir::Statement::ConstI64 {
                local,
                value,
                span,
            } = *statement
            {
                self.lower_const_i64(local, value, span, xlil_block, local_types, values, lowered);
            }
            if let mir::Statement::ConstI32 {
                local,
                value,
                span,
            } = *statement
            {
                self.lower_const_i32(local, value, span, xlil_block, local_types, values, lowered);
            }
            if let mir::Statement::ConstU16 {
                local,
                value,
                span,
            } = *statement
            {
                self.lower_const_u16(local, value, span, xlil_block, local_types, values, lowered);
            }
            if let mir::Statement::ConstInteger {
                local,
                value,
                span,
            } = *statement
            {
                self.lower_const_integer(local, value, span, xlil_block, local_types, values, lowered);
            }
            if let mir::Statement::ConstF32 {
                local,
                bits,
                span,
            } = *statement
            {
                self.lower_const_float(
                    local,
                    bits as u64,
                    Type::F32,
                    span,
                    xlil_block,
                    local_types,
                    values,
                    lowered,
                );
            }
            if let mir::Statement::ConstF64 {
                local,
                bits,
                span,
            } = *statement
            {
                self.lower_const_float(local, bits, Type::F64, span, xlil_block, local_types, values, lowered);
            }
            if let mir::Statement::ConstStr {
                local,
                ref units,
                span,
            } = *statement
            {
                self.lower_const_str(local, units, span, xlil_block, local_types, values, lowered);
            }
            if let mir::Statement::ConstBool {
                local,
                value,
                span,
            } = *statement
            {
                self.lower_const_bool(local, value, span, xlil_block, local_types, values, lowered);
            }
            if let mir::Statement::BinaryInteger {
                operation,
                value_type,
                result,
                left,
                right,
                span,
            } = *statement
            {
                self.lower_binary_integer(
                    result, left, right, value_type, operation, span, xlil_block, values, lowered,
                );
            }
            if let mir::Statement::BinaryFloat {
                operation,
                value_type,
                result,
                left,
                right,
                span,
            } = *statement
            {
                self.lower_float_operation(
                    result,
                    left,
                    right,
                    span,
                    "floating instruction",
                    xlil_block,
                    values,
                    lowered,
                    |function, block, left, right| function.binary_float(block, operation, value_type, left, right),
                );
            }
            if let mir::Statement::CompareFloat {
                operation,
                value_type,
                result,
                left,
                right,
                span,
            } = *statement
            {
                self.lower_float_operation(
                    result,
                    left,
                    right,
                    span,
                    "floating comparison",
                    xlil_block,
                    values,
                    lowered,
                    |function, block, left, right| function.compare_float(block, operation, value_type, left, right),
                );
            }
            if let mir::Statement::CompareStr {
                operation,
                result,
                left,
                right,
                span,
            } = *statement
            {
                self.lower_str_comparison(operation, result, left, right, span, xlil_block, values, lowered);
            }
            if let mir::Statement::StoreLocal {
                local,
                value,
                span,
            } = *statement
            {
                let Some(slot) = slots.get(&local).copied()
                else
                {
                    self.report(
                        DiagnosticCode::MissingLocalValue,
                        "MIR store.local target has no XLIL stack slot",
                        span,
                    );
                    continue;
                };
                let Some(value) = values.get(&value).copied()
                else
                {
                    self.report(
                        DiagnosticCode::MissingLocalValue,
                        "MIR store.local value has not been lowered",
                        span,
                    );
                    continue;
                };
                if !lowered.add_store(xlil_block, slot, value)
                {
                    self.report(
                        DiagnosticCode::UnsupportedLocalType,
                        "MIR store.local could not lower to XLIL",
                        span,
                    );
                }
            }
            if let mir::Statement::LoadLocal {
                result,
                local,
                span,
            } = *statement
            {
                let Some(slot) = slots.get(&local).copied()
                else
                {
                    self.report(
                        DiagnosticCode::MissingLocalValue,
                        "MIR load.local source has no XLIL stack slot",
                        span,
                    );
                    continue;
                };
                let Some(value) = lowered.add_load(xlil_block, slot)
                else
                {
                    self.report(
                        DiagnosticCode::UnsupportedLocalType,
                        "MIR load.local could not lower to XLIL",
                        span,
                    );
                    continue;
                };
                values.insert(result, value);
            }
            if let mir::Statement::AddI64 {
                result,
                left,
                right,
                span,
            } = *statement
            {
                self.lower_binary_i64(result, left, right, span, "add.i64", xlil_block, values, lowered);
            }
            if let mir::Statement::SubI64 {
                result,
                left,
                right,
                span,
            } = *statement
            {
                self.lower_binary_i64(result, left, right, span, "sub.i64", xlil_block, values, lowered);
            }
            if let mir::Statement::MulI64 {
                result,
                left,
                right,
                span,
            } = *statement
            {
                self.lower_binary_i64(result, left, right, span, "mul.i64", xlil_block, values, lowered);
            }
            if let mir::Statement::EqI64 {
                result,
                left,
                right,
                span,
            } = *statement
            {
                self.lower_eq_i64(result, left, right, span, xlil_block, values, lowered);
            }
            if let mir::Statement::BinaryI64 {
                operation,
                result,
                left,
                right,
                span,
            } = *statement
            {
                self.lower_binary_i64(
                    result,
                    left,
                    right,
                    span,
                    operation.text_name(),
                    xlil_block,
                    values,
                    lowered,
                );
            }
            if let mir::Statement::CompareI64 {
                operation,
                result,
                left,
                right,
                span,
            } = *statement
            {
                self.lower_compare_i64(result, left, right, span, operation, xlil_block, values, lowered);
            }
            if let mir::Statement::AddI32 {
                result,
                left,
                right,
                span,
            } = *statement
            {
                self.lower_binary_i32(result, left, right, span, "add.i32", xlil_block, values, lowered);
            }
            if let mir::Statement::SubI32 {
                result,
                left,
                right,
                span,
            } = *statement
            {
                self.lower_binary_i32(result, left, right, span, "sub.i32", xlil_block, values, lowered);
            }
            if let mir::Statement::MulI32 {
                result,
                left,
                right,
                span,
            } = *statement
            {
                self.lower_binary_i32(result, left, right, span, "mul.i32", xlil_block, values, lowered);
            }
            if let mir::Statement::BinaryI32 {
                operation,
                result,
                left,
                right,
                span,
            } = *statement
            {
                self.lower_binary_i32(
                    result,
                    left,
                    right,
                    span,
                    operation.text_name(),
                    xlil_block,
                    values,
                    lowered,
                );
            }
            if let mir::Statement::EqI32 {
                result,
                left,
                right,
                span,
            } = *statement
            {
                self.lower_binary_i32(result, left, right, span, "eq.i32", xlil_block, values, lowered);
            }
            if let mir::Statement::LtI32 {
                result,
                left,
                right,
                span,
            } = *statement
            {
                self.lower_binary_i32(result, left, right, span, "lt.i32", xlil_block, values, lowered);
            }
            if let mir::Statement::LeI32 {
                result,
                left,
                right,
                span,
            } = *statement
            {
                self.lower_binary_i32(result, left, right, span, "le.i32", xlil_block, values, lowered);
            }
            if let mir::Statement::GtI32 {
                result,
                left,
                right,
                span,
            } = *statement
            {
                self.lower_binary_i32(result, left, right, span, "gt.i32", xlil_block, values, lowered);
            }
            if let mir::Statement::GeI32 {
                result,
                left,
                right,
                span,
            } = *statement
            {
                self.lower_binary_i32(result, left, right, span, "ge.i32", xlil_block, values, lowered);
            }
            if let mir::Statement::NotBool {
                result,
                operand,
                span,
            } = *statement
            {
                let Some(operand) = values.get(&operand).copied()
                else
                {
                    self.report(
                        DiagnosticCode::MissingLocalValue,
                        "MIR not.bool operand does not have a lowered XLIL value",
                        span,
                    );
                    continue;
                };
                let Some(value) = lowered.not_bool(xlil_block, operand)
                else
                {
                    self.report(
                        DiagnosticCode::UnsupportedLocalType,
                        "MIR not.bool operand must lower to an XLIL bool value",
                        span,
                    );
                    continue;
                };
                values.insert(result, value);
            }
            if let mir::Statement::Call {
                result,
                ref function,
                ref arguments,
                return_type,
                span,
            } = *statement
            {
                self.lower_call(
                    result,
                    function,
                    arguments,
                    return_type,
                    span,
                    xlil_block,
                    values,
                    lowered,
                );
            }
        }
    }

    #[allow(clippy::too_many_arguments)]
    fn lower_const_str(
        &mut self,
        local: mir::LocalId,
        units: &[u32],
        span: Span,
        block: BlockId,
        local_types: &HashMap<mir::LocalId, Option<Type>>,
        values: &mut HashMap<mir::LocalId, ValueId>,
        function: &mut Function,
    )
    {
        if local_types.get(&local) != Some(&Some(Type::STR))
        {
            self.report(
                DiagnosticCode::UnsupportedLocalType,
                "MIR const.str target must have str type",
                span,
            );
            return;
        }
        match function.add_const_str(block, self.utf32_encoding, units)
        {
            Some(result) =>
            {
                values.insert(local, result);
            }
            None => self.report(
                DiagnosticCode::MissingLocalValue,
                "could not lower MIR const.str value",
                span,
            ),
        }
    }

    #[allow(clippy::too_many_arguments)]
    fn lower_const_i64(
        &mut self,
        local: mir::LocalId,
        value: i64,
        span: Span,
        xlil_block: BlockId,
        local_types: &HashMap<mir::LocalId, Option<Type>>,
        values: &mut HashMap<mir::LocalId, ValueId>,
        lowered: &mut Function,
    )
    {
        match local_types.get(&local).copied().flatten()
        {
            Some(Type::I64) =>
            {
                if let Some(value_id) = lowered.add_const_i64(xlil_block, value)
                {
                    values.insert(local, value_id);
                }
            }
            Some(_) => self.report(
                DiagnosticCode::UnsupportedLocalType,
                "MIR const.i64 target local must have XLIL i64 type",
                span,
            ),
            None => self.report(
                DiagnosticCode::MissingLocalType,
                "MIR const.i64 target local has no XLIL value type",
                span,
            ),
        }
    }

    #[allow(clippy::too_many_arguments)]
    fn lower_const_bool(
        &mut self,
        local: mir::LocalId,
        value: bool,
        span: Span,
        xlil_block: BlockId,
        local_types: &HashMap<mir::LocalId, Option<Type>>,
        values: &mut HashMap<mir::LocalId, ValueId>,
        lowered: &mut Function,
    )
    {
        match local_types.get(&local).copied().flatten()
        {
            Some(Type::BOOL) =>
            {
                if let Some(value_id) = lowered.add_const_bool(xlil_block, value)
                {
                    values.insert(local, value_id);
                }
            }
            Some(_) => self.report(
                DiagnosticCode::UnsupportedLocalType,
                "MIR const.bool target local must have XLIL bool type",
                span,
            ),
            None => self.report(
                DiagnosticCode::MissingLocalType,
                "MIR const.bool target local has no XLIL value type",
                span,
            ),
        }
    }

    #[allow(clippy::too_many_arguments)]
    fn lower_const_i32(
        &mut self,
        local: mir::LocalId,
        value: i32,
        span: Span,
        xlil_block: BlockId,
        local_types: &HashMap<mir::LocalId, Option<Type>>,
        values: &mut HashMap<mir::LocalId, ValueId>,
        lowered: &mut Function,
    )
    {
        match local_types.get(&local).copied().flatten()
        {
            Some(Type::I32) =>
            {
                if let Some(value_id) = lowered.add_const_i32(xlil_block, value)
                {
                    values.insert(local, value_id);
                }
            }
            Some(_) => self.report(
                DiagnosticCode::UnsupportedLocalType,
                "MIR const.i32 target local must have XLIL i32 type",
                span,
            ),
            None => self.report(
                DiagnosticCode::MissingLocalType,
                "MIR const.i32 target local has no XLIL value type",
                span,
            ),
        }
    }

    #[allow(clippy::too_many_arguments)]
    fn lower_const_float(
        &mut self,
        local: mir::LocalId,
        bits: u64,
        expected: Type,
        span: Span,
        xlil_block: BlockId,
        local_types: &HashMap<mir::LocalId, Option<Type>>,
        values: &mut HashMap<mir::LocalId, ValueId>,
        lowered: &mut Function,
    )
    {
        if local_types.get(&local).copied().flatten() != Some(expected)
        {
            self.report(
                DiagnosticCode::UnsupportedLocalType,
                "MIR floating constant target has the wrong XLIL type",
                span,
            );
            return;
        }
        let value = match expected
        {
            Type::F32 => lowered.add_const_f32_bits(xlil_block, bits as u32),
            Type::F64 => lowered.add_const_f64_bits(xlil_block, bits),
            _ => None,
        };
        if let Some(value) = value
        {
            values.insert(local, value);
        }
    }
}
include!("lowering/calls_and_control.rs");
