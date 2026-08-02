/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::{function_to_xhir, parse_xhir_function};
use crate::hir::async_check::Span;
use crate::hir::type_check::{
    Expression, Function, Literal, Local, PrimitiveType, Statement, TupleFieldType, TupleFieldValue, Type,
};

const fn span() -> Span
{
    Span::new(0, 0, 0)
}

fn integer(value: &str) -> Expression
{
    Expression::Literal {
        literal: Literal::Integer(value.to_string()),
        span: span(),
    }
}

#[test]
fn roundtrips_named_tuple_and_element_records()
{
    let tuple_type = Type::Tuple {
        fields: vec![
            TupleFieldType {
                name: Some("left".to_string()),
                ty: Type::Primitive(PrimitiveType::Long),
            },
            TupleFieldType {
                name: Some("right".to_string()),
                ty: Type::Primitive(PrimitiveType::Long),
            },
        ],
    };
    let function = Function {
        name: "tuple".to_string(),
        return_type: Some(Type::Primitive(PrimitiveType::Long)),
        locals: vec![Local {
            name: "pair".to_string(),
            ty: tuple_type.clone(),
            mutable: false,
            span: span(),
        }],
        body: vec![
            Statement::Let {
                local: Local {
                    name: "pair".to_string(),
                    ty: tuple_type.clone(),
                    mutable: false,
                    span: span(),
                },
                initializer: Some(Expression::Tuple {
                    fields: vec![
                        TupleFieldValue {
                            name: Some("left".to_string()),
                            value: integer("2"),
                            span: span(),
                        },
                        TupleFieldValue {
                            name: Some("right".to_string()),
                            value: integer("7"),
                            span: span(),
                        },
                    ],
                    tuple_type: Box::new(tuple_type.clone()),
                    span: span(),
                }),
            },
            Statement::AssignTupleElement {
                target: "pair".to_string(),
                index: 1,
                value: integer("9"),
                tuple_type: tuple_type.clone(),
                element_type: Type::Primitive(PrimitiveType::Long),
                span: span(),
            },
            Statement::Return {
                value: Some(Expression::TupleElement {
                    tuple: Box::new(Expression::Local {
                        name: "pair".to_string(),
                        span: span(),
                    }),
                    index: 1,
                    element_type: Box::new(Type::Primitive(PrimitiveType::Long)),
                    span: span(),
                }),
                span: span(),
            },
        ],
    };

    let text = function_to_xhir(&function);
    assert!(text.contains("tuple (left: Long, right: Long)"));
    assert!(text.contains("tuple_element 1 Long"));
    assert!(text.contains("assign_tuple_element pair 1"));
    assert_eq!(parse_xhir_function(&text).expect("tuple XHIR should parse"), function);
}
