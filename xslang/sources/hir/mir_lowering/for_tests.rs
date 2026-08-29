/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

use super::*;
use crate::hir::collection_registry::{ArrayLayout, CollectionRegistry};
use crate::hir::type_check::Local;
use crate::mir::verify::verify_function;

fn span() -> Span
{
    Span::new(1, 0, 1)
}

fn local(name: &str) -> Local
{
    Local {
        name: name.to_string(),
        ty: Type::Primitive(PrimitiveType::Long),
        mutable: true,
        span: span(),
    }
}

fn local_expression(name: &str) -> Expression
{
    Expression::Local {
        name: name.to_string(),
        span: span(),
    }
}

fn integer(value: &str) -> Expression
{
    Expression::Literal {
        literal: Literal::Integer(value.to_string()),
        span: span(),
    }
}

#[test]
fn lowers_classic_for_with_continue_through_verified_mir_and_xlil()
{
    let increment = Expression::Assign {
        target: "i".to_string(),
        value: Box::new(Expression::Binary {
            operator: BinaryOperator::Add,
            left: Box::new(local_expression("i")),
            right: Box::new(integer("1")),
            span: span(),
        }),
        span: span(),
    };
    let function = Function {
        name: "count".to_string(),
        return_type: Some(Type::Primitive(PrimitiveType::Long)),
        locals: Vec::new(),
        body: vec![
            Statement::For {
                initializer: Some(Box::new(Statement::Let {
                    local: local("i"),
                    initializer: Some(integer("0")),
                })),
                condition: Some(Expression::Binary {
                    operator: BinaryOperator::Less,
                    left: Box::new(local_expression("i")),
                    right: Box::new(integer("3")),
                    span: span(),
                }),
                update: Some(increment),
                body: Block {
                    statements: vec![Statement::Continue {
                        span: span(),
                    }],
                    tail: None,
                    span: span(),
                },
                span: span(),
            },
            Statement::Return {
                value: Some(integer("7")),
                span: span(),
            },
        ],
    };

    let diagnostics = crate::hir::TypeChecker::new().check_function(&function);
    assert!(diagnostics.is_empty(), "{diagnostics:#?}");
    let mir = HirToMirLowerer::new()
        .lower_function(&function)
        .expect("classic for should lower");
    assert_eq!(mir.blocks.len(), 5);
    assert!(matches!(
        mir.blocks[1].terminator,
        Some(mir::Terminator::BranchIf { .. })
    ));
    assert!(matches!(mir.blocks[2].terminator,
                   Some(mir::Terminator::Goto(target)) if target == mir.blocks[3].id));
    assert!(matches!(mir.blocks[3].terminator,
                   Some(mir::Terminator::Goto(target)) if target == mir.blocks[1].id));
    assert!(matches!(
        mir.blocks[4].terminator,
        Some(mir::Terminator::Return(Some(_)))
    ));
    assert!(verify_function(&mir).is_empty());

    let xlil = crate::xlil::lowering::MirToXlilLowerer::new()
        .lower_function(&mir)
        .expect("for MIR should lower to XLIL");
    assert!(matches!(
        xlil.blocks[1].terminator,
        Some(crate::xlil::Terminator::BranchIf { .. })
    ));
    assert!(matches!(xlil.blocks[2].terminator,
                   Some(crate::xlil::Terminator::Branch(target)) if target == xlil.blocks[3].id));
    assert!(matches!(xlil.blocks[3].terminator,
                   Some(crate::xlil::Terminator::Branch(target)) if target == xlil.blocks[1].id));
}

#[test]
fn lowers_fixed_array_for_each_to_checked_index_cfg()
{
    let array_type = Type::Array {
        element: Box::new(Type::Primitive(PrimitiveType::Long)),
        length: Some(3),
    };
    let registry = CollectionRegistry {
        arrays: vec![ArrayLayout {
            source_type: array_type.clone(),
            value_type: XlilType::array(0),
            element_type: XlilType::I32,
            length: Some(3),
        }],
    };
    let values = Local {
        name: "values".to_string(),
        ty: array_type.clone(),
        mutable: false,
        span: span(),
    };
    let total = Local {
        name: "total".to_string(),
        ty: Type::Primitive(PrimitiveType::Long),
        mutable: true,
        span: span(),
    };
    let function = Function {
        name: "sum".to_string(),
        return_type: Some(Type::Primitive(PrimitiveType::Long)),
        locals: Vec::new(),
        body: vec![
            Statement::Let {
                local: values,
                initializer: Some(Expression::Array {
                    elements: vec![integer("1"), integer("2"), integer("3")],
                    span: span(),
                }),
            },
            Statement::Let {
                local: total,
                initializer: Some(integer("0")),
            },
            Statement::ForEach {
                binding: Local {
                    name: "value".to_string(),
                    ty: Type::Primitive(PrimitiveType::Long),
                    mutable: false,
                    span: span(),
                },
                iterable: local_expression("values"),
                iterable_type: array_type,
                body: Block {
                    statements: vec![Statement::Expr(Expression::Assign {
                        target: "total".to_string(),
                        value: Box::new(Expression::Binary {
                            operator: BinaryOperator::Add,
                            left: Box::new(local_expression("total")),
                            right: Box::new(local_expression("value")),
                            span: span(),
                        }),
                        span: span(),
                    })],
                    tail: None,
                    span: span(),
                },
                span: span(),
            },
            Statement::Return {
                value: Some(local_expression("total")),
                span: span(),
            },
        ],
    };

    let diagnostics = crate::hir::TypeChecker::new().check_function(&function);
    assert!(diagnostics.is_empty(), "{diagnostics:#?}");
    let mir = HirToMirLowerer::new()
        .with_collection_types(&registry)
        .lower_function(&function)
        .expect("fixed-array for-each should lower");
    assert!(verify_function(&mir).is_empty());
    assert!(
        mir.blocks
            .iter()
            .flat_map(|block| &block.statements)
            .any(|statement| { matches!(statement, mir::Statement::ArrayGet { .. }) })
    );
    assert!(
        mir.blocks
            .iter()
            .any(|block| matches!(block.terminator, Some(mir::Terminator::BranchIf { .. })))
    );

    let xlil = crate::xlil::lowering::MirToXlilLowerer::new()
        .lower_function(&mir)
        .expect("for-each MIR should lower to XLIL");
    assert!(
        xlil.blocks
            .iter()
            .flat_map(|block| &block.instructions)
            .any(|instruction| { matches!(instruction, crate::xlil::Instruction::ArrayGet { .. }) })
    );
}
