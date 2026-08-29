/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

use crate::hir::async_check::Span;

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum DiagnosticCode
{
    MissingTerminator,
    UnknownLocal,
    UseAfterMove,
    MoveWhileBorrowed,
    MutableBorrowConflict,
    ImmutableLocalMutableBorrow,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct Diagnostic
{
    pub code: DiagnosticCode,
    pub message: String,
    pub span: Span,
}
