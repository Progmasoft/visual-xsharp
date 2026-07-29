/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use crate::hir::text::{function_to_xhir, parse_xhir_function};
use crate::hir::{
  BinaryOperator, Expression, Function, Literal, Local, PrimitiveType, Span, Statement, Type, TypeChecker,
};
use crate::mir;
use crate::xlil::{Instruction, IntegerBinaryOperation, Type as XlilType};

use super::HirToMirLowerer;

fn binary_function(primitive: PrimitiveType, operator: BinaryOperator, comparison: bool) -> Function
{
  let span = Span::new(1, 0, 8);
  let ty = Type::Primitive(primitive);
  let local = |name: &str| Local { name: name.to_string(),
                                   ty: ty.clone(),
                                   mutable: false,
                                   span };
  Function { name: "integer_operator".to_string(),
             return_type: Some(Type::Primitive(if comparison
                                               {
                                                 PrimitiveType::Bool
                                               }
                                               else
                                               {
                                                 primitive
                                               })),
             locals: vec![],
             body: vec![Statement::Let { local: local("left"),
                                         initializer: Some(Expression::Literal {
                                           literal: Literal::Integer("8".to_string()),
                                           span,
                                         }) },
                        Statement::Let { local: local("right"),
                                         initializer: Some(Expression::Literal {
                                           literal: Literal::Integer("2".to_string()),
                                           span,
                                         }) },
                        Statement::Return { value: Some(Expression::Binary {
                                              operator,
                                              left: Box::new(Expression::Local { name: "left".to_string(),
                                                                                span }),
                                              right: Box::new(Expression::Local { name: "right".to_string(),
                                                                                 span }),
                                              span,
                                            }),
                                            span }] }
}

#[test]
fn lowers_operations_for_every_non_legacy_integer_width()
{
  let cases = [(PrimitiveType::Byte, XlilType::U8),
               (PrimitiveType::SByte, XlilType::I8),
               (PrimitiveType::UShort, XlilType::U16),
               (PrimitiveType::Short, XlilType::I16),
               (PrimitiveType::ULong, XlilType::U32),
               (PrimitiveType::UInt, XlilType::U64),
               (PrimitiveType::UInteger, XlilType::U128),
               (PrimitiveType::Integer, XlilType::I128)];
  for (primitive, value_type) in cases
  {
    let function = binary_function(primitive, BinaryOperator::Add, false);
    assert!(TypeChecker::new().check_function(&function).is_empty());
    let parsed = parse_xhir_function(&function_to_xhir(&function)).expect("integer operator XHIR should round-trip");
    let mir = HirToMirLowerer::new().lower_function(&parsed)
                                    .expect("integer operator should lower to MIR");
    let mir =
      crate::mir::text::parse_xmir_function(&crate::mir::text::function_to_xmir(&mir)).expect("integer operator XMIR \
                                                                                               should round-trip");
    assert!(crate::mir::verify::verify_function(&mir).is_empty());
    assert!(mir.blocks.iter().flat_map(|block| &block.statements).any(|statement| {
      matches!(statement, mir::Statement::BinaryInteger { operation: IntegerBinaryOperation::Add, value_type: actual, .. } if *actual == value_type)
    }));
    let xlil = crate::xlil::lowering::MirToXlilLowerer::new().lower_function(&mir)
                                                             .expect("integer MIR should lower to XLIL");
    let module = crate::xlil::Module { name: "test".to_string(),
                                       aggregate_types: vec![],
                                       array_types: vec![],
                                       functions: vec![xlil.clone()] };
    assert!(crate::xlil::verify_module(&module).is_empty());
    let reparsed = crate::xlil::parse_module(&crate::xlil::module_to_string(&module)).expect("integer operator XLIL \
                                                                                              should round-trip");
    assert!(crate::xlil::verify_module(&reparsed).is_empty());
    assert!(xlil.blocks.iter().flat_map(|block| &block.instructions).any(|instruction| {
      matches!(instruction, Instruction::BinaryInteger { operation: IntegerBinaryOperation::Add, value_type: actual, .. } if *actual == value_type)
    }));
  }
}

#[test]
fn preserves_signed_and_unsigned_integer_operation_types()
{
  for (primitive, value_type) in [(PrimitiveType::UInteger, XlilType::U128),
                                  (PrimitiveType::Integer, XlilType::I128)]
  {
    for (operator, operation, comparison) in [(BinaryOperator::Div, IntegerBinaryOperation::Div, false),
                                              (BinaryOperator::ShiftRight, IntegerBinaryOperation::ShiftRight, false),
                                              (BinaryOperator::Less, IntegerBinaryOperation::Less, true)]
    {
      let function = binary_function(primitive, operator, comparison);
      assert!(TypeChecker::new().check_function(&function).is_empty());
      let mir = HirToMirLowerer::new().lower_function(&function)
                                      .expect("wide integer operation should lower");
      assert!(mir.blocks.iter().flat_map(|block| &block.statements).any(|statement| {
        matches!(statement, mir::Statement::BinaryInteger { operation: actual_operation, value_type: actual_type, .. } if *actual_operation == operation && *actual_type == value_type)
      }));
    }
  }
}
