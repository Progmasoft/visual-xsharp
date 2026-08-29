/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

use super::{DiagnosticCode, Parser};
use crate::xlil::{BlockId, Terminator, ValueId};

impl Parser<'_>
{
    pub(super) fn call_operands(&mut self, call: &str, line: usize) -> Option<(String, Vec<ValueId>)>
    {
        let Some((function, arguments)) = call.split_once('(')
        else
        {
            self.report(
                DiagnosticCode::InvalidInstruction,
                line,
                "XLIL call operands are invalid",
            );
            return None;
        };
        let Some(arguments) = arguments.strip_suffix(')')
        else
        {
            self.report(
                DiagnosticCode::InvalidInstruction,
                line,
                "XLIL call arguments are invalid",
            );
            return None;
        };
        let mut parsed = Vec::new();
        if !arguments.is_empty()
        {
            for argument in arguments.split(", ")
            {
                parsed.push(self.value_operand(argument, line)?);
            }
        }
        Some((function.to_string(), parsed))
    }

    pub(super) fn value_operand(&mut self, text: &str, line: usize) -> Option<ValueId>
    {
        let Some(value) = text.strip_prefix('%')
        else
        {
            self.report(
                DiagnosticCode::InvalidInstruction,
                line,
                "XLIL value operand is invalid",
            );
            return None;
        };
        self.value_id(value, line)
    }

    pub(super) fn return_terminator(&mut self, text: &str, line: usize) -> Option<Terminator>
    {
        if text.is_empty()
        {
            return Some(Terminator::Return(None));
        }
        let Some(value) = text.strip_prefix(" %")
        else
        {
            self.report(
                DiagnosticCode::InvalidTerminator,
                line,
                "XLIL return terminator is invalid",
            );
            return None;
        };
        Some(Terminator::Return(Some(self.value_id(value, line)?)))
    }

    pub(super) fn branch_terminator(&mut self, text: &str, line: usize) -> Option<Terminator>
    {
        let Some(target) = text
            .strip_prefix("bb")
            .and_then(|target| target.parse::<u32>().ok())
            .map(BlockId)
        else
        {
            self.report(DiagnosticCode::InvalidTerminator, line, "XLIL branch target is invalid");
            return None;
        };
        Some(Terminator::Branch(target))
    }

    pub(super) fn branch_if_terminator(&mut self, text: &str, line: usize) -> Option<Terminator>
    {
        let Some((condition, targets)) = text.split_once(", ")
        else
        {
            self.report(
                DiagnosticCode::InvalidTerminator,
                line,
                "XLIL br_if terminator is invalid",
            );
            return None;
        };
        let condition = self.value_operand(condition, line)?;
        let Some((then_block, else_block)) = targets.split_once(", ")
        else
        {
            self.report(
                DiagnosticCode::InvalidTerminator,
                line,
                "XLIL br_if targets are invalid",
            );
            return None;
        };
        Some(Terminator::BranchIf {
            condition,
            then_block: self.branch_target(then_block, line)?,
            else_block: self.branch_target(else_block, line)?,
        })
    }

    fn branch_target(&mut self, text: &str, line: usize) -> Option<BlockId>
    {
        let parsed = text
            .strip_prefix("bb")
            .and_then(|target| target.parse::<u32>().ok())
            .map(BlockId);
        if parsed.is_none()
        {
            self.report(DiagnosticCode::InvalidTerminator, line, "XLIL branch target is invalid");
        }
        parsed
    }

    pub(super) fn value_id(&mut self, text: &str, line: usize) -> Option<ValueId>
    {
        let parsed = text
            .strip_prefix('r')
            .and_then(|text| text.parse::<u32>().ok())
            .map(ValueId);
        if parsed.is_none()
        {
            self.report(DiagnosticCode::InvalidValueId, line, "XLIL value id is invalid");
        }
        parsed
    }
}
