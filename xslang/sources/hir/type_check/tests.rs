/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::*;

fn span(start: u32, end: u32) -> Span
{
    Span::new(1, start, end)
}

fn primitive(primitive: PrimitiveType) -> Type
{
    Type::Primitive(primitive)
}

fn local(name: &str, ty: Type, mutable: bool) -> Local
{
    Local {
        name: name.to_string(),
        ty,
        mutable,
        span: span(0, 1),
    }
}

#[test]
fn validates_literal_initializer_types()
{
    let function = Function {
        name: "main".to_string(),
        return_type: None,
        locals: vec![],
        body: vec![
            Statement::Let {
                local: local("count", primitive(PrimitiveType::Int), true),
                initializer: Some(Expression::Literal {
                    literal: Literal::Integer("1".to_string()),
                    span: span(10, 11),
                }),
            },
            Statement::Let {
                local: local(
                    "name",
                    Type::Reference {
                        referent: Box::new(primitive(PrimitiveType::Str)),
                        mutable: false,
                    },
                    false,
                ),
                initializer: Some(Expression::Literal {
                    literal: Literal::String("xs".to_string()),
                    span: span(20, 24),
                }),
            },
        ],
    };

    assert!(TypeChecker::new().check_function(&function).is_empty());
}

#[test]
fn validates_builtin_collection_literals_against_collection_types()
{
    let function = Function {
        name: "collections".to_string(),
        return_type: None,
        locals: vec![],
        body: vec![
            Statement::Let {
                local: local(
                    "values",
                    Type::Array {
                        element: Box::new(primitive(PrimitiveType::Int)),
                        length: Some(2),
                    },
                    false,
                ),
                initializer: Some(Expression::Array {
                    elements: vec![
                        Expression::Literal {
                            literal: Literal::Integer("1".to_string()),
                            span: span(1, 2),
                        },
                        Expression::Literal {
                            literal: Literal::Integer("2".to_string()),
                            span: span(3, 4),
                        },
                    ],
                    span: span(1, 5),
                }),
            },
            Statement::Let {
                local: local(
                    "unique",
                    Type::Set {
                        element: Box::new(primitive(PrimitiveType::Int)),
                    },
                    false,
                ),
                initializer: Some(Expression::Set {
                    elements: vec![
                        Expression::Literal {
                            literal: Literal::Integer("1".to_string()),
                            span: span(6, 7),
                        },
                        Expression::Literal {
                            literal: Literal::Integer("2".to_string()),
                            span: span(8, 9),
                        },
                    ],
                    span: span(6, 10),
                }),
            },
        ],
    };

    assert!(TypeChecker::new().check_function(&function).is_empty());
}

#[test]
fn rejects_literal_initializer_type_mismatch()
{
    let function = Function {
        name: "main".to_string(),
        return_type: None,
        locals: vec![],
        body: vec![Statement::Let {
            local: local("count", primitive(PrimitiveType::Int), true),
            initializer: Some(Expression::Literal {
                literal: Literal::String("bad".to_string()),
                span: span(10, 15),
            }),
        }],
    };

    let diagnostics = TypeChecker::new().check_function(&function);

    assert_eq!(diagnostics.len(), 1);
    assert_eq!(diagnostics[0].code, DiagnosticCode::LiteralTypeMismatch);
}

#[test]
fn rejects_assignment_to_immutable_local()
{
    let function = Function {
        name: "main".to_string(),
        return_type: None,
        locals: vec![local(
            "version",
            Type::Reference {
                referent: Box::new(primitive(PrimitiveType::Str)),
                mutable: false,
            },
            false,
        )],
        body: vec![Statement::Expr(Expression::Assign {
            target: "version".to_string(),
            value: Box::new(Expression::Literal {
                literal: Literal::String("2.0".to_string()),
                span: span(20, 25),
            }),
            span: span(10, 25),
        })],
    };

    let diagnostics = TypeChecker::new().check_function(&function);

    assert_eq!(diagnostics.len(), 1);
    assert_eq!(diagnostics[0].code, DiagnosticCode::ImmutableAssignment);
}

#[test]
fn validates_integer_update_and_rejects_immutable_or_non_integer_targets()
{
    let update = |target: &str| Expression::Update {
        target: target.to_string(),
        operator: UpdateOperator::Increment,
        position: UpdatePosition::Postfix,
        span: span(10, 17),
    };
    let valid = Function {
        name: "valid".to_string(),
        return_type: Some(primitive(PrimitiveType::Long)),
        locals: vec![local("value", primitive(PrimitiveType::Long), true)],
        body: vec![Statement::Return {
            value: Some(update("value")),
            span: span(3, 17),
        }],
    };
    assert!(TypeChecker::new().check_function(&valid).is_empty());

    let invalid = Function {
        name: "invalid".to_string(),
        return_type: None,
        locals: vec![
            local("fixed", primitive(PrimitiveType::Long), false),
            local("flag", primitive(PrimitiveType::Bool), true),
        ],
        body: vec![Statement::Expr(update("fixed")), Statement::Expr(update("flag"))],
    };
    let diagnostics = TypeChecker::new().check_function(&invalid);
    assert_eq!(diagnostics.len(), 2);
    assert_eq!(diagnostics[0].code, DiagnosticCode::ImmutableAssignment);
    assert_eq!(diagnostics[1].code, DiagnosticCode::UnaryTypeMismatch);
}

#[test]
fn validates_return_literal_type()
{
    let function = Function {
        name: "answer".to_string(),
        return_type: Some(primitive(PrimitiveType::Int)),
        locals: vec![],
        body: vec![Statement::Return {
            value: Some(Expression::Literal {
                literal: Literal::Integer("42".to_string()),
                span: span(10, 12),
            }),
            span: span(3, 12),
        }],
    };

    assert!(TypeChecker::new().check_function(&function).is_empty());
}

#[test]
fn accepts_integer_literal_in_explicit_bool_return_context()
{
    let function = Function {
        name: "answer".to_string(),
        return_type: Some(primitive(PrimitiveType::Bool)),
        locals: vec![],
        body: vec![Statement::Return {
            value: Some(Expression::Literal {
                literal: Literal::Integer("42".to_string()),
                span: span(10, 12),
            }),
            span: span(3, 12),
        }],
    };

    assert!(TypeChecker::new().check_function(&function).is_empty());
}

#[test]
fn validates_long_binary_expression()
{
    let function = Function {
        name: "compare".to_string(),
        return_type: Some(primitive(PrimitiveType::Bool)),
        locals: vec![
            local("left", primitive(PrimitiveType::Long), false),
            local("right", primitive(PrimitiveType::Long), false),
        ],
        body: vec![Statement::Return {
            value: Some(Expression::Binary {
                operator: BinaryOperator::Less,
                left: Box::new(Expression::Local {
                    name: "left".to_string(),
                    span: span(10, 14),
                }),
                right: Box::new(Expression::Local {
                    name: "right".to_string(),
                    span: span(17, 22),
                }),
                span: span(10, 22),
            }),
            span: span(3, 22),
        }],
    };

    assert!(TypeChecker::new().check_function(&function).is_empty());
}

#[test]
fn rejects_unsupported_binary_expression()
{
    let function = Function {
        name: "bitwise_float".to_string(),
        return_type: Some(primitive(PrimitiveType::SFloat)),
        locals: vec![
            local("left", primitive(PrimitiveType::SFloat), false),
            local("right", primitive(PrimitiveType::SFloat), false),
        ],
        body: vec![Statement::Return {
            value: Some(Expression::Binary {
                operator: BinaryOperator::BitAnd,
                left: Box::new(Expression::Local {
                    name: "left".to_string(),
                    span: span(10, 14),
                }),
                right: Box::new(Expression::Local {
                    name: "right".to_string(),
                    span: span(17, 22),
                }),
                span: span(10, 22),
            }),
            span: span(3, 22),
        }],
    };

    let diagnostics = TypeChecker::new().check_function(&function);

    assert_eq!(diagnostics[0].code, DiagnosticCode::BinaryTypeMismatch);
}

#[test]
fn validates_result_propagation_success_type_and_error_return()
{
    let function = Function {
        name: "main".to_string(),
        return_type: Some(Type::Named("Result<()>".to_string())),
        locals: vec![local("work", Type::Named("Result<Int, Error>".to_string()), false)],
        body: vec![Statement::Let {
            local: local("value", primitive(PrimitiveType::Int), false),
            initializer: Some(Expression::ResultPropagation {
                value: Box::new(Expression::Local {
                    name: "work".to_string(),
                    span: span(4, 8),
                }),
                span: span(4, 9),
            }),
        }],
    };

    assert!(TypeChecker::new().check_function(&function).is_empty());
}

#[test]
fn validates_unit_result_as_default_error()
{
    let function = Function {
        name: "main".to_string(),
        return_type: Some(Type::Named("Result<()>".to_string())),
        locals: vec![local("work", Type::Named("Result<()>".to_string()), false)],
        body: vec![Statement::Expr(Expression::ResultPropagation {
            value: Box::new(Expression::Local {
                name: "work".to_string(),
                span: span(4, 8),
            }),
            span: span(4, 9),
        })],
    };

    assert!(TypeChecker::new().check_function(&function).is_empty());
}

#[test]
fn rejects_result_propagation_on_non_result_value()
{
    let function = Function {
        name: "main".to_string(),
        return_type: None,
        locals: vec![local("work", primitive(PrimitiveType::Int), false)],
        body: vec![Statement::Expr(Expression::ResultPropagation {
            value: Box::new(Expression::Local {
                name: "work".to_string(),
                span: span(4, 8),
            }),
            span: span(4, 9),
        })],
    };

    let diagnostics = TypeChecker::new().check_function(&function);

    assert_eq!(diagnostics[0].code, DiagnosticCode::ResultPropagationRequiresResult);
}

#[test]
fn rejects_result_propagation_error_mismatch()
{
    let function = Function {
        name: "main".to_string(),
        return_type: Some(Type::Named("Result<Bool, Other>".to_string())),
        locals: vec![local("work", Type::Named("Result<Int, Error>".to_string()), false)],
        body: vec![Statement::Expr(Expression::ResultPropagation {
            value: Box::new(Expression::Local {
                name: "work".to_string(),
                span: span(4, 8),
            }),
            span: span(4, 9),
        })],
    };

    let diagnostics = TypeChecker::new().check_function(&function);

    assert_eq!(diagnostics[0].code, DiagnosticCode::ResultPropagationReturnMismatch);
}
