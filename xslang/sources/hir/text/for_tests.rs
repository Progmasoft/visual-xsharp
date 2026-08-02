/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::{function_to_xhir, parse_xhir_function};
use crate::hir::type_check::Local;
use crate::hir::{
    Block, Expression, Function, Literal, PrimitiveType, Span, Statement, Type, UpdateOperator, UpdatePosition,
};

fn span() -> Span
{
    Span::new(1, 0, 1)
}

#[test]
fn roundtrips_prefix_and_postfix_update_values()
{
    let local = Local {
        name: "value".to_string(),
        ty: Type::Primitive(PrimitiveType::Long),
        mutable: true,
        span: span(),
    };
    let function = Function {
        name: "updates".to_string(),
        return_type: Some(Type::Primitive(PrimitiveType::Long)),
        locals: vec![local.clone()],
        body: vec![
            Statement::Let {
                local,
                initializer: Some(integer("1")),
            },
            Statement::Expr(Expression::Update {
                target: "value".to_string(),
                operator: UpdateOperator::Increment,
                position: UpdatePosition::Postfix,
                span: span(),
            }),
            Statement::Return {
                value: Some(Expression::Update {
                    target: "value".to_string(),
                    operator: UpdateOperator::Decrement,
                    position: UpdatePosition::Prefix,
                    span: span(),
                }),
                span: span(),
            },
        ],
    };

    let text = function_to_xhir(&function);
    let parsed = parse_xhir_function(&text).expect("update XHIR should parse");

    assert!(text.contains("update postfix increment value"));
    assert!(text.contains("update prefix decrement value"));
    assert!(matches!(
        &parsed.body[1],
        Statement::Expr(Expression::Update {
            operator: UpdateOperator::Increment,
            position: UpdatePosition::Postfix,
            ..
        })
    ));
    assert!(matches!(&parsed.body[2], Statement::Return {
        value: Some(Expression::Update {
            operator: UpdateOperator::Decrement,
            position: UpdatePosition::Prefix,
            ..
        }),
        ..
    }));
}

fn integer(value: &str) -> Expression
{
    Expression::Literal {
        literal: Literal::Integer(value.to_string()),
        span: span(),
    }
}

#[test]
fn roundtrips_classic_for_records_with_optional_header_fields()
{
    let function = Function {
        name: "loop_records".to_string(),
        return_type: None,
        locals: Vec::new(),
        body: vec![Statement::For {
            initializer: Some(Box::new(Statement::Let {
                local: Local {
                    name: "i".to_string(),
                    ty: Type::Primitive(PrimitiveType::Long),
                    mutable: true,
                    span: span(),
                },
                initializer: Some(integer("0")),
            })),
            condition: Some(Expression::Literal {
                literal: Literal::Bool(true),
                span: span(),
            }),
            update: Some(Expression::Assign {
                target: "i".to_string(),
                value: Box::new(integer("1")),
                span: span(),
            }),
            body: Block {
                statements: vec![Statement::Break {
                    span: span(),
                }],
                tail: None,
                span: span(),
            },
            span: span(),
        }],
    };

    let text = function_to_xhir(&function);
    let parsed = parse_xhir_function(&text).expect("for XHIR should parse");

    assert!(text.contains("for\n"));
    assert!(text.contains("initializer\n"));
    assert!(text.contains("update\n"));
    assert!(matches!(&parsed.body[0], Statement::For {
    initializer: Some(_), condition: Some(_), update: Some(_), body, ..
  } if matches!(body.statements.as_slice(), [Statement::Break { .. }])));
}

#[test]
fn roundtrips_fixed_array_for_each_records()
{
    let array_type = Type::Array {
        element: Box::new(Type::Primitive(PrimitiveType::Long)),
        length: Some(3),
    };
    let function = Function {
        name: "visit".to_string(),
        return_type: None,
        locals: Vec::new(),
        body: vec![Statement::ForEach {
            binding: Local {
                name: "value".to_string(),
                ty: Type::Primitive(PrimitiveType::Long),
                mutable: false,
                span: span(),
            },
            iterable: Expression::Local {
                name: "values".to_string(),
                span: span(),
            },
            iterable_type: array_type.clone(),
            body: Block {
                statements: vec![Statement::Continue {
                    span: span(),
                }],
                tail: None,
                span: span(),
            },
            span: span(),
        }],
    };

    let text = function_to_xhir(&function);
    let parsed = parse_xhir_function(&text).expect("for-each XHIR should parse");

    assert!(text.contains("for_each value"));
    assert!(text.contains("iterable_type [Long; 3]"));
    assert!(
        matches!(&parsed.body[0], Statement::ForEach { binding, iterable_type, body, .. }
    if binding.name == "value" && iterable_type == &array_type &&
       matches!(body.statements.as_slice(), [Statement::Continue { .. }]))
    );
}
