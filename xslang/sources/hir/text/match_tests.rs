/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::{function_to_xhir, parse_xhir_function};
use crate::hir::{Block, Expression, Function, Literal, MatchArm, MatchPattern, PrimitiveType, Span, Statement, Type};

fn span() -> Span
{
  Span::new(1, 0, 1)
}

#[test]
fn roundtrips_structured_match_arms_with_explicit_end_marker()
{
  let empty = || Block { statements: Vec::new(),
                         tail: None,
                         span: span() };
  let function = Function { name: "choose".to_string(),
                            return_type: None,
                            locals: Vec::new(),
                            body: vec![Statement::Match { selector:
                                                            Expression::Literal { literal:
                                                                                    Literal::Integer("2".to_string()),
                                                                                  span: span() },
                                                          selector_type: Type::Primitive(PrimitiveType::Long),
                                                          arms: vec![MatchArm { pattern:
                                                             MatchPattern::Literal(Literal::Integer("2".to_string())),
                                                           body: empty(),
                                                           span: span() },
                                                MatchArm { pattern: MatchPattern::Else,
                                                           body: empty(),
                                                           span: span() }],
                                                          span: span() }] };

  let text = function_to_xhir(&function);
  let parsed = parse_xhir_function(&text).expect("match XHIR should parse");

  assert!(text.contains("match Long\n"));
  assert!(text.contains("arm literal integer 2\n"));
  assert!(text.contains("arm else\n"));
  assert!(matches!(&parsed.body[0], Statement::Match { arms, .. } if arms.len() == 2));
}

#[test]
fn roundtrips_typed_match_expression_with_value_arms()
{
  let value_arm = |pattern, value: &str| {
    MatchArm { pattern,
               body: Block { statements: Vec::new(),
                             tail: Some(Box::new(Expression::Literal { literal:
                                                                         Literal::Integer(value.to_string()),
                                                                       span: span() })),
                             span: span() },
               span: span() }
  };
  let match_expression =
    Expression::Match { selector: Box::new(Expression::Literal { literal: Literal::Integer("2".to_string()),
                                                                 span: span() }),
                        selector_type: Box::new(Type::Primitive(PrimitiveType::Long)),
                        arms: vec![value_arm(MatchPattern::Literal(Literal::Integer("2".to_string())), "7"),
                                   value_arm(MatchPattern::Else, "9")],
                        result_type: Box::new(Type::Primitive(PrimitiveType::Long)),
                        span: span() };
  let function = Function { name: "choose_value".to_string(),
                            return_type: Some(Type::Primitive(PrimitiveType::Long)),
                            locals: Vec::new(),
                            body: vec![Statement::Return { value: Some(match_expression),
                                                           span: span() }] };

  let text = function_to_xhir(&function);
  let parsed = parse_xhir_function(&text).expect("match expression XHIR should parse");

  assert!(text.contains("match_expression Long selector Long"));
  assert!(matches!(&parsed.body[0], Statement::Return { value: Some(Expression::Match { arms, .. }), .. }
                   if arms.len() == 2));
}
