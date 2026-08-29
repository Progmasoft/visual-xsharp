// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

#[cfg(test)]
mod tests
{
    use super::*;

    fn span(start: u32, end: u32) -> Span
    {
        Span::new(1, start, end)
    }

    fn local(id: u32, mutable: bool) -> Local
    {
        Local {
            id: LocalId(id),
            name: format!("local{id}"),
            value_type: None,
            mutable,
            span: span(0, 1),
        }
    }

    fn function(statements: Vec<Statement>, terminator: Option<Terminator>) -> Function
    {
        Function {
            name: "main".to_string(),
            parameters: vec![],
            return_type: Type::VOID,
            locals: vec![local(0, true), local(1, false)],
            blocks: vec![BasicBlock {
                id: BlockId(0),
                statements,
                terminator,
                span: span(0, 10),
            }],
        }
    }

    #[test]
    fn accepts_move_after_borrow_ends()
    {
        let function = function(
            vec![
                Statement::BorrowShared {
                    local: LocalId(0),
                    span: span(1, 2),
                },
                Statement::EndBorrow {
                    local: LocalId(0),
                    span: span(2, 3),
                },
                Statement::Move {
                    local: LocalId(0),
                    span: span(3, 4),
                },
            ],
            Some(Terminator::Return(None)),
        );

        assert!(BorrowChecker::new().check_function(&function).is_empty());
    }

    #[test]
    fn rejects_use_after_move()
    {
        let function = function(
            vec![
                Statement::Move {
                    local: LocalId(0),
                    span: span(1, 2),
                },
                Statement::Use {
                    local: LocalId(0),
                    span: span(3, 4),
                },
            ],
            Some(Terminator::Return(None)),
        );

        let diagnostics = BorrowChecker::new().check_function(&function);

        assert_eq!(diagnostics.len(), 1);
        assert_eq!(diagnostics[0].code, DiagnosticCode::UseAfterMove);
    }

    #[test]
    fn rejects_branch_if_condition_after_move()
    {
        let function = function(
            vec![Statement::Move {
                local: LocalId(0),
                span: span(1, 2),
            }],
            Some(Terminator::BranchIf {
                condition: LocalId(0),
                then_block: BlockId(0),
                else_block: BlockId(0),
            }),
        );

        let diagnostics = BorrowChecker::new().check_function(&function);

        assert_eq!(diagnostics.len(), 1);
        assert_eq!(diagnostics[0].code, DiagnosticCode::UseAfterMove);
    }

    #[test]
    fn rejects_move_while_borrowed()
    {
        let function = function(
            vec![
                Statement::BorrowShared {
                    local: LocalId(0),
                    span: span(1, 2),
                },
                Statement::Move {
                    local: LocalId(0),
                    span: span(3, 4),
                },
            ],
            Some(Terminator::Return(None)),
        );

        let diagnostics = BorrowChecker::new().check_function(&function);

        assert_eq!(diagnostics.len(), 1);
        assert_eq!(diagnostics[0].code, DiagnosticCode::MoveWhileBorrowed);
    }

    #[test]
    fn rejects_mutable_borrow_conflict()
    {
        let function = function(
            vec![
                Statement::BorrowShared {
                    local: LocalId(0),
                    span: span(1, 2),
                },
                Statement::BorrowMutable {
                    local: LocalId(0),
                    span: span(3, 4),
                },
            ],
            Some(Terminator::Return(None)),
        );

        let diagnostics = BorrowChecker::new().check_function(&function);

        assert_eq!(diagnostics.len(), 1);
        assert_eq!(diagnostics[0].code, DiagnosticCode::MutableBorrowConflict);
    }

    #[test]
    fn rejects_mutable_borrow_of_immutable_local()
    {
        let function = function(
            vec![Statement::BorrowMutable {
                local: LocalId(1),
                span: span(1, 2),
            }],
            Some(Terminator::Return(None)),
        );

        let diagnostics = BorrowChecker::new().check_function(&function);

        assert_eq!(diagnostics.len(), 1);
        assert_eq!(diagnostics[0].code, DiagnosticCode::ImmutableLocalMutableBorrow);
    }

    #[test]
    fn treats_parameters_as_immutable_live_locals()
    {
        let function = Function {
            name: "main".to_string(),
            parameters: vec![Parameter {
                local: LocalId(0),
                name: "input".to_string(),
                value_type: Type::I64,
                span: span(0, 1),
            }],
            return_type: Type::VOID,
            locals: vec![],
            blocks: vec![BasicBlock {
                id: BlockId(0),
                statements: vec![Statement::BorrowMutable {
                    local: LocalId(0),
                    span: span(1, 2),
                }],
                terminator: Some(Terminator::Return(None)),
                span: span(0, 2),
            }],
        };

        let diagnostics = BorrowChecker::new().check_function(&function);

        assert_eq!(diagnostics[0].code, DiagnosticCode::ImmutableLocalMutableBorrow);
    }

    #[test]
    fn reports_missing_terminator()
    {
        let diagnostics = BorrowChecker::new().check_function(&function(vec![], None));

        assert_eq!(diagnostics.len(), 1);
        assert_eq!(diagnostics[0].code, DiagnosticCode::MissingTerminator);
    }

    #[test]
    fn computes_reachable_blocks_through_goto()
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

        let reachable = reachable_blocks(&function);

        assert!(reachable.contains(&BlockId(0)));
        assert!(reachable.contains(&BlockId(1)));
        assert!(!reachable.contains(&BlockId(2)));
    }

    #[test]
    fn computes_reachable_blocks_through_branch_if()
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
                    terminator: Some(Terminator::BranchIf {
                        condition: LocalId(0),
                        then_block: BlockId(1),
                        else_block: BlockId(2),
                    }),
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

        let reachable = reachable_blocks(&function);

        assert!(reachable.contains(&BlockId(0)));
        assert!(reachable.contains(&BlockId(1)));
        assert!(reachable.contains(&BlockId(2)));
    }
}
