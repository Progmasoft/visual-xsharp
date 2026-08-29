/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
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
    let empty = || Block {
        statements: Vec::new(),
        tail: None,
        span: span(),
    };
    let function = Function {
        name: "choose".to_string(),
        return_type: None,
        locals: Vec::new(),
        body: vec![Statement::Match {
            selector: Expression::Literal {
                literal: Literal::Integer("2".to_string()),
                span: span(),
            },
            selector_type: Type::Primitive(PrimitiveType::Long),
            arms: vec![
                MatchArm {
                    pattern: MatchPattern::Literal(Literal::Integer("2".to_string())),
                    body: empty(),
                    span: span(),
                },
                MatchArm {
                    pattern: MatchPattern::Else,
                    body: empty(),
                    span: span(),
                },
            ],
            span: span(),
        }],
    };

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
    let value_arm = |pattern, value: &str| MatchArm {
        pattern,
        body: Block {
            statements: Vec::new(),
            tail: Some(Box::new(Expression::Literal {
                literal: Literal::Integer(value.to_string()),
                span: span(),
            })),
            span: span(),
        },
        span: span(),
    };
    let match_expression = Expression::Match {
        selector: Box::new(Expression::Literal {
            literal: Literal::Integer("2".to_string()),
            span: span(),
        }),
        selector_type: Box::new(Type::Primitive(PrimitiveType::Long)),
        arms: vec![
            value_arm(MatchPattern::Literal(Literal::Integer("2".to_string())), "7"),
            value_arm(MatchPattern::Else, "9"),
        ],
        result_type: Box::new(Type::Primitive(PrimitiveType::Long)),
        span: span(),
    };
    let function = Function {
        name: "choose_value".to_string(),
        return_type: Some(Type::Primitive(PrimitiveType::Long)),
        locals: Vec::new(),
        body: vec![Statement::Return {
            value: Some(match_expression),
            span: span(),
        }],
    };

    let text = function_to_xhir(&function);
    let parsed = parse_xhir_function(&text).expect("match expression XHIR should parse");

    assert!(text.contains("match_expression Long selector Long"));
    assert!(
        matches!(&parsed.body[0], Statement::Return { value: Some(Expression::Match { arms, .. }), .. }
                   if arms.len() == 2)
    );
}

#[test]
fn roundtrips_resolved_enum_data_pattern_identity()
{
    let function = Function {
        name: "inspect".to_string(),
        return_type: None,
        locals: vec![crate::hir::Local {
            name: "input".to_string(),
            ty: Type::Named("Value".to_string()),
            mutable: false,
            span: span(),
        }],
        body: vec![Statement::Match {
            selector: Expression::Local {
                name: "input".to_string(),
                span: span(),
            },
            selector_type: Type::Named("Value".to_string()),
            arms: vec![
                MatchArm {
                    pattern: MatchPattern::EnumDataVariant {
                        enum_type: "Value".to_string(),
                        owner: "Root".to_string(),
                        variant: "Number".to_string(),
                        tag: 3,
                        binding: Some("number".to_string()),
                        payload_type: Some(Type::Primitive(PrimitiveType::Long)),
                    },
                    body: Block {
                        statements: Vec::new(),
                        tail: None,
                        span: span(),
                    },
                    span: span(),
                },
                MatchArm {
                    pattern: MatchPattern::Else,
                    body: Block {
                        statements: Vec::new(),
                        tail: None,
                        span: span(),
                    },
                    span: span(),
                },
            ],
            span: span(),
        }],
    };
    let text = function_to_xhir(&function);
    let parsed = parse_xhir_function(&text).expect("enum data match XHIR should parse");
    assert!(text.contains("arm enum_data Value::Number owner Root tag 3 binding number : Long"));
    assert!(matches!(&parsed.body[0],
                   Statement::Match { arms, .. }
                     if matches!(&arms[0].pattern,
                                 MatchPattern::EnumDataVariant { enum_type,
                                                                 owner,
                                                                 variant,
                                                                 tag: 3,
                                                                 binding: Some(binding),
                                                                 payload_type: Some(Type::Primitive(PrimitiveType::Long)) }
                                   if enum_type == "Value" && owner == "Root" && variant == "Number" &&
                                      binding == "number")));
}

#[test]
fn roundtrips_payload_free_enum_data_pattern()
{
    let function = Function {
        name: "consume".to_string(),
        return_type: None,
        locals: vec![crate::hir::Local {
            name: "input".to_string(),
            ty: Type::Named("Token".to_string()),
            mutable: false,
            span: span(),
        }],
        body: vec![Statement::Match {
            selector: Expression::Local {
                name: "input".to_string(),
                span: span(),
            },
            selector_type: Type::Named("Token".to_string()),
            arms: vec![
                MatchArm {
                    pattern: MatchPattern::EnumDataVariant {
                        enum_type: "Token".to_string(),
                        owner: "Token".to_string(),
                        variant: "End".to_string(),
                        tag: 2,
                        binding: None,
                        payload_type: None,
                    },
                    body: Block {
                        statements: Vec::new(),
                        tail: None,
                        span: span(),
                    },
                    span: span(),
                },
                MatchArm {
                    pattern: MatchPattern::Else,
                    body: Block {
                        statements: Vec::new(),
                        tail: None,
                        span: span(),
                    },
                    span: span(),
                },
            ],
            span: span(),
        }],
    };
    let source = function_to_xhir(&function);
    let parsed = parse_xhir_function(&source).expect("payload-free enum data match should parse");
    let text = function_to_xhir(&parsed);
    assert!(text.contains("arm enum_data Token::End owner Token tag 2 binding else : ()"));
    assert_eq!(parse_xhir_function(&text).unwrap(), parsed);
}
