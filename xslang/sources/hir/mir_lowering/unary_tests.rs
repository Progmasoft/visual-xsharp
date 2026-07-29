/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use crate::hir::text::{function_to_xhir, parse_xhir_function};
use crate::hir::{Expression, Function, Literal, PrimitiveType, Span, Statement, Type, TypeChecker, UnaryOperator};
use crate::mir;
use crate::xlil::Instruction;

use super::HirToMirLowerer;

fn unary_function(operator: UnaryOperator, return_type: PrimitiveType, literal: Literal) -> Function
{
  let span = Span::new(1, 0, 4);
  Function { name: "unary_value".to_string(),
             return_type: Some(Type::Primitive(return_type)),
             locals: vec![],
             body: vec![Statement::Return { value: Some(Expression::Unary { operator,
                                                                            operand:
                                                                              Box::new(Expression::Literal { literal,
                                                                                                             span }),
                                                                            span }),
                                            span }] }
}

#[test]
fn lowers_long_positive_and_negative_through_xhir_and_mir()
{
  for (operator, record) in [(UnaryOperator::Positive, "unary positive"),
                             (UnaryOperator::Negative, "unary negative")]
  {
    let function = unary_function(operator, PrimitiveType::Long, Literal::Integer("7".to_string()));
    assert!(TypeChecker::new().check_function(&function).is_empty());
    let xhir = function_to_xhir(&function);
    assert!(xhir.contains(record));
    let parsed = parse_xhir_function(&xhir).expect("unary XHIR should parse");
    let mir = HirToMirLowerer::new().lower_function(&parsed)
                                    .expect("unary Long should lower");
    assert!(crate::mir::verify::verify_function(&mir).is_empty());
    let expected = if operator == UnaryOperator::Negative
    {
      -7
    }
    else
    {
      7
    };
    assert!(mir.blocks[0]
      .statements
      .iter()
      .any(|statement| matches!(statement, mir::Statement::ConstI32 { value, .. } if *value == expected)));
  }
}

#[test]
fn lowers_logical_not_through_xhir_mir_and_xlil()
{
  let function = unary_function(UnaryOperator::LogicalNot, PrimitiveType::Bool, Literal::Bool(false));
  assert!(TypeChecker::new().check_function(&function).is_empty());
  let xhir = function_to_xhir(&function);
  assert!(xhir.contains("unary logical_not"));
  let parsed = parse_xhir_function(&xhir).expect("logical-not XHIR should parse");
  let mir = HirToMirLowerer::new().lower_function(&parsed)
                                  .expect("logical not should lower");
  assert!(crate::mir::verify::verify_function(&mir).is_empty());
  assert!(mir.blocks[0].statements
                       .iter()
                       .any(|statement| matches!(statement, mir::Statement::NotBool { .. })));

  let xlil = crate::xlil::lowering::MirToXlilLowerer::new().lower_function(&mir)
                                                           .expect("logical-not MIR should lower");
  assert!(xlil.blocks[0].instructions
                        .iter()
                        .any(|instruction| matches!(instruction, Instruction::NotBool { .. })));
}

#[test]
fn rejects_unary_operator_type_mismatches()
{
  let numeric_not = unary_function(UnaryOperator::LogicalNot,
                                   PrimitiveType::Long,
                                   Literal::Integer("7".to_string()));
  let bool_negative = unary_function(UnaryOperator::Negative, PrimitiveType::Bool, Literal::Bool(true));

  assert!(!TypeChecker::new().check_function(&numeric_not).is_empty());
  assert!(!TypeChecker::new().check_function(&bool_negative).is_empty());
}
