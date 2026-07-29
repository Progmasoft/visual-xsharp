/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
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
  Else,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct MatchArm
{
  pub pattern: MatchPattern,
  pub body: Block,
  pub span: Span,
}
