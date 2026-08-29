/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

use crate::hir::Span;
use crate::hir::text::{function_to_xhir, parse_xhir_function};
use crate::hir::{BinaryOperator, Expression, Function, Literal, PrimitiveType, Statement, Type, TypeChecker};
use crate::mir;
use crate::xlil::{I32BinaryOperation, I64BinaryOperation, I64ComparisonOperation, Instruction};

use super::HirToMirLowerer;

fn long_binary(operator: BinaryOperator) -> Function
{
    let span = Span::new(1, 0, 6);
    Function {
        name: "operator_value".to_string(),
        return_type: Some(Type::Primitive(PrimitiveType::Long)),
        locals: vec![],
        body: vec![Statement::Return {
            value: Some(Expression::Binary {
                operator,
                left: Box::new(Expression::Literal {
                    literal: Literal::Integer("28".to_string()),
                    span,
                }),
                right: Box::new(Expression::Literal {
                    literal: Literal::Integer("2".to_string()),
                    span,
                }),
                span,
            }),
            span,
        }],
    }
}

fn int_binary(operator: BinaryOperator, return_type: PrimitiveType) -> Function
{
    let span = Span::new(1, 0, 6);
    Function {
        name: "wide_operator_value".to_string(),
        return_type: Some(Type::Primitive(return_type)),
        locals: vec![],
        body: vec![Statement::Return {
            value: Some(Expression::Binary {
                operator,
                left: Box::new(Expression::Literal {
                    literal: Literal::Integer("28".to_string()),
                    span,
                }),
                right: Box::new(Expression::Literal {
                    literal: Literal::Integer("2".to_string()),
                    span,
                }),
                span,
            }),
            span,
        }],
    }
}

#[test]
fn lowers_long_integer_operators_through_xhir_mir_and_xlil()
{
    let cases = [
        (BinaryOperator::Div, I32BinaryOperation::Div, "binary div"),
        (BinaryOperator::Rem, I32BinaryOperation::Rem, "binary rem"),
        (BinaryOperator::BitAnd, I32BinaryOperation::BitAnd, "binary bit_and"),
        (BinaryOperator::BitOr, I32BinaryOperation::BitOr, "binary bit_or"),
        (BinaryOperator::BitXor, I32BinaryOperation::BitXor, "binary bit_xor"),
        (
            BinaryOperator::ShiftLeft,
            I32BinaryOperation::ShiftLeft,
            "binary shift_left",
        ),
        (
            BinaryOperator::ShiftRight,
            I32BinaryOperation::ShiftRight,
            "binary shift_right",
        ),
    ];

    for (hir_operation, expected_operation, xhir_record) in cases
    {
        let function = long_binary(hir_operation);
        assert!(TypeChecker::new().check_function(&function).is_empty());

        let xhir = function_to_xhir(&function);
        assert!(xhir.contains(xhir_record));
        let parsed = parse_xhir_function(&xhir).expect("operator XHIR should round-trip");
        let mir = HirToMirLowerer::new()
            .lower_function(&parsed)
            .expect("Long operator should lower to MIR");
        assert!(crate::mir::verify::verify_function(&mir).is_empty());
        assert!(mir.blocks[0].statements.iter().any(|statement| {
            matches!(statement,
               mir::Statement::BinaryI32 { operation, .. } if *operation == expected_operation)
        }));

        let xlil = crate::xlil::lowering::MirToXlilLowerer::new()
            .lower_function(&mir)
            .expect("operator MIR should lower to XLIL");
        assert!(xlil.blocks[0].instructions.iter().any(|instruction| {
            matches!(instruction,
               Instruction::BinaryI32 { operation, .. } if *operation == expected_operation)
        }));
    }
}

#[test]
fn lowers_int_i64_operators_and_comparisons_through_mir_and_xlil()
{
    let binary_cases = [
        (BinaryOperator::Div, I64BinaryOperation::Div),
        (BinaryOperator::Rem, I64BinaryOperation::Rem),
        (BinaryOperator::BitAnd, I64BinaryOperation::BitAnd),
        (BinaryOperator::BitOr, I64BinaryOperation::BitOr),
        (BinaryOperator::BitXor, I64BinaryOperation::BitXor),
        (BinaryOperator::ShiftLeft, I64BinaryOperation::ShiftLeft),
        (BinaryOperator::ShiftRight, I64BinaryOperation::ShiftRight),
    ];
    for (hir_operation, expected_operation) in binary_cases
    {
        let function = int_binary(hir_operation, PrimitiveType::Int);
        assert!(TypeChecker::new().check_function(&function).is_empty());
        let mir = HirToMirLowerer::new()
            .lower_function(&function)
            .expect("Int operator should lower");
        assert!(crate::mir::verify::verify_function(&mir).is_empty());
        assert!(mir.blocks[0].statements.iter().any(|statement| {
            matches!(statement,
               mir::Statement::BinaryI64 { operation, .. } if *operation == expected_operation)
        }));
        let xlil = crate::xlil::lowering::MirToXlilLowerer::new()
            .lower_function(&mir)
            .expect("i64 MIR should lower");
        assert!(xlil.blocks[0].instructions.iter().any(|instruction| {
            matches!(instruction,
               Instruction::BinaryI64 { operation, .. } if *operation == expected_operation)
        }));
    }

    let comparison_cases = [
        (BinaryOperator::Less, I64ComparisonOperation::Less),
        (BinaryOperator::LessEqual, I64ComparisonOperation::LessEqual),
        (BinaryOperator::Greater, I64ComparisonOperation::Greater),
        (BinaryOperator::GreaterEqual, I64ComparisonOperation::GreaterEqual),
    ];
    for (hir_operation, expected_operation) in comparison_cases
    {
        let function = int_binary(hir_operation, PrimitiveType::Bool);
        let mir = HirToMirLowerer::new()
            .lower_function(&function)
            .expect("Int comparison should lower");
        assert!(mir.blocks[0].statements.iter().any(|statement| {
            matches!(statement,
               mir::Statement::CompareI64 { operation, .. } if *operation == expected_operation)
        }));
    }
}
