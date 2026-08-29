/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

use super::async_check::Span;
use super::type_check::{Block, Literal, Type};

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum MatchPattern
{
    Literal(Literal),
    ResultVariant
    {
        success: bool,
        binding: Option<String>,
        payload_type: Type,
    },
    /// A resolved user-defined `enum data` variant pattern.
    ///
    /// The owner and flattened tag preserve the exact overload selected during
    /// compiler-core lowering. `enum_type` is the selector's nominal type; it
    /// may differ from `owner` when a variant is inherited.
    EnumDataVariant
    {
        enum_type: String,
        owner: String,
        variant: String,
        tag: u32,
        binding: Option<String>,
        payload_type: Option<Type>,
    },
    Else,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct MatchArm
{
    pub pattern: MatchPattern,
    pub body: Block,
    pub span: Span,
}
