/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

use super::*;
use crate::hir::async_check::Span;
use crate::hir::type_check::{Literal, PrimitiveType};

fn span() -> Span
{
    Span::new(0, 0, 0)
}

#[test]
fn enum_data_payload_round_trips_through_xhir()
{
    let function = Function {
        name: "wrap".to_string(),
        return_type: Some(Type::Named("Value".to_string())),
        locals: Vec::new(),
        body: vec![Statement::Return {
            value: Some(Expression::EnumData {
                enum_type: "Value".to_string(),
                owner: "BaseValue".to_string(),
                variant: "Number".to_string(),
                tag: 3,
                payload: Some(Box::new(Expression::Literal {
                    literal: Literal::Integer("42".to_string()),
                    span: span(),
                })),
                payload_type: Some(Box::new(Type::Primitive(PrimitiveType::Int))),
                span: span(),
            }),
            span: span(),
        }],
    };
    let text = function_to_xhir(&function);
    assert!(text.contains("enum_data Value::Number owner BaseValue tag 3 payload Int"));
    assert!(text.contains("value\n          literal integer 42"));
    assert_eq!(parse_xhir_function(&text).expect("enum-data XHIR"), function);
}

#[test]
fn payload_free_enum_data_round_trips_through_xhir()
{
    let function = Function {
        name: "finish".to_string(),
        return_type: Some(Type::Named("Token".to_string())),
        locals: Vec::new(),
        body: vec![Statement::Return {
            value: Some(Expression::EnumData {
                enum_type: "Token".to_string(),
                owner: "Token".to_string(),
                variant: "End".to_string(),
                tag: 1,
                payload: None,
                payload_type: None,
                span: span(),
            }),
            span: span(),
        }],
    };
    let text = function_to_xhir(&function);
    assert!(text.contains("enum_data Token::End owner Token tag 1 payload ()"));
    assert_eq!(
        parse_xhir_function(&text).expect("payload-free enum-data XHIR"),
        function
    );
}

#[test]
fn malformed_enum_data_tag_is_reported()
{
    let text = ".xhir version 1\nfunction broken\n  signature\n    returns Value\n  .end\n  body\n    return\n      \
                enum_data Value::Number owner Value tag nope payload Int\n        value\n          literal integer \
                1\n      .end\n  .end\n.program end\n";
    let diagnostics = parse_xhir_function(text).expect_err("invalid tag must fail");
    assert!(
        diagnostics
            .iter()
            .any(|diagnostic| diagnostic.message.contains("invalid enum-data tag"))
    );
}
