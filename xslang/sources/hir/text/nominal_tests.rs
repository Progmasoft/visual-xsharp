/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

use super::*;
use crate::hir::async_check::Span;
use crate::hir::type_check::{FieldPath, Local, ObjectField};

fn span() -> Span
{
    Span::new(0, 0, 0)
}

#[test]
fn roundtrips_nominal_object_and_field_records()
{
    let path = FieldPath {
        root: "point".to_string(),
        fields: vec!["x".to_string()],
        ty: Type::Primitive(PrimitiveType::Long),
        mutable: true,
        span: span(),
    };
    let function = Function {
        name: "main".to_string(),
        return_type: Some(Type::Primitive(PrimitiveType::Long)),
        locals: vec![Local {
            name: "point".to_string(),
            ty: Type::Named("Point".to_string()),
            mutable: true,
            span: span(),
        }],
        body: vec![
            Statement::Let {
                local: Local {
                    name: "point".to_string(),
                    ty: Type::Named("Point".to_string()),
                    mutable: true,
                    span: span(),
                },
                initializer: Some(Expression::Object {
                    nominal_type: "Point".to_string(),
                    fields: vec![ObjectField {
                        name: "x".to_string(),
                        value: Expression::Literal {
                            literal: Literal::Integer("7".to_string()),
                            span: span(),
                        },
                        span: span(),
                    }],
                    span: span(),
                }),
            },
            Statement::Expr(Expression::AssignField {
                target: path.clone(),
                value: Box::new(Expression::Literal {
                    literal: Literal::Integer("9".to_string()),
                    span: span(),
                }),
                span: span(),
            }),
            Statement::Return {
                value: Some(Expression::Field {
                    path,
                }),
                span: span(),
            },
        ],
    };
    let text = function_to_xhir(&function);
    let parsed = parse_xhir_function(&text).expect("nominal XHIR");
    assert_eq!(parsed, function);
}

#[test]
fn roundtrips_member_projection_from_a_value()
{
    let function = Function {
        name: "project".to_string(),
        return_type: Some(Type::Primitive(PrimitiveType::Long)),
        locals: Vec::new(),
        body: vec![Statement::Return {
            value: Some(Expression::Member {
                receiver: Box::new(Expression::Call {
                    function: "make_point".to_string(),
                    arguments: Vec::new(),
                    parameter_types: Vec::new(),
                    return_type: Box::new(Type::Named("Point".to_string())),
                    span: span(),
                }),
                owner: "Point".to_string(),
                name: "x".to_string(),
                field_type: Box::new(Type::Primitive(PrimitiveType::Long)),
                span: span(),
            }),
            span: span(),
        }],
    };
    let text = function_to_xhir(&function);
    assert!(text.contains("member Point.x : Long"));
    assert_eq!(parse_xhir_function(&text).expect("member projection XHIR"), function);
}
