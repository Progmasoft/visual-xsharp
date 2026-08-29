/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

use super::*;

impl Parser<'_>
{
    pub(super) fn extended_i64_statement(&mut self, instruction: &str) -> Statement
    {
        let result = self.binary_i64_local(instruction, "result");
        let left = self.binary_i64_local(instruction, "left");
        let right = self.binary_i64_local(instruction, "right");
        if let Some(operation) = I64BinaryOperation::parse_text(instruction)
        {
            Statement::BinaryI64 {
                operation,
                result,
                left,
                right,
                span: span(),
            }
        }
        else if let Some(operation) = I64ComparisonOperation::parse_text(instruction)
        {
            Statement::CompareI64 {
                operation,
                result,
                left,
                right,
                span: span(),
            }
        }
        else
        {
            self.report(format!("unknown i64 instruction '{instruction}'"));
            Statement::Use {
                local: result,
                span: span(),
            }
        }
    }
}
