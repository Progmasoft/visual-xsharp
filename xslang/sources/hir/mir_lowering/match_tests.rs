/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::*;
use crate::hir::{MatchArm, MatchPattern};
use crate::mir::verify::verify_function;

fn span() -> Span
{
    Span::new(1, 0, 1)
}

fn integer(value: &str) -> Literal
{
    Literal::Integer(value.to_string())
}

fn returning(pattern: MatchPattern, value: &str) -> MatchArm
{
    MatchArm {
        pattern,
        body: Block {
            statements: vec![Statement::Return {
                value: Some(Expression::Literal {
                    literal: integer(value),
                    span: span(),
                }),
                span: span(),
            }],
            tail: None,
            span: span(),
        },
        span: span(),
    }
}

fn yielding(pattern: MatchPattern, value: &str) -> MatchArm
{
    MatchArm {
        pattern,
        body: Block {
            statements: Vec::new(),
            tail: Some(Box::new(Expression::Literal {
                literal: integer(value),
                span: span(),
            })),
            span: span(),
        },
        span: span(),
    }
}

#[test]
fn lowers_match_expression_through_result_storage_to_xlil()
{
    let expression = Expression::Match {
        selector: Box::new(Expression::Literal {
            literal: integer("2"),
            span: span(),
        }),
        selector_type: Box::new(Type::Primitive(PrimitiveType::Long)),
        arms: vec![
            yielding(MatchPattern::Literal(integer("2")), "7"),
            yielding(MatchPattern::Else, "9"),
        ],
        result_type: Box::new(Type::Primitive(PrimitiveType::Long)),
        span: span(),
    };
    let function = Function {
        name: "choose_value".to_string(),
        return_type: Some(Type::Primitive(PrimitiveType::Long)),
        locals: Vec::new(),
        body: vec![Statement::Return {
            value: Some(expression),
            span: span(),
        }],
    };

    assert!(crate::hir::TypeChecker::new().check_function(&function).is_empty());
    let mir = HirToMirLowerer::new()
        .lower_function(&function)
        .expect("match expression should lower");
    assert!(verify_function(&mir).is_empty());
    assert!(
        mir.blocks
            .iter()
            .flat_map(|block| &block.statements)
            .any(|statement| { matches!(statement, mir::Statement::StoreLocal { .. }) })
    );

    let xlil = crate::xlil::lowering::MirToXlilLowerer::new()
        .lower_function(&mir)
        .expect("match expression MIR should lower to XLIL");
    assert!(
        xlil.blocks
            .iter()
            .flat_map(|block| &block.instructions)
            .any(|instruction| matches!(instruction, crate::xlil::Instruction::Store { .. }))
    );
}

#[test]
fn lowers_long_match_to_verified_mir_and_xlil_branches()
{
    let function = Function {
        name: "choose".to_string(),
        return_type: Some(Type::Primitive(PrimitiveType::Long)),
        locals: Vec::new(),
        body: vec![Statement::Match {
            selector: Expression::Literal {
                literal: integer("2"),
                span: span(),
            },
            selector_type: Type::Primitive(PrimitiveType::Long),
            arms: vec![
                returning(MatchPattern::Literal(integer("0")), "1"),
                returning(MatchPattern::Literal(integer("2")), "7"),
                returning(MatchPattern::Else, "3"),
            ],
            span: span(),
        }],
    };

    assert!(crate::hir::TypeChecker::new().check_function(&function).is_empty());
    let mir = HirToMirLowerer::new()
        .lower_function(&function)
        .expect("Long match should lower");
    assert!(verify_function(&mir).is_empty());
    assert_eq!(
        mir.blocks
            .iter()
            .filter(|block| matches!(block.terminator, Some(mir::Terminator::BranchIf { .. })))
            .count(),
        2
    );

    let xlil = crate::xlil::lowering::MirToXlilLowerer::new()
        .lower_function(&mir)
        .expect("match MIR should lower to XLIL");
    assert_eq!(
        xlil.blocks
            .iter()
            .filter(|block| matches!(block.terminator, Some(crate::xlil::Terminator::BranchIf { .. })))
            .count(),
        2
    );
}

#[test]
fn lowers_false_bool_pattern_by_reversing_branch_targets()
{
    let function = Function {
        name: "choose".to_string(),
        return_type: Some(Type::Primitive(PrimitiveType::Long)),
        locals: Vec::new(),
        body: vec![Statement::Match {
            selector: Expression::Literal {
                literal: Literal::Bool(false),
                span: span(),
            },
            selector_type: Type::Primitive(PrimitiveType::Bool),
            arms: vec![
                returning(MatchPattern::Literal(Literal::Bool(false)), "7"),
                returning(MatchPattern::Else, "3"),
            ],
            span: span(),
        }],
    };

    let mir = HirToMirLowerer::new()
        .lower_function(&function)
        .expect("Bool match should lower");
    let Some(mir::Terminator::BranchIf {
        then_block,
        else_block,
        ..
    }) = mir.blocks[0].terminator
    else
    {
        panic!("expected Bool match branch");
    };
    assert_ne!(then_block, else_block);
    assert_eq!(else_block, mir::BlockId(2));
    assert!(verify_function(&mir).is_empty());
}
