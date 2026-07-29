/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::*;

impl Parser<'_>
{
  pub(super) fn for_each_statement(&mut self) -> Statement
  {
    let name = self.current()
                   .and_then(|line| line.strip_prefix("for_each ").map(ToString::to_string))
                   .unwrap_or_default();
    self.index += 1;
    let ty = self.local_type();
    let iterable_type_text = self.current()
                                 .and_then(|line| line.strip_prefix("iterable_type ").map(ToString::to_string));
    let iterable_type = iterable_type_text.as_deref()
                                          .and_then(|value| self.parse_type(value))
                                          .unwrap_or(Type::Named(String::new()));
    self.index += 1;
    self.consume_expression_field("iterable");
    let iterable = self.expression()
                       .unwrap_or(Expression::Literal { literal: Literal::None,
                                                        span: span() });
    let body = self.named_block("body");
    Statement::ForEach { binding: Local { name,
                                          ty,
                                          mutable: false,
                                          span: span() },
                         iterable,
                         iterable_type,
                         body,
                         span: span() }
  }
}
