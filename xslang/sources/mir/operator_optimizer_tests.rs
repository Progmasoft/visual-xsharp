/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use crate::hir::Span;
use crate::xlil::{I32BinaryOperation, I64BinaryOperation, I64ComparisonOperation, Type};

use super::optimizer::{OptimizationPass, optimize_verified_function};
use super::{BasicBlock, BlockId, Function, Local, LocalId, Statement, Terminator};

fn span() -> Span
{
    Span::new(1, 0, 1)
}

fn function(operation: I32BinaryOperation, left: i32, right: i32) -> Function
{
    Function {
        name: "fold_i32".to_string(),
        parameters: vec![],
        return_type: Type::I32,
        locals: (0..3)
            .map(|id| Local {
                id: LocalId(id),
                name: format!("value_{id}"),
                value_type: Some(Type::I32),
                mutable: false,
                span: span(),
            })
            .collect(),
        blocks: vec![BasicBlock {
            id: BlockId(0),
            statements: vec![
                Statement::ConstI32 {
                    local: LocalId(0),
                    value: left,
                    span: span(),
                },
                Statement::ConstI32 {
                    local: LocalId(1),
                    value: right,
                    span: span(),
                },
                Statement::BinaryI32 {
                    operation,
                    result: LocalId(2),
                    left: LocalId(0),
                    right: LocalId(1),
                    span: span(),
                },
            ],
            terminator: Some(Terminator::Return(Some(LocalId(2)))),
            span: span(),
        }],
    }
}

#[test]
fn folds_safe_i32_binary_operations()
{
    let cases = [
        (I32BinaryOperation::Div, 28, 4, 7),
        (I32BinaryOperation::Rem, 15, 8, 7),
        (I32BinaryOperation::BitAnd, 15, 7, 7),
        (I32BinaryOperation::BitOr, 6, 1, 7),
        (I32BinaryOperation::BitXor, 3, 4, 7),
        (I32BinaryOperation::ShiftLeft, 7, 1, 14),
        (I32BinaryOperation::ShiftRight, -14, 1, -7),
    ];
    for (operation, left, right, expected) in cases
    {
        let optimized = optimize_verified_function(function(operation, left, right))
            .expect("valid operator function should optimize");
        assert!(
            optimized
                .reports
                .iter()
                .any(|report| report.pass == OptimizationPass::FoldConstI32Binary)
        );
        assert!(matches!(optimized.function.blocks[0].statements[2],
                     Statement::ConstI32 { value, .. } if value == expected));
    }
}

#[test]
fn preserves_trapping_or_invalid_i32_operations()
{
    let cases = [
        (I32BinaryOperation::Div, 7, 0),
        (I32BinaryOperation::Rem, 7, 0),
        (I32BinaryOperation::Div, i32::MIN, -1),
        (I32BinaryOperation::ShiftLeft, 1, 32),
        (I32BinaryOperation::ShiftRight, 1, -1),
    ];
    for (operation, left, right) in cases
    {
        let optimized = optimize_verified_function(function(operation, left, right))
            .expect("unfolded operator function should remain valid");
        assert!(matches!(
            optimized.function.blocks[0].statements[2],
            Statement::BinaryI32 { .. }
        ));
    }
}

#[test]
fn folds_safe_i64_binary_and_comparison_operations()
{
    let function = Function {
        name: "fold_i64".to_string(),
        parameters: vec![],
        return_type: Type::BOOL,
        locals: vec![
            Local {
                id: LocalId(0),
                name: "left".to_string(),
                value_type: Some(Type::I64),
                mutable: false,
                span: span(),
            },
            Local {
                id: LocalId(1),
                name: "right".to_string(),
                value_type: Some(Type::I64),
                mutable: false,
                span: span(),
            },
            Local {
                id: LocalId(2),
                name: "quotient".to_string(),
                value_type: Some(Type::I64),
                mutable: false,
                span: span(),
            },
            Local {
                id: LocalId(3),
                name: "greater".to_string(),
                value_type: Some(Type::BOOL),
                mutable: false,
                span: span(),
            },
        ],
        blocks: vec![BasicBlock {
            id: BlockId(0),
            statements: vec![
                Statement::ConstI64 {
                    local: LocalId(0),
                    value: 28,
                    span: span(),
                },
                Statement::ConstI64 {
                    local: LocalId(1),
                    value: 4,
                    span: span(),
                },
                Statement::BinaryI64 {
                    operation: I64BinaryOperation::Div,
                    result: LocalId(2),
                    left: LocalId(0),
                    right: LocalId(1),
                    span: span(),
                },
                Statement::CompareI64 {
                    operation: I64ComparisonOperation::Greater,
                    result: LocalId(3),
                    left: LocalId(2),
                    right: LocalId(1),
                    span: span(),
                },
            ],
            terminator: Some(Terminator::Return(Some(LocalId(3)))),
            span: span(),
        }],
    };
    let optimized = optimize_verified_function(function).expect("i64 operations should optimize");
    assert!(
        optimized
            .reports
            .iter()
            .any(|report| report.pass == OptimizationPass::FoldConstI64Binary)
    );
    assert!(
        optimized
            .reports
            .iter()
            .any(|report| report.pass == OptimizationPass::FoldConstI64Comparison)
    );
    assert!(matches!(
        optimized.function.blocks[0].statements[2],
        Statement::ConstI64 {
            value: 7,
            ..
        }
    ));
    assert!(matches!(
        optimized.function.blocks[0].statements[3],
        Statement::ConstBool {
            value: true,
            ..
        }
    ));
}

#[test]
fn folds_constant_logical_not()
{
    let function = Function {
        name: "fold_not".to_string(),
        parameters: vec![],
        return_type: Type::BOOL,
        locals: (0..2)
            .map(|id| Local {
                id: LocalId(id),
                name: format!("bool_{id}"),
                value_type: Some(Type::BOOL),
                mutable: false,
                span: span(),
            })
            .collect(),
        blocks: vec![BasicBlock {
            id: BlockId(0),
            statements: vec![
                Statement::ConstBool {
                    local: LocalId(0),
                    value: false,
                    span: span(),
                },
                Statement::NotBool {
                    result: LocalId(1),
                    operand: LocalId(0),
                    span: span(),
                },
            ],
            terminator: Some(Terminator::Return(Some(LocalId(1)))),
            span: span(),
        }],
    };
    let optimized = optimize_verified_function(function).expect("logical not should optimize");

    assert!(
        optimized
            .reports
            .iter()
            .any(|report| report.pass == OptimizationPass::FoldConstBoolNot)
    );
    assert!(matches!(
        optimized.function.blocks[0].statements[1],
        Statement::ConstBool {
            value: true,
            ..
        }
    ));
}
