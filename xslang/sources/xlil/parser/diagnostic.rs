/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

/// Category of a rejected XLIL text record.
#[derive(Clone, Debug, Eq, PartialEq)]
pub enum DiagnosticCode
{
    /// Input contained no records.
    EmptyInput,
    /// The module record was absent or malformed.
    InvalidModuleHeader,
    /// The version record was absent, malformed, or unsupported.
    InvalidVersionHeader,
    /// A function declaration/definition record was malformed.
    InvalidFunctionRecord,
    /// A function signature was malformed.
    InvalidSignature,
    /// A type spelling or registry reference was invalid.
    InvalidType,
    /// A basic block record was malformed.
    InvalidBlockRecord,
    /// An instruction record was unsupported or malformed.
    InvalidInstruction,
    /// A terminator record was unsupported or malformed.
    InvalidTerminator,
    /// A value register spelling was malformed.
    InvalidValueId,
    /// An integer token was malformed or out of range.
    InvalidInteger,
    /// A required following record was absent.
    UnexpectedEndOfInput,
}

/// Source-line diagnostic produced by [`super::parse_module`].
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct Diagnostic
{
    /// Stable diagnostic category.
    pub code: DiagnosticCode,
    /// One-based input line number.
    pub line: usize,
    /// Human-readable explanation.
    pub message: String,
}
