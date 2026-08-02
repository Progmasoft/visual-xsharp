/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::*;

impl Parser<'_>
{
    pub(super) fn extended_i64(
        &mut self,
        function: &mut Function,
        result: &str,
        operands: &str,
        instruction: &str,
        line: usize,
    ) -> Option<Instruction>
    {
        let result_type = if I64BinaryOperation::parse_text(instruction).is_some()
        {
            Type::I64
        }
        else if I64ComparisonOperation::parse_text(instruction).is_some()
        {
            Type::BOOL
        }
        else
        {
            return None;
        };
        let suffix = if result_type == Type::I64
        {
            ":i64"
        }
        else
        {
            ":bool"
        };
        let Some(result) = result.strip_suffix(suffix)
        else
        {
            self.report(
                DiagnosticCode::InvalidInstruction,
                line,
                &format!("XLIL {instruction} result type is invalid"),
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
                &format!("XLIL {instruction} operands are invalid"),
            );
            return None;
        };
        let left = self.value_operand(left, line)?;
        let right = self.value_operand(right, line)?;
        function.values.push(Value {
            id: result,
            value_type: result_type,
        });
        if let Some(operation) = I64BinaryOperation::parse_text(instruction)
        {
            Some(Instruction::BinaryI64 {
                operation,
                result,
                left,
                right,
            })
        }
        else
        {
            Some(Instruction::CompareI64 {
                operation: I64ComparisonOperation::parse_text(instruction)?,
                result,
                left,
                right,
            })
        }
    }
}
