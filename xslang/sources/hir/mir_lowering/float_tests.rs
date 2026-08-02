/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use crate::hir::{Expression, Function, Literal, PrimitiveType, Span, Statement, Type};
use crate::mir::verify::verify_function;
use crate::mir::{self};
use crate::xlil::{Instruction, Type as XlilType};

use super::HirToMirLowerer;

#[test]
fn lowers_lfloat_and_float_literals_through_mir_and_xlil()
{
    for (primitive, expected) in [
        (PrimitiveType::LFloat, XlilType::F32),
        (PrimitiveType::Float, XlilType::F64),
    ]
    {
        let span = Span::new(1, 10, 13);
        let function = Function {
            name: "floating_value".to_string(),
            return_type: Some(Type::Primitive(primitive)),
            locals: vec![],
            body: vec![Statement::Return {
                value: Some(Expression::Literal {
                    literal: Literal::Float("1.5".to_string()),
                    span,
                }),
                span,
            }],
        };
        let mir = HirToMirLowerer::new()
            .lower_function(&function)
            .expect("floating literal should lower to MIR");
        assert_eq!(mir.return_type, expected);
        assert!(matches!(
            mir.blocks[0].statements[0],
            mir::Statement::ConstF32 {
                bits: 0x3fc00000,
                ..
            } | mir::Statement::ConstF64 {
                bits: 0x3ff8000000000000,
                ..
            }
        ));
        assert!(verify_function(&mir).is_empty());
        let xlil = crate::xlil::lowering::MirToXlilLowerer::new()
            .lower_function(&mir)
            .expect("floating MIR should lower to XLIL");
        assert_eq!(xlil.return_type, expected);
        assert!(matches!(
            xlil.blocks[0].instructions[0],
            Instruction::ConstF32 { .. } | Instruction::ConstF64 { .. }
        ));
    }
}

#[test]
fn lowers_float_arithmetic_expression_to_typed_mir()
{
    let span = Span::new(1, 0, 7);
    let expression = Expression::Binary {
        operator: crate::hir::BinaryOperator::Add,
        left: Box::new(Expression::Literal {
            literal: Literal::Float("1.5".to_string()),
            span,
        }),
        right: Box::new(Expression::Literal {
            literal: Literal::Float("2.5".to_string()),
            span,
        }),
        span,
    };
    let function = Function {
        name: "add".to_string(),
        return_type: Some(Type::Primitive(PrimitiveType::Float)),
        locals: vec![],
        body: vec![Statement::Return {
            value: Some(expression),
            span,
        }],
    };
    let lowered = HirToMirLowerer::new()
        .lower_function(&function)
        .expect("Float addition should lower");
    assert!(matches!(lowered.blocks[0].statements[2], mir::Statement::BinaryFloat {
        operation: crate::xlil::FloatBinaryOperation::Add,
        value_type: XlilType::F64,
        ..
    }));
}
