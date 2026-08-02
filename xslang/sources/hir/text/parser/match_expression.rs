/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::*;
use crate::hir::MatchPattern;

impl Parser<'_>
{
  pub(super) fn match_pattern(&mut self, line: &str) -> Option<MatchPattern>
  {
    let record = line.strip_prefix("arm ")?;
    if record == "else"
    {
      return Some(MatchPattern::Else);
    }
    if let Some(literal) = record.strip_prefix("literal ")
    {
      return Some(MatchPattern::Literal(self.literal(literal)));
    }
    if let Some(enum_data) = record.strip_prefix("enum_data ")
    {
      let (identity, payload_type) = enum_data.split_once(" : ")?;
      let (identity, binding) = identity.split_once(" binding ")?;
      let (identity, tag) = identity.rsplit_once(" tag ")?;
      let (variant_path, owner) = identity.rsplit_once(" owner ")?;
      let (enum_type, variant) = variant_path.rsplit_once("::")?;
      let tag = tag.parse().ok()?;
      return Some(MatchPattern::EnumDataVariant { enum_type: enum_type.to_string(),
                                                  owner: owner.to_string(),
                                                  variant: variant.to_string(),
                                                  tag,
                                                  binding: (binding != "else").then(|| binding.to_string()),
                                                  payload_type: (payload_type != "()").then(|| {
                                                                                        self.parse_type(payload_type)
                                                                                      })
                                                                                      .flatten() });
    }
    let result = record.strip_prefix("result ")?;
    let (variant_binding, payload_type) = result.split_once(" : ")?;
    let (variant, binding) = variant_binding.split_once(" binding ")?;
    let success = match variant
    {
      "Ok" => true,
      "Error" => false,
      _ => return None,
    };
    Some(MatchPattern::ResultVariant { success,
                                       binding: (binding != "else").then(|| binding.to_string()),
                                       payload_type: self.parse_type(payload_type)? })
  }

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
      let Some(pattern) = self.match_pattern(&line)
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
