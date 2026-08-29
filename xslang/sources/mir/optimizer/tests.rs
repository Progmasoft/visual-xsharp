// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

#[cfg(test)]
mod tests
{
    use super::*;
    use crate::hir::async_check::Span;
    use crate::mir::{BasicBlock, BlockId, Function, Local, Terminator};
    use crate::xlil::Type;

    fn span(start: u32, end: u32) -> Span
    {
        Span::new(1, start, end)
    }

    fn local(id: u32) -> Local
    {
        Local {
            id: LocalId(id),
            name: format!("local{id}"),
            value_type: None,
            mutable: true,
            span: span(0, 1),
        }
    }

    #[test]
    fn removes_unreachable_blocks()
    {
        let function = Function {
            name: "main".to_string(),
            parameters: vec![],
            return_type: Type::VOID,
            locals: vec![],
            blocks: vec![
                BasicBlock {
                    id: BlockId(0),
                    statements: vec![],
                    terminator: Some(Terminator::Goto(BlockId(1))),
                    span: span(0, 1),
                },
                BasicBlock {
                    id: BlockId(1),
                    statements: vec![],
                    terminator: Some(Terminator::Return(None)),
                    span: span(1, 2),
                },
                BasicBlock {
                    id: BlockId(2),
                    statements: vec![],
                    terminator: Some(Terminator::Return(None)),
                    span: span(2, 3),
                },
            ],
        };

        let optimized = optimize_function(function);

        assert_eq!(optimized.function.blocks.len(), 1);
        assert_eq!(optimized.reports[0].pass, OptimizationPass::RemoveUnreachableBlocks);
        assert_eq!(optimized.reports[0].removed_items, 1);
        assert!(
            optimized.reports.iter().any(
                |report| report.pass == OptimizationPass::CollapseSinglePredecessorGoto && report.removed_items == 1
            )
        );
    }

    #[test]
    fn removes_redundant_end_borrow()
    {
        let function = Function {
            name: "main".to_string(),
            parameters: vec![],
            return_type: Type::VOID,
            locals: vec![local(0)],
            blocks: vec![BasicBlock {
                id: BlockId(0),
                statements: vec![
                    Statement::EndBorrow {
                        local: LocalId(0),
                        span: span(1, 2),
                    },
                    Statement::BorrowShared {
                        local: LocalId(0),
                        span: span(2, 3),
                    },
                    Statement::EndBorrow {
                        local: LocalId(0),
                        span: span(3, 4),
                    },
                ],
                terminator: Some(Terminator::Return(None)),
                span: span(0, 4),
            }],
        };

        let optimized = optimize_function(function);

        assert_eq!(optimized.function.blocks[0].statements.len(), 2);
        assert_eq!(optimized.reports[0].pass, OptimizationPass::RemoveRedundantEndBorrow);
        assert_eq!(optimized.reports[0].removed_items, 1);
    }

    #[test]
    fn verified_optimizer_rejects_invalid_input()
    {
        let function = Function {
            name: "bad".to_string(),
            parameters: vec![],
            return_type: Type::VOID,
            locals: vec![],
            blocks: vec![BasicBlock {
                id: BlockId(0),
                statements: vec![Statement::Use {
                    local: LocalId(9),
                    span: span(1, 2),
                }],
                terminator: Some(Terminator::Return(None)),
                span: span(0, 3),
            }],
        };

        let error = optimize_verified_function(function).expect_err("invalid input must fail");

        assert!(matches!(error, OptimizationError::InputInvalid(_)));
    }

    #[test]
    fn verified_optimizer_returns_valid_output()
    {
        let function = Function {
            name: "main".to_string(),
            parameters: vec![],
            return_type: Type::VOID,
            locals: vec![local(0)],
            blocks: vec![BasicBlock {
                id: BlockId(0),
                statements: vec![
                    Statement::BorrowShared {
                        local: LocalId(0),
                        span: span(1, 2),
                    },
                    Statement::EndBorrow {
                        local: LocalId(0),
                        span: span(2, 3),
                    },
                ],
                terminator: Some(Terminator::Return(None)),
                span: span(0, 4),
            }],
        };

        let optimized = optimize_verified_function(function).expect("valid input should optimize");

        assert!(verify_function(&optimized.function).is_empty());
        assert!(optimized.reports.is_empty());
    }

    #[test]
    fn folds_const_i64_adds()
    {
        let function = Function {
            name: "add".to_string(),
            parameters: vec![],
            return_type: Type::I64,
            locals: vec![
                Local {
                    id: LocalId(0),
                    name: "left".to_string(),
                    value_type: Some(Type::I64),
                    mutable: false,
                    span: span(0, 1),
                },
                Local {
                    id: LocalId(1),
                    name: "right".to_string(),
                    value_type: Some(Type::I64),
                    mutable: false,
                    span: span(0, 1),
                },
                Local {
                    id: LocalId(2),
                    name: "sum".to_string(),
                    value_type: Some(Type::I64),
                    mutable: false,
                    span: span(0, 1),
                },
            ],
            blocks: vec![BasicBlock {
                id: BlockId(0),
                statements: vec![
                    Statement::ConstI64 {
                        local: LocalId(0),
                        value: 2,
                        span: span(1, 2),
                    },
                    Statement::ConstI64 {
                        local: LocalId(1),
                        value: 3,
                        span: span(2, 3),
                    },
                    Statement::AddI64 {
                        result: LocalId(2),
                        left: LocalId(0),
                        right: LocalId(1),
                        span: span(3, 4),
                    },
                ],
                terminator: Some(Terminator::Return(Some(LocalId(2)))),
                span: span(0, 4),
            }],
        };

        let optimized = optimize_verified_function(function).expect("valid input should optimize");

        assert_eq!(optimized.reports[0].pass, OptimizationPass::FoldConstI64Add);
        assert!(matches!(
            optimized.function.blocks[0].statements[2],
            Statement::ConstI64 {
                local: LocalId(2),
                value: 5,
                ..
            }
        ));
    }

    #[test]
    fn folds_const_i64_subs()
    {
        let function = Function {
            name: "sub".to_string(),
            parameters: vec![],
            return_type: Type::I64,
            locals: vec![
                Local {
                    id: LocalId(0),
                    name: "left".to_string(),
                    value_type: Some(Type::I64),
                    mutable: false,
                    span: span(0, 1),
                },
                Local {
                    id: LocalId(1),
                    name: "right".to_string(),
                    value_type: Some(Type::I64),
                    mutable: false,
                    span: span(0, 1),
                },
                Local {
                    id: LocalId(2),
                    name: "difference".to_string(),
                    value_type: Some(Type::I64),
                    mutable: false,
                    span: span(0, 1),
                },
            ],
            blocks: vec![BasicBlock {
                id: BlockId(0),
                statements: vec![
                    Statement::ConstI64 {
                        local: LocalId(0),
                        value: 8,
                        span: span(1, 2),
                    },
                    Statement::ConstI64 {
                        local: LocalId(1),
                        value: 3,
                        span: span(2, 3),
                    },
                    Statement::SubI64 {
                        result: LocalId(2),
                        left: LocalId(0),
                        right: LocalId(1),
                        span: span(3, 4),
                    },
                ],
                terminator: Some(Terminator::Return(Some(LocalId(2)))),
                span: span(0, 4),
            }],
        };

        let optimized = optimize_verified_function(function).expect("valid input should optimize");

        assert_eq!(optimized.reports[0].pass, OptimizationPass::FoldConstI64Sub);
        assert!(matches!(
            optimized.function.blocks[0].statements[2],
            Statement::ConstI64 {
                local: LocalId(2),
                value: 5,
                ..
            }
        ));
    }

    #[test]
    fn folds_const_i64_muls()
    {
        let function = Function {
            name: "mul".to_string(),
            parameters: vec![],
            return_type: Type::I64,
            locals: vec![
                Local {
                    id: LocalId(0),
                    name: "left".to_string(),
                    value_type: Some(Type::I64),
                    mutable: false,
                    span: span(0, 1),
                },
                Local {
                    id: LocalId(1),
                    name: "right".to_string(),
                    value_type: Some(Type::I64),
                    mutable: false,
                    span: span(0, 1),
                },
                Local {
                    id: LocalId(2),
                    name: "product".to_string(),
                    value_type: Some(Type::I64),
                    mutable: false,
                    span: span(0, 1),
                },
            ],
            blocks: vec![BasicBlock {
                id: BlockId(0),
                statements: vec![
                    Statement::ConstI64 {
                        local: LocalId(0),
                        value: 6,
                        span: span(1, 2),
                    },
                    Statement::ConstI64 {
                        local: LocalId(1),
                        value: 7,
                        span: span(2, 3),
                    },
                    Statement::MulI64 {
                        result: LocalId(2),
                        left: LocalId(0),
                        right: LocalId(1),
                        span: span(3, 4),
                    },
                ],
                terminator: Some(Terminator::Return(Some(LocalId(2)))),
                span: span(0, 4),
            }],
        };

        let optimized = optimize_verified_function(function).expect("valid input should optimize");

        assert_eq!(optimized.reports[0].pass, OptimizationPass::FoldConstI64Mul);
        assert!(matches!(
            optimized.function.blocks[0].statements[2],
            Statement::ConstI64 {
                local: LocalId(2),
                value: 42,
                ..
            }
        ));
    }

    #[test]
    fn folds_const_i64_eqs()
    {
        let function = Function {
            name: "eq".to_string(),
            parameters: vec![],
            return_type: Type::BOOL,
            locals: vec![
                Local {
                    id: LocalId(0),
                    name: "left".to_string(),
                    value_type: Some(Type::I64),
                    mutable: false,
                    span: span(0, 1),
                },
                Local {
                    id: LocalId(1),
                    name: "right".to_string(),
                    value_type: Some(Type::I64),
                    mutable: false,
                    span: span(0, 1),
                },
                Local {
                    id: LocalId(2),
                    name: "same".to_string(),
                    value_type: Some(Type::BOOL),
                    mutable: false,
                    span: span(0, 1),
                },
            ],
            blocks: vec![BasicBlock {
                id: BlockId(0),
                statements: vec![
                    Statement::ConstI64 {
                        local: LocalId(0),
                        value: 7,
                        span: span(1, 2),
                    },
                    Statement::ConstI64 {
                        local: LocalId(1),
                        value: 7,
                        span: span(2, 3),
                    },
                    Statement::EqI64 {
                        result: LocalId(2),
                        left: LocalId(0),
                        right: LocalId(1),
                        span: span(3, 4),
                    },
                ],
                terminator: Some(Terminator::Return(Some(LocalId(2)))),
                span: span(0, 4),
            }],
        };

        let optimized = optimize_verified_function(function).expect("valid input should optimize");

        assert_eq!(optimized.reports[0].pass, OptimizationPass::FoldConstI64Eq);
        assert!(matches!(
            optimized.function.blocks[0].statements[2],
            Statement::ConstBool {
                local: LocalId(2),
                value: true,
                ..
            }
        ));
    }

    #[test]
    fn folds_const_bool_branch_and_removes_dead_target()
    {
        let function = Function {
            name: "branch".to_string(),
            parameters: vec![],
            return_type: Type::VOID,
            locals: vec![Local {
                id: LocalId(0),
                name: "condition".to_string(),
                value_type: Some(Type::BOOL),
                mutable: false,
                span: span(0, 1),
            }],
            blocks: vec![
                BasicBlock {
                    id: BlockId(0),
                    statements: vec![Statement::ConstBool {
                        local: LocalId(0),
                        value: true,
                        span: span(1, 2),
                    }],
                    terminator: Some(Terminator::BranchIf {
                        condition: LocalId(0),
                        then_block: BlockId(1),
                        else_block: BlockId(2),
                    }),
                    span: span(0, 3),
                },
                BasicBlock {
                    id: BlockId(1),
                    statements: vec![],
                    terminator: Some(Terminator::Return(None)),
                    span: span(3, 4),
                },
                BasicBlock {
                    id: BlockId(2),
                    statements: vec![],
                    terminator: Some(Terminator::Return(None)),
                    span: span(4, 5),
                },
            ],
        };

        let optimized = optimize_verified_function(function).expect("valid input should optimize");

        assert!(
            optimized
                .reports
                .iter()
                .any(|report| report.pass == OptimizationPass::FoldConstBoolBranch && report.removed_items == 1)
        );
        assert!(
            optimized
                .reports
                .iter()
                .any(|report| report.pass == OptimizationPass::RemoveUnreachableBlocks && report.removed_items == 1)
        );
        assert_eq!(optimized.function.blocks.len(), 1);
        assert_eq!(optimized.function.blocks[0].terminator, Some(Terminator::Return(None)));
    }
}
