/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::{Parser, span};
use crate::mir::Statement;
use crate::xlil::{FloatBinaryOperation, FloatComparisonOperation, Type};

pub(super) fn is_float_instruction(name: &str) -> bool
{
    let Some((operation, suffix)) = name.split_once('.')
    else
    {
        return false;
    };
    matches!(suffix, "f32" | "f64") &&
        (FloatBinaryOperation::parse_text_stem(operation).is_some() ||
            FloatComparisonOperation::parse_text_stem(operation).is_some())
}

impl Parser<'_>
{
    pub(super) fn float_statement(&mut self, instruction: &str) -> Statement
    {
        let (operation, suffix) = instruction
            .split_once('.')
            .expect("guarded floating instruction must have suffix");
        let value_type = if suffix == "f32"
        {
            Type::F32
        }
        else
        {
            Type::F64
        };
        let result = self.binary_local(instruction, "result");
        let left = self.binary_local(instruction, "left");
        let right = self.binary_local(instruction, "right");
        if let Some(operation) = FloatComparisonOperation::parse_text_stem(operation)
        {
            Statement::CompareFloat {
                operation,
                value_type,
                result,
                left,
                right,
                span: span(),
            }
        }
        else
        {
            Statement::BinaryFloat {
                operation: FloatBinaryOperation::parse_text_stem(operation)
                    .expect("guarded floating operation must parse"),
                value_type,
                result,
                left,
                right,
                span: span(),
            }
        }
    }

    pub(super) fn const_f32_statement(&mut self) -> Statement
    {
        let local = self.const_target();
        Statement::ConstF32 {
            local,
            bits: self.float_bits(8) as u32,
            span: span(),
        }
    }

    pub(super) fn const_f64_statement(&mut self) -> Statement
    {
        let local = self.const_target();
        Statement::ConstF64 {
            local,
            bits: self.float_bits(16),
            span: span(),
        }
    }

    fn float_bits(&mut self, digits: usize) -> u64
    {
        let Some(line) = self.current()
        else
        {
            self.report("missing floating-point bit pattern".to_string());
            return 0;
        };
        self.index += 1;
        let Some(value) = line.strip_prefix("bits 0x")
        else
        {
            self.report("expected hexadecimal floating-point bit pattern".to_string());
            return 0;
        };
        if value.len() != digits
        {
            self.report("floating-point bit pattern has the wrong width".to_string());
            return 0;
        }
        u64::from_str_radix(value, 16).unwrap_or_else(|_| {
            self.report("floating-point bit pattern is invalid".to_string());
            0
        })
    }
}
