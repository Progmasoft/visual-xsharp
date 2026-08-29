/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

use crate::hir::async_check::Span;

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum DiagnosticCode
{
    LiteralTypeMismatch,
    ImmutableAssignment,
    UnknownLocal,
    DuplicateLocal,
    BinaryTypeMismatch,
    UnaryTypeMismatch,
    ResultPropagationRequiresResult,
    ResultPropagationReturnMismatch,
    ConditionTypeMismatch,
    ForEachTypeMismatch,
    MissingBlockValue,
    BreakOutsideLoop,
    ContinueOutsideLoop,
    MatchRequiresFinalElse,
    DuplicateMatchPattern,
    UnknownNominalType,
    UnknownEnumVariant,
    UnknownField,
    MissingField,
    DuplicateField,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct Diagnostic
{
    pub code: DiagnosticCode,
    pub message: String,
    pub span: Span,
}
