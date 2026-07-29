/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::*;
use crate::hir::Local;
use crate::hir::text::{function_to_xhir, parse_xhir_function};
use crate::mir::verify::verify_function;

fn span() -> Span
{
  Span::new(1, 0, 1)
}

fn function(operator: BinaryOperator, left: bool) -> Function
{
  Function { name: "choose".to_string(),
             return_type: Some(Type::Primitive(PrimitiveType::Bool)),
             locals: vec![],
             body: vec![Statement::Return {
      value: Some(Expression::Binary {
        operator,
        left: Box::new(Expression::Literal { literal: Literal::Bool(left),
                                             span: span() }),
        right: Box::new(Expression::Call { function: "right".to_string(),
                                           arguments: vec![],
                                           parameter_types: vec![],
                                           return_type: Box::new(Type::Primitive(PrimitiveType::Bool)),
                                           span: span() }),
        span: span(),
      }),
      span: span(),
    }] }
}

fn logical(operator: BinaryOperator, left: bool, right: bool) -> Expression
{
  Expression::Binary { operator,
                       left: Box::new(Expression::Literal { literal: Literal::Bool(left),
                                                            span: span() }),
                       right: Box::new(Expression::Literal { literal: Literal::Bool(right),
                                                             span: span() }),
                       span: span() }
}

fn assert_short_circuit(operator: BinaryOperator, left: bool, short_block: mir::BlockId)
{
  let function = function(operator, left);
  let xhir = function_to_xhir(&function);
  let parsed = parse_xhir_function(&xhir).expect("short-circuit XHIR should parse");
  let mir = HirToMirLowerer::new().lower_function(&parsed)
                                  .expect("short-circuit expression should lower");

  assert!(verify_function(&mir).is_empty());
  assert!(crate::mir::BorrowChecker::new().check_function(&mir).is_empty());
  assert!(matches!(mir.blocks[0].terminator,
                   Some(mir::Terminator::BranchIf { then_block, else_block, .. })
                     if then_block == if operator == BinaryOperator::LogicalAnd { mir::BlockId(1) } else { short_block }
                       && else_block == if operator == BinaryOperator::LogicalAnd { short_block } else { mir::BlockId(1) }));
  assert!(mir.blocks[1].statements
                       .iter()
                       .any(|statement| matches!(statement, mir::Statement::Call { function, .. } if function == "right")));
  assert!(mir.blocks[short_block.0 as usize].statements
                                           .iter()
                                           .any(|statement| matches!(statement,
                                                                     mir::Statement::ConstBool { value, .. }
                                                                       if *value == (operator == BinaryOperator::LogicalOr))));
  let xlil = crate::xlil::lowering::MirToXlilLowerer::new().lower_function(&mir)
                                                           .expect("short-circuit MIR should lower to XLIL");
  assert!(matches!(xlil.blocks[0].terminator,
                   Some(crate::xlil::Terminator::BranchIf { .. })));
  crate::mir::optimizer::optimize_verified_function(mir).expect("short-circuit MIR should optimize");
}

#[test]
fn lowers_logical_and_with_false_short_path()
{
  assert_short_circuit(BinaryOperator::LogicalAnd, false, mir::BlockId(2));
}

#[test]
fn lowers_logical_or_with_true_short_path()
{
  assert_short_circuit(BinaryOperator::LogicalOr, true, mir::BlockId(2));
}

#[test]
fn lowers_sequential_short_circuit_initializers()
{
  let bool_type = Type::Primitive(PrimitiveType::Bool);
  let left = Local { name: "left".to_string(),
                     ty: bool_type.clone(),
                     mutable: false,
                     span: span() };
  let right = Local { name: "right".to_string(),
                      ty: bool_type,
                      mutable: false,
                      span: span() };
  let function = Function { name: "sequential".to_string(),
                            return_type: Some(Type::Primitive(PrimitiveType::Long)),
                            locals: vec![left.clone(), right.clone()],
                            body: vec![Statement::Let { local: left,
                                           initializer: Some(logical(BinaryOperator::LogicalAnd, false, true)) },
                          Statement::Let { local: right,
                                           initializer: Some(logical(BinaryOperator::LogicalOr, true, false)) },
                          Statement::Return { value: Some(Expression::Literal { literal:
                                                                                  Literal::Integer("1".to_string()),
                                                                                span: span() }),
                                              span: span() }] };
  let mir = HirToMirLowerer::new().lower_function(&function)
                                  .expect("sequential expressions should lower");
  let diagnostics = verify_function(&mir);
  assert!(diagnostics.is_empty(), "{diagnostics:?}");
  let borrow = crate::mir::BorrowChecker::new().check_function(&mir);
  assert!(borrow.is_empty(), "{borrow:?}");
  let optimized =
    crate::mir::optimizer::optimize_verified_function(mir).expect("sequential short-circuit MIR should optimize");
  crate::xlil::lowering::MirToXlilLowerer::new().lower_function(&optimized.function)
                                                .expect("optimized sequential MIR should lower");
}
