/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

use super::*;
use crate::hir::type_check::Local;
use crate::mir::verify::verify_function;

fn span() -> Span
{
    Span::new(1, 0, 1)
}

fn literal(value: &str) -> Expression
{
    Expression::Literal {
        literal: Literal::Integer(value.to_string()),
        span: span(),
    }
}

fn value_block(value: &str) -> Block
{
    Block {
        statements: Vec::new(),
        tail: Some(Box::new(literal(value))),
        span: span(),
    }
}

#[test]
fn lowers_returned_if_expression_to_branching_mir_and_xlil()
{
    let long = Type::Primitive(PrimitiveType::Long);
    let function = Function {
        name: "choose".to_string(),
        return_type: Some(long.clone()),
        locals: Vec::new(),
        body: vec![Statement::Return {
            value: Some(Expression::If {
                condition: Box::new(Expression::Literal {
                    literal: Literal::Bool(true),
                    span: span(),
                }),
                then_block: Box::new(value_block("7")),
                else_block: Box::new(value_block("9")),
                result_type: Box::new(long),
                span: span(),
            }),
            span: span(),
        }],
    };

    let mir = HirToMirLowerer::new()
        .lower_function(&function)
        .expect("if expression should lower");
    assert_eq!(mir.blocks.len(), 3);
    assert!(matches!(
        mir.blocks[0].terminator,
        Some(mir::Terminator::BranchIf { .. })
    ));
    assert!(matches!(
        mir.blocks[1].terminator,
        Some(mir::Terminator::Return(Some(_)))
    ));
    assert!(matches!(
        mir.blocks[2].terminator,
        Some(mir::Terminator::Return(Some(_)))
    ));
    assert!(verify_function(&mir).is_empty());

    let xlil = crate::xlil::lowering::MirToXlilLowerer::new()
        .lower_function(&mir)
        .expect("branching MIR should lower to XLIL");
    assert!(matches!(
        xlil.blocks[0].terminator,
        Some(crate::xlil::Terminator::BranchIf { .. })
    ));
    assert!(matches!(
        xlil.blocks[1].terminator,
        Some(crate::xlil::Terminator::Return(Some(_)))
    ));
    assert!(matches!(
        xlil.blocks[2].terminator,
        Some(crate::xlil::Terminator::Return(Some(_)))
    ));
}

#[test]
fn lowers_if_expression_initializer_through_merge_storage()
{
    let long = Type::Primitive(PrimitiveType::Long);
    let result = Local {
        name: "result".to_string(),
        ty: long.clone(),
        mutable: false,
        span: span(),
    };
    let expression = Expression::If {
        condition: Box::new(Expression::Literal {
            literal: Literal::Bool(true),
            span: span(),
        }),
        then_block: Box::new(value_block("7")),
        else_block: Box::new(value_block("9")),
        result_type: Box::new(long.clone()),
        span: span(),
    };
    let function = Function {
        name: "choose_local".to_string(),
        return_type: Some(long),
        locals: Vec::new(),
        body: vec![
            Statement::Let {
                local: result,
                initializer: Some(expression),
            },
            Statement::Return {
                value: Some(Expression::Local {
                    name: "result".to_string(),
                    span: span(),
                }),
                span: span(),
            },
        ],
    };

    let mir = HirToMirLowerer::new()
        .lower_function(&function)
        .expect("if expression initializer should lower");
    assert!(verify_function(&mir).is_empty());
    assert!(
        mir.blocks
            .iter()
            .flat_map(|block| &block.statements)
            .filter(|statement| matches!(statement, mir::Statement::StoreLocal { .. }))
            .count() >=
            3
    );

    let xlil = crate::xlil::lowering::MirToXlilLowerer::new()
        .lower_function(&mir)
        .expect("if initializer MIR should lower to XLIL");
    assert!(
        xlil.blocks
            .iter()
            .flat_map(|block| &block.instructions)
            .any(|instruction| matches!(instruction, crate::xlil::Instruction::Load { .. }))
    );
}

fn local(name: &str) -> Local
{
    Local {
        name: name.to_string(),
        ty: Type::Primitive(PrimitiveType::Long),
        mutable: false,
        span: span(),
    }
}

#[test]
fn rejects_duplicate_local_in_one_lexical_scope()
{
    let function = Function {
        name: "main".to_string(),
        return_type: None,
        locals: Vec::new(),
        body: vec![
            Statement::Let {
                local: local("value"),
                initializer: Some(literal("1")),
            },
            Statement::Let {
                local: local("value"),
                initializer: Some(literal("2")),
            },
        ],
    };

    let diagnostics = crate::hir::type_check::TypeChecker::new().check_function(&function);

    assert_eq!(diagnostics.len(), 1);
    assert_eq!(
        diagnostics[0].code,
        crate::hir::type_check::DiagnosticCode::DuplicateLocal
    );
}

#[test]
fn block_local_can_shadow_without_replacing_outer_mir_binding()
{
    let function = Function {
        name: "main".to_string(),
        return_type: Some(Type::Primitive(PrimitiveType::Long)),
        locals: Vec::new(),
        body: vec![
            Statement::Let {
                local: local("value"),
                initializer: Some(literal("0")),
            },
            Statement::If {
                condition: Expression::Literal {
                    literal: Literal::Bool(true),
                    span: span(),
                },
                then_block: Block {
                    statements: vec![Statement::Let {
                        local: local("value"),
                        initializer: Some(literal("7")),
                    }],
                    tail: None,
                    span: span(),
                },
                else_block: None,
                span: span(),
            },
            Statement::Return {
                value: Some(Expression::Local {
                    name: "value".to_string(),
                    span: span(),
                }),
                span: span(),
            },
        ],
    };

    let mir = HirToMirLowerer::new()
        .lower_function(&function)
        .expect("shadowing should lower");

    let bindings = mir
        .locals
        .iter()
        .filter(|value| value.name == "value")
        .collect::<Vec<_>>();
    assert_eq!(bindings.len(), 2);
    let outer = bindings[0].id;
    assert!(mir.blocks.iter().any(|block| {
        block.statements.iter().any(|statement| {
            matches!(statement,
                                                            mir::Statement::LoadLocal { local, .. } if *local == outer)
        })
    }));
    assert!(verify_function(&mir).is_empty());
}

#[test]
fn lowers_while_break_to_verified_mir_and_xlil_control_flow()
{
    let function = Function {
        name: "loop_once".to_string(),
        return_type: Some(Type::Primitive(PrimitiveType::Long)),
        locals: Vec::new(),
        body: vec![
            Statement::While {
                condition: Expression::Literal {
                    literal: Literal::Bool(true),
                    span: span(),
                },
                body: Block {
                    statements: vec![Statement::Break {
                        span: span(),
                    }],
                    tail: None,
                    span: span(),
                },
                span: span(),
            },
            Statement::Return {
                value: Some(literal("7")),
                span: span(),
            },
        ],
    };

    assert!(
        crate::hir::type_check::TypeChecker::new()
            .check_function(&function)
            .is_empty()
    );
    let mir = HirToMirLowerer::new()
        .lower_function(&function)
        .expect("while should lower");
    assert_eq!(mir.blocks.len(), 4);
    assert!(matches!(mir.blocks[0].terminator, Some(mir::Terminator::Goto(_))));
    assert!(matches!(
        mir.blocks[1].terminator,
        Some(mir::Terminator::BranchIf { .. })
    ));
    assert!(matches!(mir.blocks[2].terminator, Some(mir::Terminator::Goto(_))));
    assert!(matches!(
        mir.blocks[3].terminator,
        Some(mir::Terminator::Return(Some(_)))
    ));
    assert!(verify_function(&mir).is_empty());

    let xlil = crate::xlil::lowering::MirToXlilLowerer::new()
        .lower_function(&mir)
        .expect("loop MIR should lower to XLIL");
    assert!(matches!(
        xlil.blocks[1].terminator,
        Some(crate::xlil::Terminator::BranchIf { .. })
    ));
}

#[test]
fn rejects_loop_jumps_outside_loop()
{
    let function = Function {
        name: "invalid".to_string(),
        return_type: None,
        locals: Vec::new(),
        body: vec![
            Statement::Break {
                span: span(),
            },
            Statement::Continue {
                span: span(),
            },
        ],
    };
    let diagnostics = crate::hir::type_check::TypeChecker::new().check_function(&function);
    assert_eq!(diagnostics.len(), 2);
    assert_eq!(
        diagnostics[0].code,
        crate::hir::type_check::DiagnosticCode::BreakOutsideLoop
    );
    assert_eq!(
        diagnostics[1].code,
        crate::hir::type_check::DiagnosticCode::ContinueOutsideLoop
    );
}
