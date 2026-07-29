/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use crate::hir::async_check::Span;
use crate::mir::{self, BasicBlock, BlockId as MirBlockId, Function as MirFunction, Local, LocalId, Parameter};
use crate::xlil::{Instruction, Terminator, Type};

use super::{Diagnostic, DiagnosticCode, MirToXlilLowerer};

fn span(start: u32, end: u32) -> Span
{
  Span::new(1, start, end)
}

#[test]
fn lowers_void_return_function_skeleton()
{
  let function = MirFunction { name: "Main".to_string(),
                               parameters: vec![],
                               return_type: Type::VOID,
                               locals: vec![],
                               blocks: vec![BasicBlock { id: MirBlockId(0),
                                                         statements: vec![],
                                                         terminator: Some(mir::Terminator::Return(None)),
                                                         span: span(0, 1) }] };

  let lowered = MirToXlilLowerer::new().lower_function(&function)
                                       .expect("lowering should succeed");

  assert_eq!(lowered.name, "Main");
  assert_eq!(lowered.blocks.len(), 1);
  assert_eq!(lowered.blocks[0].terminator, Some(Terminator::Return(None)));
}

#[test]
fn lowers_panic_terminator()
{
  let function = MirFunction { name: "Panic".to_string(),
                               parameters: vec![],
                               return_type: Type::VOID,
                               locals: vec![],
                               blocks: vec![BasicBlock { id: MirBlockId(0),
                                                         statements: vec![],
                                                         terminator: Some(mir::Terminator::Panic),
                                                         span: span(0, 1) }] };

  let lowered = MirToXlilLowerer::new().lower_function(&function)
                                       .expect("lowering should succeed");

  assert_eq!(lowered.blocks[0].terminator, Some(Terminator::Panic));
}

#[test]
fn rejects_return_value_without_type_and_value_mapping()
{
  let function = MirFunction { name: "Value".to_string(),
                               parameters: vec![],
                               return_type: Type::I64,
                               locals: vec![],
                               blocks: vec![BasicBlock { id: MirBlockId(0),
                                                         statements: vec![],
                                                         terminator:
                                                           Some(mir::Terminator::Return(Some(LocalId(0)))),
                                                         span: span(0, 1) }] };

  let diagnostics = MirToXlilLowerer::new().lower_function(&function)
                                           .expect_err("lowering must diagnose");

  assert!(diagnostics.iter()
                     .any(|diagnostic| diagnostic.code == DiagnosticCode::MissingLocalType));
  assert!(diagnostics.iter()
                     .any(|diagnostic| diagnostic.code == DiagnosticCode::MissingLocalValue));
}

#[test]
fn lowers_i64_const_and_return_value()
{
  let function =
    MirFunction { name: "Value".to_string(),
                  parameters: vec![],
                  return_type: Type::I64,
                  locals: vec![Local { id: LocalId(0),
                                       name: "answer".to_string(),
                                       value_type: Some(Type::I64),
                                       mutable: false,
                                       span: span(0, 1) }],
                  blocks: vec![BasicBlock { id: MirBlockId(0),
                                            statements: vec![mir::Statement::ConstI64 { local: LocalId(0),
                                                                                        value: 42,
                                                                                        span: span(1, 2) }],
                                            terminator: Some(mir::Terminator::Return(Some(LocalId(0)))),
                                            span: span(0, 3) }] };

  let lowered = MirToXlilLowerer::new().lower_function(&function)
                                       .expect("lowering should succeed");

  assert_eq!(lowered.return_type, Type::I64);
  assert_eq!(lowered.blocks[0].instructions,
             vec![Instruction::ConstI64 { result: crate::xlil::ValueId(0),
                                          value: 42 }]);
  assert_eq!(lowered.blocks[0].terminator,
             Some(Terminator::Return(Some(crate::xlil::ValueId(0)))));
}

#[test]
fn lowers_i32_const_and_return_value()
{
  let function =
    MirFunction { name: "main".to_string(),
                  parameters: vec![],
                  return_type: Type::I32,
                  locals: vec![Local { id: LocalId(0),
                                       name: "exit_code".to_string(),
                                       value_type: Some(Type::I32),
                                       mutable: false,
                                       span: span(0, 1) }],
                  blocks: vec![BasicBlock { id: MirBlockId(0),
                                            statements: vec![mir::Statement::ConstI32 { local: LocalId(0),
                                                                                        value: 0,
                                                                                        span: span(1, 2) }],
                                            terminator: Some(mir::Terminator::Return(Some(LocalId(0)))),
                                            span: span(0, 3) }] };

  let lowered = MirToXlilLowerer::new().lower_function(&function)
                                       .expect("lowering should succeed");

  assert_eq!(lowered.blocks[0].instructions,
             vec![Instruction::ConstI32 { result: crate::xlil::ValueId(0),
                                          value: 0 }]);
}

#[test]
fn lowers_call_statement()
{
  let function = MirFunction { name: "CallValue".to_string(),
                               parameters: vec![],
                               return_type: Type::I64,
                               locals: vec![Local { id: LocalId(0),
                                                    name: "argument".to_string(),
                                                    value_type: Some(Type::I64),
                                                    mutable: false,
                                                    span: span(0, 1) },
                                            Local { id: LocalId(1),
                                                    name: "result".to_string(),
                                                    value_type: Some(Type::I64),
                                                    mutable: false,
                                                    span: span(0, 1) }],
                               blocks: vec![BasicBlock { id: MirBlockId(0),
                                                         statements:
                                                           vec![mir::Statement::ConstI64 { local: LocalId(0),
                                                                                           value: 7,
                                                                                           span: span(1, 2) },
                                                                mir::Statement::Call { result: Some(LocalId(1)),
                                                                                       function:
                                                                                         "xs$App$Callee".to_string(),
                                                                                       arguments: vec![LocalId(0)],
                                                                                       return_type: Type::I64,
                                                                                       span: span(2, 3) }],
                                                         terminator:
                                                           Some(mir::Terminator::Return(Some(LocalId(1)))),
                                                         span: span(0, 4) }] };

  let lowered = MirToXlilLowerer::new().lower_function(&function)
                                       .expect("lowering should succeed");

  assert_eq!(lowered.blocks[0].instructions.len(), 2);
  assert_eq!(lowered.blocks[0].terminator,
             Some(Terminator::Return(Some(crate::xlil::ValueId(1)))));
}

#[test]
fn lowers_add_i64_statement()
{
  let function =
    MirFunction { name: "Add".to_string(),
                  parameters: vec![],
                  return_type: Type::I64,
                  locals: vec![Local { id: LocalId(0),
                                       name: "left".to_string(),
                                       value_type: Some(Type::I64),
                                       mutable: false,
                                       span: span(0, 1) },
                               Local { id: LocalId(1),
                                       name: "right".to_string(),
                                       value_type: Some(Type::I64),
                                       mutable: false,
                                       span: span(0, 1) },
                               Local { id: LocalId(2),
                                       name: "sum".to_string(),
                                       value_type: Some(Type::I64),
                                       mutable: false,
                                       span: span(0, 1) }],
                  blocks: vec![BasicBlock { id: MirBlockId(0),
                                            statements: vec![mir::Statement::ConstI64 { local: LocalId(0),
                                                                                        value: 2,
                                                                                        span: span(1, 2) },
                                                             mir::Statement::ConstI64 { local: LocalId(1),
                                                                                        value: 3,
                                                                                        span: span(2, 3) },
                                                             mir::Statement::AddI64 { result: LocalId(2),
                                                                                      left: LocalId(0),
                                                                                      right: LocalId(1),
                                                                                      span: span(3, 4) }],
                                            terminator: Some(mir::Terminator::Return(Some(LocalId(2)))),
                                            span: span(0, 5) }] };

  let lowered = MirToXlilLowerer::new().lower_function(&function)
                                       .expect("lowering should succeed");

  assert_eq!(lowered.blocks[0].instructions[2], Instruction::AddI64 { result:
                                                                        crate::xlil::ValueId(2),
                                                                      left:
                                                                        crate::xlil::ValueId(0),
                                                                      right:
                                                                        crate::xlil::ValueId(1) });
  assert_eq!(lowered.blocks[0].terminator,
             Some(Terminator::Return(Some(crate::xlil::ValueId(2)))));
}

#[test]
fn lowers_sub_i64_statement()
{
  let function =
    MirFunction { name: "Sub".to_string(),
                  parameters: vec![],
                  return_type: Type::I64,
                  locals: vec![Local { id: LocalId(0),
                                       name: "left".to_string(),
                                       value_type: Some(Type::I64),
                                       mutable: false,
                                       span: span(0, 1) },
                               Local { id: LocalId(1),
                                       name: "right".to_string(),
                                       value_type: Some(Type::I64),
                                       mutable: false,
                                       span: span(0, 1) },
                               Local { id: LocalId(2),
                                       name: "difference".to_string(),
                                       value_type: Some(Type::I64),
                                       mutable: false,
                                       span: span(0, 1) }],
                  blocks: vec![BasicBlock { id: MirBlockId(0),
                                            statements: vec![mir::Statement::ConstI64 { local: LocalId(0),
                                                                                        value: 8,
                                                                                        span: span(1, 2) },
                                                             mir::Statement::ConstI64 { local: LocalId(1),
                                                                                        value: 3,
                                                                                        span: span(2, 3) },
                                                             mir::Statement::SubI64 { result: LocalId(2),
                                                                                      left: LocalId(0),
                                                                                      right: LocalId(1),
                                                                                      span: span(3, 4) }],
                                            terminator: Some(mir::Terminator::Return(Some(LocalId(2)))),
                                            span: span(0, 5) }] };

  let lowered = MirToXlilLowerer::new().lower_function(&function)
                                       .expect("lowering should succeed");

  assert_eq!(lowered.blocks[0].instructions[2], Instruction::SubI64 { result:
                                                                        crate::xlil::ValueId(2),
                                                                      left:
                                                                        crate::xlil::ValueId(0),
                                                                      right:
                                                                        crate::xlil::ValueId(1) });
  assert_eq!(lowered.blocks[0].terminator,
             Some(Terminator::Return(Some(crate::xlil::ValueId(2)))));
}

#[test]
fn lowers_mul_i64_statement()
{
  let function =
    MirFunction { name: "Mul".to_string(),
                  parameters: vec![],
                  return_type: Type::I64,
                  locals: vec![Local { id: LocalId(0),
                                       name: "left".to_string(),
                                       value_type: Some(Type::I64),
                                       mutable: false,
                                       span: span(0, 1) },
                               Local { id: LocalId(1),
                                       name: "right".to_string(),
                                       value_type: Some(Type::I64),
                                       mutable: false,
                                       span: span(0, 1) },
                               Local { id: LocalId(2),
                                       name: "product".to_string(),
                                       value_type: Some(Type::I64),
                                       mutable: false,
                                       span: span(0, 1) }],
                  blocks: vec![BasicBlock { id: MirBlockId(0),
                                            statements: vec![mir::Statement::ConstI64 { local: LocalId(0),
                                                                                        value: 6,
                                                                                        span: span(1, 2) },
                                                             mir::Statement::ConstI64 { local: LocalId(1),
                                                                                        value: 7,
                                                                                        span: span(2, 3) },
                                                             mir::Statement::MulI64 { result: LocalId(2),
                                                                                      left: LocalId(0),
                                                                                      right: LocalId(1),
                                                                                      span: span(3, 4) }],
                                            terminator: Some(mir::Terminator::Return(Some(LocalId(2)))),
                                            span: span(0, 5) }] };

  let lowered = MirToXlilLowerer::new().lower_function(&function)
                                       .expect("lowering should succeed");

  assert_eq!(lowered.blocks[0].instructions[2], Instruction::MulI64 { result:
                                                                        crate::xlil::ValueId(2),
                                                                      left:
                                                                        crate::xlil::ValueId(0),
                                                                      right:
                                                                        crate::xlil::ValueId(1) });
  assert_eq!(lowered.blocks[0].terminator,
             Some(Terminator::Return(Some(crate::xlil::ValueId(2)))));
}

#[test]
fn lowers_const_bool_and_eq_i64_statement()
{
  let function =
    MirFunction { name: "Eq".to_string(),
                  parameters: vec![],
                  return_type: Type::BOOL,
                  locals: vec![Local { id: LocalId(0),
                                       name: "left".to_string(),
                                       value_type: Some(Type::I64),
                                       mutable: false,
                                       span: span(0, 1) },
                               Local { id: LocalId(1),
                                       name: "right".to_string(),
                                       value_type: Some(Type::I64),
                                       mutable: false,
                                       span: span(0, 1) },
                               Local { id: LocalId(2),
                                       name: "same".to_string(),
                                       value_type: Some(Type::BOOL),
                                       mutable: false,
                                       span: span(0, 1) }],
                  blocks: vec![BasicBlock { id: MirBlockId(0),
                                            statements: vec![mir::Statement::ConstI64 { local: LocalId(0),
                                                                                        value: 7,
                                                                                        span: span(1, 2) },
                                                             mir::Statement::ConstI64 { local: LocalId(1),
                                                                                        value: 7,
                                                                                        span: span(2, 3) },
                                                             mir::Statement::EqI64 { result: LocalId(2),
                                                                                     left: LocalId(0),
                                                                                     right: LocalId(1),
                                                                                     span: span(3, 4) }],
                                            terminator: Some(mir::Terminator::Return(Some(LocalId(2)))),
                                            span: span(0, 5) }] };

  let lowered = MirToXlilLowerer::new().lower_function(&function)
                                       .expect("lowering should succeed");

  assert_eq!(lowered.blocks[0].instructions[2], Instruction::EqI64 { result:
                                                                       crate::xlil::ValueId(2),
                                                                     left:
                                                                       crate::xlil::ValueId(0),
                                                                     right:
                                                                       crate::xlil::ValueId(1) });
  assert_eq!(lowered.blocks[0].terminator,
             Some(Terminator::Return(Some(crate::xlil::ValueId(2)))));
}

#[test]
fn lowers_i32_instruction_family()
{
  let function =
    MirFunction { name: "I32Ops".to_string(),
                  parameters: vec![],
                  return_type: Type::VOID,
                  locals: vec![Local { id: LocalId(0),
                                       name: "left".to_string(),
                                       value_type: Some(Type::I32),
                                       mutable: false,
                                       span: span(0, 1) },
                               Local { id: LocalId(1),
                                       name: "right".to_string(),
                                       value_type: Some(Type::I32),
                                       mutable: false,
                                       span: span(0, 1) },
                               Local { id: LocalId(2),
                                       name: "value".to_string(),
                                       value_type: Some(Type::I32),
                                       mutable: false,
                                       span: span(0, 1) },
                               Local { id: LocalId(3),
                                       name: "flag".to_string(),
                                       value_type: Some(Type::BOOL),
                                       mutable: false,
                                       span: span(0, 1) }],
                  blocks: vec![BasicBlock { id: MirBlockId(0),
                                            statements: vec![mir::Statement::ConstI32 { local: LocalId(0),
                                                                                        value: 4,
                                                                                        span: span(1, 2) },
                                                             mir::Statement::ConstI32 { local: LocalId(1),
                                                                                        value: 2,
                                                                                        span: span(2, 3) },
                                                             mir::Statement::AddI32 { result: LocalId(2),
                                                                                      left: LocalId(0),
                                                                                      right: LocalId(1),
                                                                                      span: span(3, 4) },
                                                             mir::Statement::SubI32 { result: LocalId(2),
                                                                                      left: LocalId(0),
                                                                                      right: LocalId(1),
                                                                                      span: span(4, 5) },
                                                             mir::Statement::MulI32 { result: LocalId(2),
                                                                                      left: LocalId(0),
                                                                                      right: LocalId(1),
                                                                                      span: span(5, 6) },
                                                             mir::Statement::EqI32 { result: LocalId(3),
                                                                                     left: LocalId(0),
                                                                                     right: LocalId(1),
                                                                                     span: span(6, 7) },
                                                             mir::Statement::LtI32 { result: LocalId(3),
                                                                                     left: LocalId(0),
                                                                                     right: LocalId(1),
                                                                                     span: span(7, 8) },
                                                             mir::Statement::LeI32 { result: LocalId(3),
                                                                                     left: LocalId(0),
                                                                                     right: LocalId(1),
                                                                                     span: span(8, 9) },
                                                             mir::Statement::GtI32 { result: LocalId(3),
                                                                                     left: LocalId(0),
                                                                                     right: LocalId(1),
                                                                                     span: span(9, 10) },
                                                             mir::Statement::GeI32 { result: LocalId(3),
                                                                                     left: LocalId(0),
                                                                                     right: LocalId(1),
                                                                                     span: span(10, 11) }],
                                            terminator: Some(mir::Terminator::Return(None)),
                                            span: span(0, 12) }] };

  let lowered = MirToXlilLowerer::new().lower_function(&function)
                                       .expect("lowering should succeed");

  assert_eq!(lowered.blocks[0].instructions[2], Instruction::AddI32 { result:
                                                                        crate::xlil::ValueId(2),
                                                                      left:
                                                                        crate::xlil::ValueId(0),
                                                                      right:
                                                                        crate::xlil::ValueId(1) });
  assert!(matches!(lowered.blocks[0].instructions[3], Instruction::SubI32 { .. }));
  assert!(matches!(lowered.blocks[0].instructions[4], Instruction::MulI32 { .. }));
  assert!(matches!(lowered.blocks[0].instructions[5], Instruction::EqI32 { .. }));
  assert!(matches!(lowered.blocks[0].instructions[6], Instruction::LtI32 { .. }));
  assert!(matches!(lowered.blocks[0].instructions[7], Instruction::LeI32 { .. }));
  assert!(matches!(lowered.blocks[0].instructions[8], Instruction::GtI32 { .. }));
  assert!(matches!(lowered.blocks[0].instructions[9], Instruction::GeI32 { .. }));
}

#[test]
fn lowers_signature_parameters()
{
  let function = MirFunction { name: "WithParameter".to_string(),
                               parameters: vec![Parameter { local: LocalId(0),
                                                            name: "value".to_string(),
                                                            value_type: Type::I64,
                                                            span: span(0, 1) }],
                               return_type: Type::I64,
                               locals: vec![],
                               blocks: vec![BasicBlock { id: MirBlockId(0),
                                                         statements: vec![],
                                                         terminator:
                                                           Some(mir::Terminator::Return(Some(LocalId(0)))),
                                                         span: span(0, 1) }] };

  let lowered = MirToXlilLowerer::new().lower_function(&function)
                                       .expect("lowering should succeed");

  assert_eq!(lowered.parameters, vec![Type::I64]);
  assert_eq!(lowered.parameter_value(0), Some(crate::xlil::ValueId(0)));
  assert_eq!(lowered.blocks[0].terminator,
             Some(Terminator::Return(Some(crate::xlil::ValueId(0)))));
}

#[test]
fn lowers_goto_to_branch_terminator()
{
  let function = MirFunction { name: "Branch".to_string(),
                               parameters: vec![],
                               return_type: Type::VOID,
                               locals: vec![],
                               blocks: vec![BasicBlock { id: MirBlockId(0),
                                                         statements: vec![],
                                                         terminator: Some(mir::Terminator::Goto(MirBlockId(1))),
                                                         span: span(0, 1) },
                                            BasicBlock { id: MirBlockId(1),
                                                         statements: vec![],
                                                         terminator: Some(mir::Terminator::Return(None)),
                                                         span: span(1, 2) }] };

  let lowered = MirToXlilLowerer::new().lower_function(&function)
                                       .expect("lowering should succeed");

  assert_eq!(lowered.blocks[0].terminator,
             Some(Terminator::Branch(crate::xlil::BlockId(1))));
}

#[test]
fn rejects_branch_if_non_bool_condition()
{
  let function =
    MirFunction { name: "BadBranchIfType".to_string(),
                  parameters: vec![],
                  return_type: Type::VOID,
                  locals: vec![Local { id: LocalId(0),
                                       name: "condition".to_string(),
                                       value_type: Some(Type::I64),
                                       mutable: false,
                                       span: span(0, 1) }],
                  blocks: vec![BasicBlock { id: MirBlockId(0),
                                            statements: vec![mir::Statement::ConstI64 { local: LocalId(0),
                                                                                        value: 1,
                                                                                        span: span(1, 2) }],
                                            terminator: Some(mir::Terminator::BranchIf { condition:
                                                                                           LocalId(0),
                                                                                         then_block:
                                                                                           MirBlockId(1),
                                                                                         else_block:
                                                                                           MirBlockId(2) }),
                                            span: span(0, 3) },
                               BasicBlock { id: MirBlockId(1),
                                            statements: vec![],
                                            terminator: Some(mir::Terminator::Return(None)),
                                            span: span(3, 4) },
                               BasicBlock { id: MirBlockId(2),
                                            statements: vec![],
                                            terminator: Some(mir::Terminator::Return(None)),
                                            span: span(4, 5) }] };

  let diagnostics = MirToXlilLowerer::new().lower_function(&function)
                                           .expect_err("lowering must diagnose");

  assert_eq!(diagnostics, vec![Diagnostic { code:
                                              DiagnosticCode::UnsupportedLocalType,
                                            message: "MIR branch_if condition \
                                                      local must have XLIL bool \
                                                      type"
                                                           .to_string(),
                                            span: span(0, 3) }]);
}

#[test]
fn reports_branch_if_missing_condition_type_and_value()
{
  let function = MirFunction { name: "BadBranchIfValue".to_string(),
                               parameters: vec![],
                               return_type: Type::VOID,
                               locals: vec![Local { id: LocalId(0),
                                                    name: "condition".to_string(),
                                                    value_type: None,
                                                    mutable: false,
                                                    span: span(0, 1) }],
                               blocks: vec![BasicBlock { id: MirBlockId(0),
                                                         statements: vec![],
                                                         terminator:
                                                           Some(mir::Terminator::BranchIf { condition: LocalId(0),
                                                                                            then_block:
                                                                                              MirBlockId(1),
                                                                                            else_block:
                                                                                              MirBlockId(2) }),
                                                         span: span(0, 3) },
                                            BasicBlock { id: MirBlockId(1),
                                                         statements: vec![],
                                                         terminator: Some(mir::Terminator::Return(None)),
                                                         span: span(3, 4) },
                                            BasicBlock { id: MirBlockId(2),
                                                         statements: vec![],
                                                         terminator: Some(mir::Terminator::Return(None)),
                                                         span: span(4, 5) }] };

  let diagnostics = MirToXlilLowerer::new().lower_function(&function)
                                           .expect_err("lowering must diagnose");

  assert!(diagnostics.iter()
                     .any(|diagnostic| diagnostic.code == DiagnosticCode::MissingLocalType));
  assert!(diagnostics.iter()
                     .any(|diagnostic| diagnostic.code == DiagnosticCode::MissingLocalValue));
}

#[test]
fn reports_branch_if_missing_target()
{
  let function = MirFunction { name: "BadBranchIfTarget".to_string(),
                               parameters: vec![],
                               return_type: Type::VOID,
                               locals: vec![Local { id: LocalId(0),
                                                    name: "condition".to_string(),
                                                    value_type: Some(Type::BOOL),
                                                    mutable: false,
                                                    span: span(0, 1) }],
                               blocks: vec![BasicBlock { id: MirBlockId(0),
                                                         statements:
                                                           vec![mir::Statement::ConstBool { local: LocalId(0),
                                                                                            value: true,
                                                                                            span: span(1, 2) }],
                                                         terminator:
                                                           Some(mir::Terminator::BranchIf { condition: LocalId(0),
                                                                                            then_block:
                                                                                              MirBlockId(1),
                                                                                            else_block:
                                                                                              MirBlockId(2) }),
                                                         span: span(0, 3) }] };

  let diagnostics = MirToXlilLowerer::new().lower_function(&function)
                                           .expect_err("lowering must diagnose");

  assert_eq!(diagnostics[0].code, DiagnosticCode::MissingBranchTarget);
}

#[test]
fn lowers_branch_if_terminator()
{
  let function = MirFunction { name: "BranchIf".to_string(),
                               parameters: vec![],
                               return_type: Type::VOID,
                               locals: vec![Local { id: LocalId(0),
                                                    name: "condition".to_string(),
                                                    value_type: Some(Type::BOOL),
                                                    mutable: false,
                                                    span: span(0, 1) }],
                               blocks: vec![BasicBlock { id: MirBlockId(0),
                                                         statements:
                                                           vec![mir::Statement::ConstBool { local: LocalId(0),
                                                                                            value: true,
                                                                                            span: span(1, 2) }],
                                                         terminator:
                                                           Some(mir::Terminator::BranchIf { condition: LocalId(0),
                                                                                            then_block:
                                                                                              MirBlockId(1),
                                                                                            else_block:
                                                                                              MirBlockId(2) }),
                                                         span: span(0, 3) },
                                            BasicBlock { id: MirBlockId(1),
                                                         statements: vec![],
                                                         terminator: Some(mir::Terminator::Return(None)),
                                                         span: span(3, 4) },
                                            BasicBlock { id: MirBlockId(2),
                                                         statements: vec![],
                                                         terminator: Some(mir::Terminator::Return(None)),
                                                         span: span(4, 5) }] };

  let lowered = MirToXlilLowerer::new().lower_function(&function)
                                       .expect("lowering should succeed");

  assert_eq!(lowered.blocks[0].terminator,
             Some(Terminator::BranchIf { condition: crate::xlil::ValueId(0),
                                         then_block: crate::xlil::BlockId(1),
                                         else_block: crate::xlil::BlockId(2) }));
}

#[test]
fn lowers_mir_local_storage_to_xlil_stack_slot()
{
  let function =
    MirFunction { name: "Storage".to_string(),
                  parameters: vec![],
                  return_type: Type::I32,
                  locals: vec![Local { id: LocalId(0),
                                       name: "value".to_string(),
                                       value_type: Some(Type::I32),
                                       mutable: true,
                                       span: span(0, 1) },
                               Local { id: LocalId(1),
                                       name: "$tmp1".to_string(),
                                       value_type: Some(Type::I32),
                                       mutable: false,
                                       span: span(1, 2) },
                               Local { id: LocalId(2),
                                       name: "$tmp2".to_string(),
                                       value_type: Some(Type::I32),
                                       mutable: false,
                                       span: span(2, 3) }],
                  blocks: vec![BasicBlock { id: MirBlockId(0),
                                            statements: vec![mir::Statement::ConstI32 { local: LocalId(1),
                                                                                        value: 7,
                                                                                        span: span(1, 2) },
                                                             mir::Statement::StoreLocal { local: LocalId(0),
                                                                                          value: LocalId(1),
                                                                                          span: span(2, 3) },
                                                             mir::Statement::LoadLocal { result: LocalId(2),
                                                                                         local: LocalId(0),
                                                                                         span: span(3, 4) }],
                                            terminator: Some(mir::Terminator::Return(Some(LocalId(2)))),
                                            span: span(0, 4) }] };

  let lowered = MirToXlilLowerer::new().lower_function(&function)
                                       .expect("storage lowering should succeed");

  assert_eq!(lowered.slots.len(), 1);
  assert!(matches!(lowered.blocks[0].instructions[1], Instruction::Store { .. }));
  assert!(matches!(lowered.blocks[0].instructions[2], Instruction::Load { .. }));
  assert!(matches!(lowered.blocks[0].terminator, Some(Terminator::Return(Some(_)))));
}
