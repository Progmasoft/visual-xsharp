/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::MirToXlilLowerer;
use crate::hir::Span;
use crate::mir::{BasicBlock, BlockId, Function, Local, LocalId, Statement, Terminator};
use crate::xlil::{Instruction, Type};

fn span() -> Span
{
  Span::new(0, 0, 0)
}

#[test]
fn lowers_mir_aggregate_and_extract_to_xlil()
{
  let typed_local = |id, value_type| Local { id: LocalId(id),
                                             name: format!("$tmp{id}"),
                                             value_type: Some(value_type),
                                             mutable: false,
                                             span: span() };
  let aggregate_type = Type::aggregate(0);
  let function =
    Function { name: "Aggregate".to_string(),
               parameters: Vec::new(),
               return_type: Type::I32,
               locals: vec![typed_local(0, Type::I32),
                            typed_local(1, Type::I32),
                            typed_local(2, aggregate_type),
                            typed_local(3, Type::I32)],
               blocks: vec![BasicBlock { id: BlockId(0),
                                         statements: vec![Statement::ConstI32 { local: LocalId(0),
                                                                                value: 7,
                                                                                span: span() },
                                                          Statement::ConstI32 { local: LocalId(1),
                                                                                value: 9,
                                                                                span: span() },
                                                          Statement::Aggregate { result: LocalId(2),
                                                                                 value_type: aggregate_type,
                                                                                 fields: vec![LocalId(0),
                                                                                              LocalId(1)],
                                                                                 field_types:
                                                                                   vec![Type::I32, Type::I32],
                                                                                 span: span() },
                                                          Statement::Extract { result: LocalId(3),
                                                                               aggregate: LocalId(2),
                                                                               field: 0,
                                                                               field_type: Type::I32,
                                                                               span: span() }],
                                         terminator: Some(Terminator::Return(Some(LocalId(3)))),
                                         span: span() }] };

  let lowered = MirToXlilLowerer::new().lower_function(&function)
                                       .expect("aggregate lowering should succeed");

  assert!(matches!(lowered.blocks[0].instructions[2], Instruction::Aggregate { .. }));
  assert!(matches!(lowered.blocks[0].instructions[3], Instruction::Extract { field: 0,
                                                                             .. }));
}
