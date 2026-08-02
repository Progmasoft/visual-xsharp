/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use crate::xlil::{Function, Instruction, IntegerBinaryOperation, Type, Value, type_from_name};

use super::{DiagnosticCode, Parser};

impl Parser<'_>
{
    pub(super) fn integer_operation(&mut self, function: &mut Function, text: &str, line: usize)
    -> Option<Instruction>
    {
        let (result, operation) = text.split_once(" = ")?;
        let (name, operands) = operation.split_once(' ')?;
        let (stem, type_name) = name.split_once('.')?;
        let operation = IntegerBinaryOperation::parse_text_stem(stem)?;
        let value_type = type_from_name(type_name)?;
        if !value_type.is_integer()
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
        let expected_suffix = format!(":{}", crate::xlil::type_name(result_type));
        let Some(result) = result.strip_suffix(&expected_suffix)
        else
        {
            self.report(
                DiagnosticCode::InvalidInstruction,
                line,
                "XLIL integer operation result type is invalid",
            );
            return None;
        };
        let result = self.value_id(result, line)?;
        let Some((left, right)) = operands.split_once(", ")
        else
        {
            self.report(
                DiagnosticCode::InvalidInstruction,
                line,
                "XLIL integer operation operands are invalid",
            );
            return None;
        };
        let left = self.value_operand(left, line)?;
        let right = self.value_operand(right, line)?;
        function.values.push(Value {
            id: result,
            value_type: result_type,
        });
        Some(Instruction::BinaryInteger {
            operation,
            value_type,
            result,
            left,
            right,
        })
    }
}
