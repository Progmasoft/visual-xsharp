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
    let exhaustive = self.match_is_exhaustive(selector_type, arms);
    if !exhaustive && !matches!(arms.last().map(|arm| &arm.pattern), Some(MatchPattern::Else))
    {
      self.diagnostics
          .push(Diagnostic { code: DiagnosticCode::MatchRequiresFinalElse,
                             message: "match statement requires a final else arm".to_string(),
                             span });
    }
    let mut patterns = Vec::new();
    let mut result_variants = Vec::new();
    let mut enum_data_variants = Vec::new();
    for (index, arm) in arms.iter().enumerate()
    {
      let local_count = self.locals.len();
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
        MatchPattern::ResultVariant { success,
                                      binding,
                                      payload_type, } =>
        {
          let expected_payload = match selector_type
          {
            Type::Result { success: expected_success,
                           error: expected_error, } =>
            {
              if *success { expected_success.as_ref() } else { expected_error.as_ref() }
            }
            _ =>
            {
              self.diagnostics
                  .push(Diagnostic { code: DiagnosticCode::LiteralTypeMismatch,
                                     message: "Ok/Error patterns require a Result<T, E> selector".to_string(),
                                     span: arm.span });
              payload_type
            }
          };
          if payload_type != expected_payload
          {
            self.diagnostics
                .push(Diagnostic { code: DiagnosticCode::LiteralTypeMismatch,
                                   message: "Result pattern payload type does not match its selector variant"
                                     .to_string(),
                                   span: arm.span });
          }
          if result_variants.contains(success)
          {
            self.diagnostics
                .push(Diagnostic { code: DiagnosticCode::DuplicateMatchPattern,
                                   message: "match statement contains a duplicate Result variant".to_string(),
                                   span: arm.span });
          }
          else
          {
            result_variants.push(*success);
          }
          if let Some(binding) = binding
          {
            self.locals.push(Local { name: binding.clone(),
                                     ty: payload_type.clone(),
                                     mutable: false,
                                     span: arm.span });
          }
        }
        MatchPattern::EnumDataVariant { tag,
                                        binding,
                                        payload_type,
                                        .. } =>
        {
          self.check_enum_data_pattern(selector_type, &arm.pattern, arm.span);
          if enum_data_variants.contains(tag)
          {
            self.diagnostics
                .push(Diagnostic { code: DiagnosticCode::DuplicateMatchPattern,
                                   message: format!("match statement contains duplicate enum data tag {tag}"),
                                   span: arm.span });
          }
          else
          {
            enum_data_variants.push(*tag);
          }
          if let (Some(binding), Some(payload_type)) = (binding, payload_type)
          {
            self.locals.push(Local { name: binding.clone(),
                                     ty: payload_type.clone(),
                                     mutable: false,
                                     span: arm.span });
          }
          else if binding.is_some()
          {
            self.diagnostics
                .push(Diagnostic { code: DiagnosticCode::LiteralTypeMismatch,
                                   message: "payload-free enum data pattern cannot bind a value".to_string(),
                                   span: arm.span });
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
      self.locals.truncate(local_count);
    }
  }

  fn check_enum_data_pattern(&mut self, selector_type: &Type, pattern: &MatchPattern, span: Span)
  {
    let MatchPattern::EnumDataVariant { enum_type,
                                        owner,
                                        variant,
                                        tag,
                                        payload_type,
                                        .. } = pattern
    else
    {
      return;
    };
    if selector_type != &Type::Named(enum_type.to_string())
    {
      self.diagnostics
          .push(Diagnostic { code: DiagnosticCode::LiteralTypeMismatch,
                             message: format!("enum data pattern for '{enum_type}' does not match selector type"),
                             span });
      return;
    }
    let selected = self.enum_data.select(enum_type, variant, payload_type.as_ref());
    match selected
    {
      Ok(selected) if selected.owner == *owner && selected.tag == *tag => {}
      Ok(_) => self.diagnostics.push(Diagnostic {
        code: DiagnosticCode::UnknownEnumVariant,
        message: format!("enum data pattern '{owner}::{variant}' has inconsistent overload metadata"),
        span,
      }),
      Err(error) => self.diagnostics.push(Diagnostic {
        code: DiagnosticCode::UnknownEnumVariant,
        message: format!("invalid enum data pattern '{enum_type}::{variant}': {error}"),
        span,
      }),
    }
  }

  fn match_is_exhaustive(&self, selector_type: &Type, arms: &[MatchArm]) -> bool
  {
    if result_arms_are_exhaustive(selector_type, arms)
    {
      return true;
    }
    let Type::Named(enum_type) = selector_type
    else
    {
      return false;
    };
    let Ok(variants) = self.enum_data.variants(enum_type)
    else
    {
      return false;
    };
    let matched = arms.iter()
                      .filter_map(|arm| match &arm.pattern
                      {
                        MatchPattern::EnumDataVariant { enum_type: pattern_type,
                                                        tag,
                                                        .. } if pattern_type == enum_type => Some(*tag),
                        _ => None,
                      })
                      .collect::<std::collections::HashSet<_>>();
    variants.iter().all(|variant| matched.contains(&variant.tag))
  }
}

fn result_arms_are_exhaustive(selector_type: &Type, arms: &[MatchArm]) -> bool
{
  matches!(selector_type, Type::Result { .. }) &&
  arms.iter()
      .filter_map(|arm| match arm.pattern
      {
        MatchPattern::ResultVariant { success, .. } => Some(success),
        _ => None,
      })
      .collect::<std::collections::HashSet<_>>()
      .len() ==
  2
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

  fn result_type(success: Type, error: Type) -> Type
  {
    Type::Result { success: Box::new(success),
                   error: Box::new(error) }
  }

  fn primitive(kind: PrimitiveType) -> Type
  {
    Type::Primitive(kind)
  }

  fn result_arm(success: bool, binding: Option<&str>, payload_type: Type, tail: Option<Expression>) -> MatchArm
  {
    MatchArm { pattern: MatchPattern::ResultVariant { success,
                                                      binding: binding.map(str::to_string),
                                                      payload_type },
               body: Block { statements: Vec::new(),
                             tail: tail.map(Box::new),
                             span: span() },
               span: span() }
  }

  fn local(name: &str) -> Expression
  {
    Expression::Local { name: name.to_string(),
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

  #[test]
  fn accepts_exhaustive_result_statement_without_else()
  {
    let long = primitive(PrimitiveType::Long);
    let result = result_type(long.clone(), long.clone());
    let function = Function {
      name: "consume".to_string(),
      return_type: None,
      locals: vec![Local { name: "input".to_string(),
                           ty: result.clone(),
                           mutable: false,
                           span: span() }],
      body: vec![Statement::Match {
        selector: local("input"),
        selector_type: result,
        arms: vec![result_arm(true, Some("value"), long.clone(), None),
                   result_arm(false, Some("error"), long, None)],
        span: span(),
      }],
    };

    let diagnostics = TypeChecker::new().check_function(&function);
    assert!(diagnostics.is_empty(), "{diagnostics:#?}");
  }

  #[test]
  fn accepts_result_payload_binding_as_expression_arm_tail()
  {
    let long = primitive(PrimitiveType::Long);
    let result = result_type(long.clone(), long.clone());
    let function = Function {
      name: "read".to_string(),
      return_type: Some(long.clone()),
      locals: vec![Local { name: "input".to_string(),
                           ty: result.clone(),
                           mutable: false,
                           span: span() }],
      body: vec![Statement::Return {
        value: Some(Expression::Match {
          selector: Box::new(local("input")),
          selector_type: Box::new(result),
          arms: vec![result_arm(true, Some("value"), long.clone(), Some(local("value"))),
                     result_arm(false, Some("error"), long.clone(), Some(local("error")))],
          result_type: Box::new(long),
          span: span(),
        }),
        span: span(),
      }],
    };

    let diagnostics = TypeChecker::new().check_function(&function);
    assert!(diagnostics.is_empty(), "{diagnostics:#?}");
  }

  #[test]
  fn ignored_result_payload_does_not_create_an_else_local()
  {
    let long = primitive(PrimitiveType::Long);
    let result = result_type(long.clone(), long.clone());
    let function = Function {
      name: "ignore".to_string(),
      return_type: Some(long.clone()),
      locals: vec![Local { name: "input".to_string(),
                           ty: result.clone(),
                           mutable: false,
                           span: span() }],
      body: vec![Statement::Return {
        value: Some(Expression::Match {
          selector: Box::new(local("input")),
          selector_type: Box::new(result),
          arms: vec![
            result_arm(true,
                       None,
                       long.clone(),
                       Some(Expression::Literal { literal: Literal::Integer("1".to_string()),
                                                  span: span() })),
            result_arm(false,
                       None,
                       long.clone(),
                       Some(Expression::Literal { literal: Literal::Integer("2".to_string()),
                                                  span: span() })),
          ],
          result_type: Box::new(long),
          span: span(),
        }),
        span: span(),
      }],
    };

    let diagnostics = TypeChecker::new().check_function(&function);
    assert!(diagnostics.is_empty(), "{diagnostics:#?}");
  }

  #[test]
  fn rejects_result_arm_tail_with_the_wrong_result_type()
  {
    let long = primitive(PrimitiveType::Long);
    let boolean = primitive(PrimitiveType::Bool);
    let result = result_type(long.clone(), boolean.clone());
    let function = Function {
      name: "invalid_tail".to_string(),
      return_type: Some(long.clone()),
      locals: vec![Local { name: "input".to_string(),
                           ty: result.clone(),
                           mutable: false,
                           span: span() }],
      body: vec![Statement::Return {
        value: Some(Expression::Match {
          selector: Box::new(local("input")),
          selector_type: Box::new(result),
          arms: vec![result_arm(true, Some("value"), long.clone(), Some(local("value"))),
                     result_arm(false, Some("failure"), boolean, Some(local("failure")))],
          result_type: Box::new(long),
          span: span(),
        }),
        span: span(),
      }],
    };

    let diagnostics = TypeChecker::new().check_function(&function);
    assert!(diagnostics.iter().any(|diagnostic| diagnostic.code == DiagnosticCode::LiteralTypeMismatch));
  }

  #[test]
  fn rejects_else_before_a_result_variant()
  {
    let long = primitive(PrimitiveType::Long);
    let result = result_type(long.clone(), long.clone());
    let function = Function {
      name: "invalid_order".to_string(),
      return_type: None,
      locals: vec![Local { name: "input".to_string(),
                           ty: result.clone(),
                           mutable: false,
                           span: span() }],
      body: vec![Statement::Match {
        selector: local("input"),
        selector_type: result,
        arms: vec![arm(MatchPattern::Else), result_arm(true, Some("value"), long, None)],
        span: span(),
      }],
    };

    let diagnostics = TypeChecker::new().check_function(&function);
    assert!(diagnostics.iter().any(|diagnostic| diagnostic.code == DiagnosticCode::MatchRequiresFinalElse &&
                                                diagnostic.message.contains("final")));
  }
}
