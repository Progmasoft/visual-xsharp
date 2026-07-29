/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::*;

impl Parser<'_>
{
  pub(super) fn unary_operator(&mut self, name: &str) -> Option<UnaryOperator>
  {
    match name
    {
      "logical_not" => Some(UnaryOperator::LogicalNot),
      "positive" => Some(UnaryOperator::Positive),
      "negative" => Some(UnaryOperator::Negative),
      _ =>
      {
        self.report(format!("unknown unary operator '{name}'"));
        None
      }
    }
  }
}
