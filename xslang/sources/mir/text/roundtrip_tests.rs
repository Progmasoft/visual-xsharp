#[cfg(test)]
mod tests
{
    use super::*;
    use crate::hir::async_check::Span;
    use crate::mir::{BasicBlock, BlockId, Function, Local, LocalId, Parameter, Statement, Terminator};

    fn span() -> Span
    {
        Span::new(1, 0, 1)
    }

    #[test]
    fn writes_function_as_structured_xmir()
    {
        let function = Function {
            name: "Main".to_string(),
            parameters: vec![Parameter {
                local: LocalId(0),
                name: "argc".to_string(),
                value_type: crate::xlil::Type::I64,
                span: span(),
            }],
            return_type: crate::xlil::Type::VOID,
            locals: vec![Local {
                id: LocalId(1),
                name: "message".to_string(),
                value_type: None,
                mutable: false,
                span: span(),
            }],
            blocks: vec![BasicBlock {
                id: BlockId(0),
                statements: vec![
                    Statement::BorrowShared {
                        local: LocalId(1),
                        span: span(),
                    },
                    Statement::EndBorrow {
                        local: LocalId(1),
                        span: span(),
                    },
                ],
                terminator: Some(Terminator::Return(None)),
                span: span(),
            }],
        };

        let text = function_to_xmir(&function);

        assert!(text.contains(".xmir version 1\nfunction Main"));
        assert!(text.contains("returns void"));
        assert!(text.contains("parameters"));
        assert!(text.contains("parameter argc"));
        assert!(text.contains("type i64"));
        assert!(text.contains("control_flow"));
        assert!(text.contains("statement borrow shared"));
        assert!(text.contains("terminator return"));
        assert!(text.contains(".end\n.program end\n"));
        assert!(!text.contains(".func"));
        assert!(!text.contains("%0"));

        let header = parse_xmir_header(&text).expect("header should parse");
        assert_eq!(header.function, "Main");

        let parsed = parse_xmir_function(&text).expect("XMIR function should parse");
        assert_eq!(parsed.name, "Main");
        assert_eq!(parsed.parameters.len(), 1);
        assert_eq!(parsed.parameters[0].local, LocalId(0));
        assert_eq!(parsed.parameters[0].name, "argc");
        assert_eq!(parsed.parameters[0].value_type, crate::xlil::Type::I64);
        assert_eq!(parsed.locals.len(), 1);
        assert_eq!(parsed.blocks.len(), 1);
        assert_eq!(parsed.blocks[0].statements.len(), 2);
        assert_eq!(parsed.blocks[0].terminator, Some(Terminator::Return(None)));
    }

    #[test]
    fn roundtrips_goto_return_value_panic_and_unreachable()
    {
        let function = Function {
            name: "Flow".to_string(),
            parameters: vec![],
            return_type: crate::xlil::Type::I64,
            locals: vec![Local {
                id: LocalId(0),
                name: "result".to_string(),
                value_type: Some(crate::xlil::Type::I64),
                mutable: true,
                span: span(),
            }],
            blocks: vec![
                BasicBlock {
                    id: BlockId(0),
                    statements: vec![
                        Statement::ConstI64 {
                            local: LocalId(0),
                            value: 7,
                            span: span(),
                        },
                        Statement::Use {
                            local: LocalId(0),
                            span: span(),
                        },
                        Statement::Move {
                            local: LocalId(0),
                            span: span(),
                        },
                        Statement::Drop {
                            local: LocalId(0),
                            span: span(),
                        },
                    ],
                    terminator: Some(Terminator::Goto(BlockId(1))),
                    span: span(),
                },
                BasicBlock {
                    id: BlockId(1),
                    statements: vec![Statement::BorrowMutable {
                        local: LocalId(0),
                        span: span(),
                    }],
                    terminator: Some(Terminator::Return(Some(LocalId(0)))),
                    span: span(),
                },
                BasicBlock {
                    id: BlockId(2),
                    statements: vec![],
                    terminator: Some(Terminator::Panic),
                    span: span(),
                },
                BasicBlock {
                    id: BlockId(3),
                    statements: vec![],
                    terminator: Some(Terminator::Unreachable),
                    span: span(),
                },
            ],
        };

        let text = function_to_xmir(&function);
        let parsed = parse_xmir_function(&text).expect("XMIR function should parse");

        assert_eq!(parsed.name, "Flow");
        assert_eq!(parsed.return_type, crate::xlil::Type::I64);
        assert_eq!(parsed.locals[0].name, "result");
        assert_eq!(parsed.locals[0].value_type, Some(crate::xlil::Type::I64));
        assert!(parsed.locals[0].mutable);
        assert_eq!(parsed.blocks[0].terminator, Some(Terminator::Goto(BlockId(1))));
        assert_eq!(parsed.blocks[1].terminator, Some(Terminator::Return(Some(LocalId(0)))));
        assert_eq!(parsed.blocks[2].terminator, Some(Terminator::Panic));
        assert_eq!(parsed.blocks[3].terminator, Some(Terminator::Unreachable));
        assert!(matches!(parsed.blocks[0].statements[0], Statement::ConstI64 {
            local: LocalId(0),
            value: 7,
            ..
        }));
        assert!(matches!(parsed.blocks[0].statements[1], Statement::Use {
            local: LocalId(0),
            ..
        }));
        assert!(matches!(parsed.blocks[0].statements[2], Statement::Move {
            local: LocalId(0),
            ..
        }));
        assert!(matches!(parsed.blocks[0].statements[3], Statement::Drop {
            local: LocalId(0),
            ..
        }));
        assert!(matches!(parsed.blocks[1].statements[0], Statement::BorrowMutable {
            local: LocalId(0),
            ..
        }));
    }

    #[test]
    fn roundtrips_call_statement()
    {
        let function = Function {
            name: "CallFlow".to_string(),
            parameters: vec![],
            return_type: crate::xlil::Type::I64,
            locals: vec![
                Local {
                    id: LocalId(0),
                    name: "argument".to_string(),
                    value_type: Some(crate::xlil::Type::I64),
                    mutable: false,
                    span: span(),
                },
                Local {
                    id: LocalId(1),
                    name: "result".to_string(),
                    value_type: Some(crate::xlil::Type::I64),
                    mutable: false,
                    span: span(),
                },
            ],
            blocks: vec![BasicBlock {
                id: BlockId(0),
                statements: vec![
                    Statement::ConstI64 {
                        local: LocalId(0),
                        value: 7,
                        span: span(),
                    },
                    Statement::Call {
                        result: Some(LocalId(1)),
                        function: "xs$App$Callee".to_string(),
                        arguments: vec![LocalId(0)],
                        return_type: crate::xlil::Type::I64,
                        span: span(),
                    },
                ],
                terminator: Some(Terminator::Return(Some(LocalId(1)))),
                span: span(),
            }],
        };

        let text = function_to_xmir(&function);
        let parsed = parse_xmir_function(&text).expect("XMIR function should parse");

        assert!(matches!(&parsed.blocks[0].statements[1],
                     Statement::Call { result: Some(LocalId(1)),
                                       function,
                                       arguments,
                                       return_type,
                                       .. }
                     if function == "xs$App$Callee" &&
                        arguments == &vec![LocalId(0)] &&
                        *return_type == crate::xlil::Type::I64));
    }

    #[test]
    fn roundtrips_add_i64_statement()
    {
        let function = Function {
            name: "AddFlow".to_string(),
            parameters: vec![],
            return_type: crate::xlil::Type::I64,
            locals: vec![
                Local {
                    id: LocalId(0),
                    name: "left".to_string(),
                    value_type: Some(crate::xlil::Type::I64),
                    mutable: false,
                    span: span(),
                },
                Local {
                    id: LocalId(1),
                    name: "right".to_string(),
                    value_type: Some(crate::xlil::Type::I64),
                    mutable: false,
                    span: span(),
                },
                Local {
                    id: LocalId(2),
                    name: "sum".to_string(),
                    value_type: Some(crate::xlil::Type::I64),
                    mutable: false,
                    span: span(),
                },
            ],
            blocks: vec![BasicBlock {
                id: BlockId(0),
                statements: vec![
                    Statement::ConstI64 {
                        local: LocalId(0),
                        value: 2,
                        span: span(),
                    },
                    Statement::ConstI64 {
                        local: LocalId(1),
                        value: 3,
                        span: span(),
                    },
                    Statement::AddI64 {
                        result: LocalId(2),
                        left: LocalId(0),
                        right: LocalId(1),
                        span: span(),
                    },
                ],
                terminator: Some(Terminator::Return(Some(LocalId(2)))),
                span: span(),
            }],
        };

        let text = function_to_xmir(&function);
        let parsed = parse_xmir_function(&text).expect("XMIR function should parse");

        assert!(matches!(parsed.blocks[0].statements[2], Statement::AddI64 {
            result: LocalId(2),
            left: LocalId(0),
            right: LocalId(1),
            ..
        }));
    }

    #[test]
    fn roundtrips_sub_i64_statement()
    {
        let function = Function {
            name: "SubFlow".to_string(),
            parameters: vec![],
            return_type: crate::xlil::Type::I64,
            locals: vec![
                Local {
                    id: LocalId(0),
                    name: "left".to_string(),
                    value_type: Some(crate::xlil::Type::I64),
                    mutable: false,
                    span: span(),
                },
                Local {
                    id: LocalId(1),
                    name: "right".to_string(),
                    value_type: Some(crate::xlil::Type::I64),
                    mutable: false,
                    span: span(),
                },
                Local {
                    id: LocalId(2),
                    name: "difference".to_string(),
                    value_type: Some(crate::xlil::Type::I64),
                    mutable: false,
                    span: span(),
                },
            ],
            blocks: vec![BasicBlock {
                id: BlockId(0),
                statements: vec![
                    Statement::ConstI64 {
                        local: LocalId(0),
                        value: 8,
                        span: span(),
                    },
                    Statement::ConstI64 {
                        local: LocalId(1),
                        value: 3,
                        span: span(),
                    },
                    Statement::SubI64 {
                        result: LocalId(2),
                        left: LocalId(0),
                        right: LocalId(1),
                        span: span(),
                    },
                ],
                terminator: Some(Terminator::Return(Some(LocalId(2)))),
                span: span(),
            }],
        };

        let text = function_to_xmir(&function);
        let parsed = parse_xmir_function(&text).expect("XMIR function should parse");

        assert!(matches!(parsed.blocks[0].statements[2], Statement::SubI64 {
            result: LocalId(2),
            left: LocalId(0),
            right: LocalId(1),
            ..
        }));
    }

    #[test]
    fn roundtrips_mul_i64_statement()
    {
        let function = Function {
            name: "MulFlow".to_string(),
            parameters: vec![],
            return_type: crate::xlil::Type::I64,
            locals: vec![
                Local {
                    id: LocalId(0),
                    name: "left".to_string(),
                    value_type: Some(crate::xlil::Type::I64),
                    mutable: false,
                    span: span(),
                },
                Local {
                    id: LocalId(1),
                    name: "right".to_string(),
                    value_type: Some(crate::xlil::Type::I64),
                    mutable: false,
                    span: span(),
                },
                Local {
                    id: LocalId(2),
                    name: "product".to_string(),
                    value_type: Some(crate::xlil::Type::I64),
                    mutable: false,
                    span: span(),
                },
            ],
            blocks: vec![BasicBlock {
                id: BlockId(0),
                statements: vec![
                    Statement::ConstI64 {
                        local: LocalId(0),
                        value: 6,
                        span: span(),
                    },
                    Statement::ConstI64 {
                        local: LocalId(1),
                        value: 7,
                        span: span(),
                    },
                    Statement::MulI64 {
                        result: LocalId(2),
                        left: LocalId(0),
                        right: LocalId(1),
                        span: span(),
                    },
                ],
                terminator: Some(Terminator::Return(Some(LocalId(2)))),
                span: span(),
            }],
        };

        let text = function_to_xmir(&function);
        let parsed = parse_xmir_function(&text).expect("XMIR function should parse");

        assert!(matches!(parsed.blocks[0].statements[2], Statement::MulI64 {
            result: LocalId(2),
            left: LocalId(0),
            right: LocalId(1),
            ..
        }));
    }

    #[test]
    fn roundtrips_const_bool_and_eq_i64_statement()
    {
        let function = Function {
            name: "EqFlow".to_string(),
            parameters: vec![],
            return_type: crate::xlil::Type::BOOL,
            locals: vec![
                Local {
                    id: LocalId(0),
                    name: "left".to_string(),
                    value_type: Some(crate::xlil::Type::I64),
                    mutable: false,
                    span: span(),
                },
                Local {
                    id: LocalId(1),
                    name: "right".to_string(),
                    value_type: Some(crate::xlil::Type::I64),
                    mutable: false,
                    span: span(),
                },
                Local {
                    id: LocalId(2),
                    name: "same".to_string(),
                    value_type: Some(crate::xlil::Type::BOOL),
                    mutable: false,
                    span: span(),
                },
            ],
            blocks: vec![BasicBlock {
                id: BlockId(0),
                statements: vec![
                    Statement::ConstI64 {
                        local: LocalId(0),
                        value: 7,
                        span: span(),
                    },
                    Statement::ConstI64 {
                        local: LocalId(1),
                        value: 7,
                        span: span(),
                    },
                    Statement::EqI64 {
                        result: LocalId(2),
                        left: LocalId(0),
                        right: LocalId(1),
                        span: span(),
                    },
                ],
                terminator: Some(Terminator::Return(Some(LocalId(2)))),
                span: span(),
            }],
        };

        let text = function_to_xmir(&function);
        let parsed = parse_xmir_function(&text).expect("XMIR function should parse");

        assert!(matches!(parsed.blocks[0].statements[2], Statement::EqI64 {
            result: LocalId(2),
            left: LocalId(0),
            right: LocalId(1),
            ..
        }));
    }

    #[test]
    fn roundtrips_branch_if_terminator()
    {
        let function = Function {
            name: "BranchIfFlow".to_string(),
            parameters: vec![],
            return_type: crate::xlil::Type::VOID,
            locals: vec![Local {
                id: LocalId(0),
                name: "condition".to_string(),
                value_type: Some(crate::xlil::Type::BOOL),
                mutable: false,
                span: span(),
            }],
            blocks: vec![
                BasicBlock {
                    id: BlockId(0),
                    statements: vec![Statement::ConstBool {
                        local: LocalId(0),
                        value: true,
                        span: span(),
                    }],
                    terminator: Some(Terminator::BranchIf {
                        condition: LocalId(0),
                        then_block: BlockId(1),
                        else_block: BlockId(2),
                    }),
                    span: span(),
                },
                BasicBlock {
                    id: BlockId(1),
                    statements: vec![],
                    terminator: Some(Terminator::Return(None)),
                    span: span(),
                },
                BasicBlock {
                    id: BlockId(2),
                    statements: vec![],
                    terminator: Some(Terminator::Return(None)),
                    span: span(),
                },
            ],
        };

        let text = function_to_xmir(&function);
        let parsed = parse_xmir_function(&text).expect("XMIR function should parse");

        assert!(text.contains("terminator branch_if"));
        assert_eq!(
            parsed.blocks[0].terminator,
            Some(Terminator::BranchIf {
                condition: LocalId(0),
                then_block: BlockId(1),
                else_block: BlockId(2)
            })
        );
    }

    #[test]
    fn rejects_non_xmir_header()
    {
        assert_eq!(parse_xmir_header(".func Main : () -> void\n"), None);
    }

    #[test]
    fn rejects_unsupported_xmir_version()
    {
        let diagnostics = parse_xmir_function(".xmir version 2\nfunction Main\nreturns void\n")
            .expect_err("unsupported XMIR version must fail");

        assert!(diagnostics[0].message.contains("unsupported XMIR version 2"));
    }

    #[test]
    fn parses_explicit_end_markers_without_indentation()
    {
        let text = ".xmir version 0\nfunction Main\nreturns void\n\nparameters\nparameter value\nlocal 0\ntype \
                    i64\n.end\nlocals\nlocal 1\nname value\ntype i64\nmutability immutable\n.end\ncontrol_flow\nblock \
                    0\nstatements\nstatement use\nlocal 1\nterminator return\n.end\n.program end\n";

        let parsed = parse_xmir_function(text).expect("XMIR should not be indentation based");

        assert_eq!(parsed.parameters.len(), 1);
        assert_eq!(parsed.locals.len(), 1);
        assert_eq!(parsed.blocks.len(), 1);
        assert_eq!(parsed.blocks[0].statements.len(), 1);
        assert_eq!(parsed.blocks[0].terminator, Some(Terminator::Return(None)));
    }

    #[test]
    fn rejects_invalid_xmir_function()
    {
        let diagnostics = parse_xmir_function(
            ".xmir version 1\nfunction Main\n\ncontrol_flow\n  block nope\n    terminator return\n",
        )
        .expect_err("invalid block id should fail");

        assert_eq!(diagnostics.len(), 1);
    }

    #[test]
    fn parsed_xmir_can_be_structurally_verified()
    {
        let function = Function {
            name: "Verified".to_string(),
            parameters: vec![],
            return_type: crate::xlil::Type::I64,
            locals: vec![Local {
                id: LocalId(0),
                name: "value".to_string(),
                value_type: Some(crate::xlil::Type::I64),
                mutable: false,
                span: span(),
            }],
            blocks: vec![BasicBlock {
                id: BlockId(0),
                statements: vec![Statement::Use {
                    local: LocalId(0),
                    span: span(),
                }],
                terminator: Some(Terminator::Return(Some(LocalId(0)))),
                span: span(),
            }],
        };
        let text = function_to_xmir(&function);
        let parsed = parse_xmir_function(&text).expect("XMIR function should parse");

        assert!(crate::mir::verify::verify_function(&parsed).is_empty());
    }

    #[test]
    fn roundtrips_const_i32_statement()
    {
        let function = Function {
            name: "main".to_string(),
            parameters: vec![],
            return_type: crate::xlil::Type::I32,
            locals: vec![Local {
                id: LocalId(0),
                name: "exit_code".to_string(),
                value_type: Some(crate::xlil::Type::I32),
                mutable: false,
                span: span(),
            }],
            blocks: vec![BasicBlock {
                id: BlockId(0),
                statements: vec![Statement::ConstI32 {
                    local: LocalId(0),
                    value: 0,
                    span: span(),
                }],
                terminator: Some(Terminator::Return(Some(LocalId(0)))),
                span: span(),
            }],
        };
        let text = function_to_xmir(&function);
        let parsed = parse_xmir_function(&text).expect("XMIR i32 const should parse");

        assert_eq!(parsed.name, function.name);
        assert_eq!(parsed.return_type, crate::xlil::Type::I32);
        assert!(matches!(parsed.blocks[0].statements[0], Statement::ConstI32 {
            local: LocalId(0),
            value: 0,
            ..
        }));
        assert_eq!(parsed.blocks[0].terminator, Some(Terminator::Return(Some(LocalId(0)))));
    }

    #[test]
    fn roundtrips_floating_constant_bit_patterns()
    {
        let function = Function {
            name: "float_bits".to_string(),
            parameters: vec![],
            return_type: crate::xlil::Type::F64,
            locals: vec![Local {
                id: LocalId(0),
                name: "value".to_string(),
                value_type: Some(crate::xlil::Type::F64),
                mutable: false,
                span: span(),
            }],
            blocks: vec![BasicBlock {
                id: BlockId(0),
                statements: vec![Statement::ConstF64 {
                    local: LocalId(0),
                    bits: 0x3ff8000000000000,
                    span: span(),
                }],
                terminator: Some(Terminator::Return(Some(LocalId(0)))),
                span: span(),
            }],
        };
        let text = function_to_xmir(&function);
        assert!(text.contains("bits 0x3ff8000000000000"));
        let parsed = parse_xmir_function(&text).expect("floating XMIR should parse");
        assert_eq!(parsed.return_type, crate::xlil::Type::F64);
        assert!(matches!(parsed.blocks[0].statements[0], Statement::ConstF64 {
            bits: 0x3ff8000000000000,
            ..
        }));
    }

    #[test]
    fn roundtrips_i32_instruction_family()
    {
        let function = Function {
            name: "I32Flow".to_string(),
            parameters: vec![],
            return_type: crate::xlil::Type::VOID,
            locals: vec![
                Local {
                    id: LocalId(0),
                    name: "left".to_string(),
                    value_type: Some(crate::xlil::Type::I32),
                    mutable: false,
                    span: span(),
                },
                Local {
                    id: LocalId(1),
                    name: "right".to_string(),
                    value_type: Some(crate::xlil::Type::I32),
                    mutable: false,
                    span: span(),
                },
                Local {
                    id: LocalId(2),
                    name: "sum".to_string(),
                    value_type: Some(crate::xlil::Type::I32),
                    mutable: false,
                    span: span(),
                },
                Local {
                    id: LocalId(3),
                    name: "same".to_string(),
                    value_type: Some(crate::xlil::Type::BOOL),
                    mutable: false,
                    span: span(),
                },
            ],
            blocks: vec![BasicBlock {
                id: BlockId(0),
                statements: vec![
                    Statement::AddI32 {
                        result: LocalId(2),
                        left: LocalId(0),
                        right: LocalId(1),
                        span: span(),
                    },
                    Statement::EqI32 {
                        result: LocalId(3),
                        left: LocalId(0),
                        right: LocalId(1),
                        span: span(),
                    },
                    Statement::LtI32 {
                        result: LocalId(3),
                        left: LocalId(0),
                        right: LocalId(1),
                        span: span(),
                    },
                    Statement::LeI32 {
                        result: LocalId(3),
                        left: LocalId(0),
                        right: LocalId(1),
                        span: span(),
                    },
                    Statement::GtI32 {
                        result: LocalId(3),
                        left: LocalId(0),
                        right: LocalId(1),
                        span: span(),
                    },
                    Statement::GeI32 {
                        result: LocalId(3),
                        left: LocalId(0),
                        right: LocalId(1),
                        span: span(),
                    },
                ],
                terminator: Some(Terminator::Return(None)),
                span: span(),
            }],
        };

        let text = function_to_xmir(&function);
        let parsed = parse_xmir_function(&text).expect("XMIR i32 instructions should parse");

        assert!(text.contains("statement add.i32"));
        assert!(matches!(parsed.blocks[0].statements[0], Statement::AddI32 {
            result: LocalId(2),
            left: LocalId(0),
            right: LocalId(1),
            ..
        }));
        assert!(matches!(parsed.blocks[0].statements[1], Statement::EqI32 {
            result: LocalId(3),
            ..
        }));
        assert!(matches!(parsed.blocks[0].statements[2], Statement::LtI32 {
            result: LocalId(3),
            ..
        }));
        assert!(matches!(parsed.blocks[0].statements[3], Statement::LeI32 {
            result: LocalId(3),
            ..
        }));
        assert!(matches!(parsed.blocks[0].statements[4], Statement::GtI32 {
            result: LocalId(3),
            ..
        }));
        assert!(matches!(parsed.blocks[0].statements[5], Statement::GeI32 {
            result: LocalId(3),
            ..
        }));
    }

    #[test]
    fn roundtrips_optimizer_analysis()
    {
        let reports = vec![
            OptimizationReport {
                pass: OptimizationPass::RemoveUnreachableBlocks,
                removed_items: 2,
            },
            OptimizationReport {
                pass: OptimizationPass::RemoveRedundantEndBorrow,
                removed_items: 1,
            },
            OptimizationReport {
                pass: OptimizationPass::FoldConstBoolBranch,
                removed_items: 1,
            },
        ];

        let text = optimizer_analysis_to_xmir(&reports);
        let parsed = parse_xmir_optimizer_analysis(&text).expect("optimizer analysis should parse");

        assert_eq!(parsed, reports);
        assert!(text.contains("analysis optimizer"));
        assert!(text.contains("fold_const_bool_branch"));
        assert!(!text.contains(".pass"));
    }

    #[test]
    fn rejects_invalid_optimizer_analysis()
    {
        let diagnostics = parse_xmir_optimizer_analysis("analysis optimizer\n  pass nope\n    removed_items many\n")
            .expect_err("invalid optimizer analysis must fail");

        assert_eq!(diagnostics.len(), 2);
    }

    #[test]
    fn roundtrips_verify_analysis()
    {
        let diagnostics = vec![
            VerifyDiagnostic {
                code: VerifyDiagnosticCode::UnknownLocal,
                message: "local id 7 is not declared".to_string(),
                span: span(),
            },
            VerifyDiagnostic {
                code: VerifyDiagnosticCode::MissingTerminator,
                message: "MIR block is missing a terminator".to_string(),
                span: Span::new(1, 4, 8),
            },
        ];

        let text = verify_analysis_to_xmir(&diagnostics);
        let parsed = parse_xmir_verify_analysis(&text).expect("verify analysis should parse");

        assert_eq!(parsed, diagnostics);
        assert!(text.contains("analysis verify"));
        assert!(!text.contains(".diagnostic"));
    }

    #[test]
    fn rejects_invalid_verify_analysis()
    {
        let diagnostics =
            parse_xmir_verify_analysis("analysis verify\n  diagnostic nope\n    span bad\n    message broken\n")
                .expect_err("invalid verify analysis must fail");

        assert_eq!(diagnostics.len(), 2);
    }
}
