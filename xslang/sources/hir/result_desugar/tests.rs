// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

#[cfg(test)]
mod tests
{
    use super::*;
    use crate::hir::type_check::PrimitiveType;

    fn span(start: u32, end: u32) -> Span
    {
        Span::new(1, start, end)
    }

    fn local(name: &str, ty: Type) -> Local
    {
        Local {
            name: name.to_string(),
            ty,
            mutable: false,
            span: span(0, 0),
        }
    }

    fn primitive(kind: PrimitiveType) -> Type
    {
        Type::Primitive(kind)
    }

    fn named(name: &str) -> Type
    {
        Type::Named(name.to_string())
    }

    #[test]
    fn desugars_result_propagation_to_explicit_result_match()
    {
        let function = Function {
            name: "try_work".to_string(),
            return_type: Some(named("Result<()>")),
            locals: vec![local("work", named("Result<Long, Error>"))],
            body: vec![Statement::Expr(Expression::ResultPropagation {
                value: Box::new(Expression::Local {
                    name: "work".to_string(),
                    span: span(4, 8),
                }),
                span: span(4, 9),
            })],
        };

        let desugared = ResultDesugar::new()
            .desugar_function(&function)
            .expect("valid Result propagation should desugar");

        let DesugaredStatement::Expr(DesugaredExpression::ResultMatch {
            success_binding,
            error_binding,
            success_type,
            error_type,
            ..
        }) = &desugared.body[0]
        else
        {
            panic!("expected explicit Result match desugar");
        };
        assert_eq!(success_binding, "__xs_try_ok_0");
        assert_eq!(error_binding, "__xs_try_error_0");
        assert_eq!(success_type, &primitive(PrimitiveType::Long));
        assert_eq!(error_type, &named("Error"));
    }

    #[test]
    fn rejects_non_result_propagation_value()
    {
        let function = Function {
            name: "bad".to_string(),
            return_type: Some(named("Result<()>")),
            locals: vec![local("value", primitive(PrimitiveType::Long))],
            body: vec![Statement::Expr(Expression::ResultPropagation {
                value: Box::new(Expression::Local {
                    name: "value".to_string(),
                    span: span(4, 9),
                }),
                span: span(4, 10),
            })],
        };

        let diagnostics = ResultDesugar::new()
            .desugar_function(&function)
            .expect_err("non-Result value cannot desugar");

        assert_eq!(diagnostics[0].code, DiagnosticCode::RequiresResult);
    }

    #[test]
    fn desugars_unit_result_with_default_error()
    {
        let function = Function {
            name: "try_work".to_string(),
            return_type: Some(named("Result<()>")),
            locals: vec![local("work", named("Result<()>"))],
            body: vec![Statement::Expr(Expression::ResultPropagation {
                value: Box::new(Expression::Local {
                    name: "work".to_string(),
                    span: span(4, 8),
                }),
                span: span(4, 9),
            })],
        };

        let desugared = ResultDesugar::new()
            .desugar_function(&function)
            .expect("unit Result shorthand should use Error");

        let DesugaredStatement::Expr(DesugaredExpression::ResultMatch {
            error_type, ..
        }) = &desugared.body[0]
        else
        {
            panic!("expected explicit Result match desugar");
        };
        assert_eq!(error_type, &named("Error"));
    }

    #[test]
    fn rejects_error_type_mismatch()
    {
        let function = Function {
            name: "bad".to_string(),
            return_type: Some(named("Result<Bool, Other>")),
            locals: vec![local("work", named("Result<Long, Error>"))],
            body: vec![Statement::Expr(Expression::ResultPropagation {
                value: Box::new(Expression::Local {
                    name: "work".to_string(),
                    span: span(4, 8),
                }),
                span: span(4, 9),
            })],
        };

        let diagnostics = ResultDesugar::new()
            .desugar_function(&function)
            .expect_err("mismatched error type cannot desugar");

        assert_eq!(diagnostics[0].code, DiagnosticCode::ReturnMismatch);
    }

    #[test]
    fn match_payload_binding_is_visible_to_nested_propagation()
    {
        let result_type = Type::Result {
            success: Box::new(primitive(PrimitiveType::Long)),
            error: Box::new(named("Error")),
        };
        let function = Function {
            name: "flatten".to_string(),
            return_type: Some(result_type.clone()),
            locals: vec![local("outer", Type::Result {
                success: Box::new(result_type.clone()),
                error: Box::new(named("Error")),
            })],
            body: vec![Statement::Return {
                value: Some(Expression::Match {
                    selector: Box::new(Expression::Local {
                        name: "outer".to_string(),
                        span: span(2, 7),
                    }),
                    selector_type: Box::new(Type::Result {
                        success: Box::new(result_type.clone()),
                        error: Box::new(named("Error")),
                    }),
                    arms: vec![
                        super::super::match_model::MatchArm {
                            pattern: MatchPattern::ResultVariant {
                                success: true,
                                binding: Some("inner".to_string()),
                                payload_type: result_type.clone(),
                            },
                            body: Block {
                                statements: vec![],
                                tail: Some(Box::new(Expression::Call {
                                    function: "Ok".to_string(),
                                    arguments: vec![Expression::ResultPropagation {
                                        value: Box::new(Expression::Local {
                                            name: "inner".to_string(),
                                            span: span(12, 17),
                                        }),
                                        span: span(12, 18),
                                    }],
                                    parameter_types: vec![primitive(PrimitiveType::Long)],
                                    return_type: Box::new(result_type.clone()),
                                    span: span(9, 19),
                                })),
                                span: span(8, 20),
                            },
                            span: span(8, 20),
                        },
                        super::super::match_model::MatchArm {
                            pattern: MatchPattern::ResultVariant {
                                success: false,
                                binding: Some("failure".to_string()),
                                payload_type: named("Error"),
                            },
                            body: Block {
                                statements: vec![],
                                tail: Some(Box::new(Expression::Call {
                                    function: "Error".to_string(),
                                    arguments: vec![Expression::Local {
                                        name: "failure".to_string(),
                                        span: span(24, 31),
                                    }],
                                    parameter_types: vec![named("Error")],
                                    return_type: Box::new(result_type.clone()),
                                    span: span(22, 32),
                                })),
                                span: span(21, 33),
                            },
                            span: span(21, 33),
                        },
                    ],
                    result_type: Box::new(result_type),
                    span: span(2, 33),
                }),
                span: span(0, 34),
            }],
        };

        let desugared = ResultDesugar::new()
            .desugar_function(&function)
            .expect("Result arm payload must be in scope while its block is desugared");
        let DesugaredStatement::Return {
            value: Some(DesugaredExpression::Match {
                arms, ..
            }),
            ..
        } = &desugared.body[0]
        else
        {
            panic!("expected a desugared match return");
        };
        let Some(DesugaredExpression::Call {
            arguments, ..
        }) = arms[0].body.tail.as_deref()
        else
        {
            panic!("expected the Ok constructor in the success arm");
        };
        assert!(matches!(&arguments[0], DesugaredExpression::ResultMatch {
            success_type: Type::Primitive(PrimitiveType::Long),
            ..
        }));
    }
}
