/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::*;
use crate::mir::verify::verify_function;

fn span(start: u32, end: u32) -> Span
{
  Span::new(1, start, end)
}

fn primitive(primitive: PrimitiveType) -> Type
{
  Type::Primitive(primitive)
}

fn local(name: &str, ty: Type, mutable: bool) -> super::super::type_check::Local
{
  super::super::type_check::Local { name: name.to_string(),
                                    ty,
                                    mutable,
                                    span: span(0, 1) }
}

#[test]
fn lowers_void_function_to_single_return_block()
{
  let function = Function { name: "Main".to_string(),
                            return_type: None,
                            locals: vec![],
                            body: vec![] };

  let mir = HirToMirLowerer::new().lower_function(&function)
                                  .expect("void function should lower");

  assert_eq!(mir.return_type, XlilType::VOID);
  assert_eq!(mir.blocks[0].terminator, Some(mir::Terminator::Return(None)));
  assert!(verify_function(&mir).is_empty());
}

#[test]
fn lowers_panic_statement_to_mir_and_xlil()
{
  let function = Function { name: "Stop".to_string(),
                            return_type: None,
                            locals: vec![],
                            body: vec![Statement::Panic { span: span(4, 12) }] };

  let mir = HirToMirLowerer::new().lower_function(&function)
                                  .expect("panic function should lower to MIR");
  let xlil = crate::xlil::lowering::MirToXlilLowerer::new().lower_function(&mir)
                                                           .expect("panic MIR should lower to XLIL");

  assert_eq!(mir.blocks[0].terminator, Some(mir::Terminator::Panic));
  assert_eq!(xlil.blocks[0].terminator, Some(crate::xlil::Terminator::Panic));
  assert!(verify_function(&mir).is_empty());
}

#[test]
fn lowers_int_literal_return_to_const_i64()
{
  let function = Function { name: "Answer".to_string(),
                            return_type: Some(primitive(PrimitiveType::Int)),
                            locals: vec![],
                            body: vec![Statement::Return { value: Some(Expression::Literal { literal:
                                                                                  Literal::Integer("42".to_string()),
                                                                                span: span(10, 12) }),
                                                          span: span(3, 12) }] };

  let mir = HirToMirLowerer::new().lower_function(&function)
                                  .expect("Int literal return should lower");

  assert_eq!(mir.return_type, XlilType::I64);
  assert_eq!(mir.locals.len(), 1);
  assert!(matches!(mir.blocks[0].statements[0], mir::Statement::ConstI64 { value: 42,
                                                                           .. }));
  assert_eq!(mir.blocks[0].terminator,
             Some(mir::Terminator::Return(Some(mir::LocalId(0)))));
  assert!(verify_function(&mir).is_empty());
}

#[test]
fn lowers_long_literal_to_const_i32_and_xlil()
{
  let function = Function { name: "LongAnswer".to_string(),
                            return_type: Some(primitive(PrimitiveType::Long)),
                            locals: vec![],
                            body: vec![Statement::Return { value:
                                            Some(Expression::Literal { literal:
                                                                         Literal::Integer("2'000'000'000".to_string()),
                                                                       span: span(10, 23) }),
                                          span: span(3, 23) }] };

  let mir = HirToMirLowerer::new().lower_function(&function)
                                  .expect("Long literal return should lower");

  assert_eq!(mir.return_type, XlilType::I32);
  assert!(matches!(mir.blocks[0].statements[0], mir::Statement::ConstI32 { value:
                                                                             2_000_000_000,
                                                                           .. }));
  assert!(verify_function(&mir).is_empty());

  let xlil = crate::xlil::lowering::MirToXlilLowerer::new().lower_function(&mir)
                                                           .expect("Long MIR should lower to XLIL");
  assert!(matches!(xlil.blocks[0].instructions[0],
                   crate::xlil::Instruction::ConstI32 { value: 2_000_000_000,
                                                        .. }));
}

#[test]
fn rejects_long_literal_outside_i32_range()
{
  let function = Function { name: "TooLarge".to_string(),
                            return_type: Some(primitive(PrimitiveType::Long)),
                            locals: vec![],
                            body: vec![Statement::Return { value:
                                            Some(Expression::Literal { literal:
                                                                         Literal::Integer("2'147'483'648".to_string()),
                                                                       span: span(10, 23) }),
                                          span: span(3, 23) }] };

  let diagnostics = HirToMirLowerer::new().lower_function(&function)
                                          .expect_err("out-of-range Long literal must fail");

  assert!(diagnostics.iter()
                     .any(|diagnostic| diagnostic.code == DiagnosticCode::InvalidIntegerLiteral &&
                                       diagnostic.message.contains("const.i32")));
}

#[test]
fn lowers_int_local_initializer_and_return()
{
  let function =
    Function { name: "Answer".to_string(),
               return_type: Some(primitive(PrimitiveType::Int)),
               locals: vec![],
               body: vec![Statement::Let { local: local("answer", primitive(PrimitiveType::Int), false),
                                           initializer:
                                             Some(Expression::Literal { literal:
                                                                          Literal::Integer("1'024".to_string()),
                                                                        span: span(10, 15) }) },
                          Statement::Return { value: Some(Expression::Local { name: "answer".to_string(),
                                                                              span: span(20, 26) }),
                                              span: span(13, 26) },] };

  let mir = HirToMirLowerer::new().lower_function(&function)
                                  .expect("local Int return should lower");

  assert_eq!(mir.locals[0].name, "answer");
  assert!(mir.blocks[0].statements
                       .iter()
                       .any(|statement| matches!(statement, mir::Statement::ConstI64 { value: 1024,
                                                                                       .. })));
  assert!(mir.blocks[0].statements
                       .iter()
                       .any(|statement| matches!(statement, mir::Statement::StoreLocal { local: mir::LocalId(0),
                                                                                         .. })));
  assert!(mir.blocks[0].statements
                       .iter()
                       .any(|statement| matches!(statement, mir::Statement::LoadLocal { local: mir::LocalId(0),
                                                                                        .. })));
  assert!(verify_function(&mir).is_empty());
}

#[test]
fn lowers_long_binary_add_to_mir_and_xlil_i32()
{
  let function = Function { name: "AddLong".to_string(),
                            return_type: Some(primitive(PrimitiveType::Long)),
                            locals: vec![],
                            body: vec![Statement::Let { local: local("left", primitive(PrimitiveType::Long), false),
                                        initializer: Some(Expression::Literal { literal:
                                                                                  Literal::Integer("4".to_string()),
                                                                                span: span(1, 2) }) },
                      Statement::Let { local: local("right", primitive(PrimitiveType::Long), false),
                                       initializer: Some(Expression::Literal { literal:
                                                                                 Literal::Integer("2".to_string()),
                                                                               span: span(3, 4) }) },
                      Statement::Return { value:
                                            Some(Expression::Binary {
                                              operator: BinaryOperator::Add,
                                              left: Box::new(Expression::Local { name: "left".to_string(),
                                                                                span: span(5, 9) }),
                                              right: Box::new(Expression::Local { name: "right".to_string(),
                                                                                 span: span(12, 17) }),
                                              span: span(5, 17),
                                            }),
                                          span: span(0, 17) }] };

  let mir = HirToMirLowerer::new().lower_function(&function)
                                  .expect("Long add should lower");

  assert_eq!(mir.blocks[0].statements
                          .iter()
                          .filter(|statement| matches!(statement, mir::Statement::StoreLocal { .. }))
                          .count(),
             2);
  assert_eq!(mir.blocks[0].statements
                          .iter()
                          .filter(|statement| matches!(statement, mir::Statement::LoadLocal { .. }))
                          .count(),
             2);
  assert!(mir.blocks[0].statements
                       .iter()
                       .any(|statement| matches!(statement, mir::Statement::AddI32 { .. })));
  assert!(verify_function(&mir).is_empty());

  let xlil = crate::xlil::lowering::MirToXlilLowerer::new().lower_function(&mir)
                                                           .expect("Long add MIR should lower to XLIL");
  assert_eq!(xlil.slots.len(), 2);
  assert!(xlil.blocks[0].instructions
                        .iter()
                        .any(|instruction| matches!(instruction, crate::xlil::Instruction::AddI32 { .. })));
}

#[test]
fn lowers_long_comparison_to_mir_and_xlil_i32()
{
  let function = Function { name: "CompareLong".to_string(),
                            return_type: Some(primitive(PrimitiveType::Bool)),
                            locals: vec![],
                            body: vec![Statement::Let { local: local("left", primitive(PrimitiveType::Long), false),
                                        initializer: Some(Expression::Literal { literal:
                                                                                  Literal::Integer("4".to_string()),
                                                                                span: span(1, 2) }) },
                      Statement::Let { local: local("right", primitive(PrimitiveType::Long), false),
                                       initializer: Some(Expression::Literal { literal:
                                                                                 Literal::Integer("7".to_string()),
                                                                               span: span(3, 4) }) },
                      Statement::Return { value:
                                            Some(Expression::Binary {
                                              operator: BinaryOperator::Less,
                                              left: Box::new(Expression::Local { name: "left".to_string(),
                                                                                span: span(5, 9) }),
                                              right: Box::new(Expression::Local { name: "right".to_string(),
                                                                                 span: span(12, 17) }),
                                              span: span(5, 17),
                                            }),
                                          span: span(0, 17) }] };

  let mir = HirToMirLowerer::new().lower_function(&function)
                                  .expect("Long comparison should lower");

  assert!(mir.blocks[0].statements
                       .iter()
                       .any(|statement| matches!(statement, mir::Statement::LtI32 { .. })));
  assert!(verify_function(&mir).is_empty());

  let xlil = crate::xlil::lowering::MirToXlilLowerer::new().lower_function(&mir)
                                                           .expect("Long comparison MIR should lower to XLIL");
  assert_eq!(xlil.slots.len(), 2);
  assert!(xlil.blocks[0].instructions
                        .iter()
                        .any(|instruction| matches!(instruction, crate::xlil::Instruction::LtI32 { .. })));
}
