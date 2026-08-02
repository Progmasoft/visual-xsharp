/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use crate::mir::Statement;
use crate::xlil::I32BinaryOperation;

use super::{Parser, span};

impl Parser<'_>
{
    pub(super) fn i32_statement(&mut self, instruction: &str) -> Statement
    {
        let result = self.binary_local(instruction, "result");
        let left = self.binary_local(instruction, "left");
        let right = self.binary_local(instruction, "right");
        match instruction
        {
            "add.i32" => Statement::AddI32 {
                result,
                left,
                right,
                span: span(),
            },
            "sub.i32" => Statement::SubI32 {
                result,
                left,
                right,
                span: span(),
            },
            "mul.i32" => Statement::MulI32 {
                result,
                left,
                right,
                span: span(),
            },
            name if I32BinaryOperation::parse_text(name).is_some() => Statement::BinaryI32 {
                operation: I32BinaryOperation::parse_text(name).expect("guarded operation must parse"),
                result,
                left,
                right,
                span: span(),
            },
            "eq.i32" => Statement::EqI32 {
                result,
                left,
                right,
                span: span(),
            },
            "lt.i32" => Statement::LtI32 {
                result,
                left,
                right,
                span: span(),
            },
            "le.i32" => Statement::LeI32 {
                result,
                left,
                right,
                span: span(),
            },
            "gt.i32" => Statement::GtI32 {
                result,
                left,
                right,
                span: span(),
            },
            "ge.i32" => Statement::GeI32 {
                result,
                left,
                right,
                span: span(),
            },
            _ => Statement::Use {
                local: result,
                span: span(),
            },
        }
    }
}
