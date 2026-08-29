/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

use super::{DiagnosticCode, Parser};
use crate::xlil::{FloatBinaryOperation, FloatComparisonOperation, Function, Instruction, Type, Value, type_name};

impl Parser<'_>
{
    pub(super) fn float_binary(
        &mut self,
        function: &mut Function,
        result: &str,
        operands: &str,
        operation: &str,
        value_type: Type,
        line: usize,
    ) -> Option<Instruction>
    {
        let comparison = FloatComparisonOperation::parse_text_stem(operation);
        let result_type = if comparison.is_some()
        {
            Type::BOOL
        }
        else
        {
            value_type
        };
        let result = self.value_id(result.strip_suffix(&format!(":{}", type_name(result_type)))?, line)?;
        let (left, right) = operands.split_once(", ")?;
        let left = self.value_operand(left, line)?;
        let right = self.value_operand(right, line)?;
        function.values.push(Value {
            id: result,
            value_type: result_type,
        });
        if let Some(operation) = comparison
        {
            Some(Instruction::CompareFloat {
                operation,
                value_type,
                result,
                left,
                right,
            })
        }
        else
        {
            Some(Instruction::BinaryFloat {
                operation: FloatBinaryOperation::parse_text_stem(operation)?,
                value_type,
                result,
                left,
                right,
            })
        }
    }

    pub(super) fn const_float(
        &mut self,
        function: &mut Function,
        result: &str,
        value: &str,
        value_type: Type,
        line: usize,
    ) -> Option<Instruction>
    {
        let suffix = format!(":{}", type_name(value_type));
        let result = self.value_id(result.strip_suffix(&suffix)?, line)?;
        let digits = if value_type == Type::F32
        {
            8
        }
        else
        {
            16
        };
        let hexadecimal = value.strip_prefix("0x")?;
        if hexadecimal.len() != digits
        {
            self.report(
                DiagnosticCode::InvalidInstruction,
                line,
                "XLIL floating constant has the wrong bit width",
            );
            return None;
        }
        let bits = u64::from_str_radix(hexadecimal, 16).ok()?;
        function.values.push(Value {
            id: result,
            value_type,
        });
        Some(
            if value_type == Type::F32
            {
                Instruction::ConstF32 {
                    result,
                    bits: bits as u32,
                }
            }
            else
            {
                Instruction::ConstF64 {
                    result,
                    bits,
                }
            },
        )
    }
}
