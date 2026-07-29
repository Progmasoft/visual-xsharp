/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::*;

impl TypeChecker
{
  pub(super) fn check_match_statement(&mut self,
                                      selector: &Expression,
                                      selector_type: &Type,
                                      arms: &[MatchArm],
                                      span: Span)
  {
    self.check_match(selector, selector_type, arms, None, span);
  }

  pub(super) fn check_match_expression(&mut self,
                                       selector: &Expression,
                                       selector_type: &Type,
                                       arms: &[MatchArm],
                                       result_type: &Type,
                                       span: Span)
  {
    self.check_match(selector, selector_type, arms, Some(result_type), span);
  }

  fn check_match(&mut self,
                 selector: &Expression,
                 selector_type: &Type,
                 arms: &[MatchArm],
                 result_type: Option<&Type>,
                 span: Span)
  {
    self.check_expression_against_type(selector, selector_type);
    if !matches!(arms.last().map(|arm| &arm.pattern), Some(MatchPattern::Else))
    {
      self.diagnostics
          .push(Diagnostic { code: DiagnosticCode::MatchRequiresFinalElse,
                             message: "match statement requires a final else arm".to_string(),
                             span });
    }
    let mut patterns = Vec::new();
    for (index, arm) in arms.iter().enumerate()
    {
      match &arm.pattern
      {
        MatchPattern::Literal(literal) =>
        {
          self.check_expression_against_type(&Expression::Literal { literal: literal.clone(),
                                                                    span: arm.span },
                                             selector_type);
          if patterns.contains(literal)
          {
            self.diagnostics
                .push(Diagnostic { code: DiagnosticCode::DuplicateMatchPattern,
                                   message: "match statement contains a duplicate literal pattern".to_string(),
                                   span: arm.span });
          }
          else
          {
            patterns.push(literal.clone());
          }
        }
        MatchPattern::Else if index + 1 != arms.len() =>
        {
          self.diagnostics
              .push(Diagnostic { code: DiagnosticCode::MatchRequiresFinalElse,
                                 message: "else must be the final match arm".to_string(),
                                 span: arm.span });
        }
        MatchPattern::Else =>
        {}
      }
      self.check_block(&arm.body, result_type);
    }
  }
}

#[cfg(test)]
mod tests
{
  use super::*;

  fn span() -> Span
  {
    Span::new(1, 0, 1)
  }

  fn arm(pattern: MatchPattern) -> MatchArm
  {
    MatchArm { pattern,
               body: Block { statements: Vec::new(),
                             tail: None,
                             span: span() },
               span: span() }
  }

  #[test]
  fn rejects_missing_else_and_duplicate_literal_patterns()
  {
    let function = Function { name: "invalid".to_string(),
                              return_type: None,
                              locals: Vec::new(),
                              body: vec![Statement::Match {
        selector: Expression::Literal { literal: Literal::Integer("1".to_string()),
                                        span: span() },
        selector_type: Type::Primitive(PrimitiveType::Long),
        arms: vec![arm(MatchPattern::Literal(Literal::Integer("1".to_string()))),
                   arm(MatchPattern::Literal(Literal::Integer("1".to_string())))],
        span: span(),
      }] };

    let diagnostics = TypeChecker::new().check_function(&function);
    assert!(diagnostics.iter()
                       .any(|value| value.code == DiagnosticCode::MatchRequiresFinalElse));
    assert!(diagnostics.iter()
                       .any(|value| value.code == DiagnosticCode::DuplicateMatchPattern));
  }
}
