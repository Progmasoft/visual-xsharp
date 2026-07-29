/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::*;

impl Parser<'_>
{
  pub(super) fn match_expression(&mut self, signature: &str) -> Option<Expression>
  {
    self.index += 1;
    let Some((result_type, selector_type)) = signature.split_once(" selector ")
    else
    {
      self.report("invalid match expression type record".to_string());
      return None;
    };
    let result_type = self.parse_type(result_type)
                          .unwrap_or(Type::Named(result_type.to_string()));
    let selector_type = self.parse_type(selector_type)
                            .unwrap_or(Type::Named(selector_type.to_string()));
    self.consume_expression_field("selector");
    let selector = self.expression()
                       .unwrap_or(Expression::Literal { literal: Literal::None,
                                                        span: span() });
    let mut arms = Vec::new();
    while let Some(line) = self.current()
    {
      if line == ".end"
      {
        self.index += 1;
        break;
      }
      let pattern = if line == "arm else"
      {
        MatchPattern::Else
      }
      else if let Some(literal) = line.strip_prefix("arm literal ")
      {
        MatchPattern::Literal(self.literal(literal))
      }
      else
      {
        self.report(format!("invalid match expression arm record '{line}'"));
        self.index += 1;
        continue;
      };
      self.index += 1;
      arms.push(MatchArm { pattern,
                           body: self.named_block("body"),
                           span: span() });
    }
    Some(Expression::Match { selector: Box::new(selector),
                             selector_type: Box::new(selector_type),
                             arms,
                             result_type: Box::new(result_type),
                             span: span() })
  }
}
