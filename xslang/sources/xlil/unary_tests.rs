/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use crate::hir::Span;
use crate::mir::{self, BasicBlock, BlockId, Local, LocalId};

use super::lowering::MirToXlilLowerer;
use super::parser::parse_module;
use super::verify::verify_module;
use super::writer::module_to_string;
use super::{Function, Instruction, Module, Terminator, Type};

fn span() -> Span
{
  Span::new(1, 0, 1)
}

#[test]
fn roundtrips_verified_not_bool_through_xmir_and_xlil()
{
  let function =
    mir::Function { name: "invert".to_string(),
                    parameters: vec![],
                    return_type: Type::BOOL,
                    locals: vec![Local { id: LocalId(0),
                                         name: "input".to_string(),
                                         value_type: Some(Type::BOOL),
                                         mutable: false,
                                         span: span() },
                                 Local { id: LocalId(1),
                                         name: "output".to_string(),
                                         value_type: Some(Type::BOOL),
                                         mutable: false,
                                         span: span() }],
                    blocks: vec![BasicBlock { id: BlockId(0),
                                              statements: vec![mir::Statement::ConstBool { local: LocalId(0),
                                                                                           value: false,
                                                                                           span: span() },
                                                               mir::Statement::NotBool { result: LocalId(1),
                                                                                         operand: LocalId(0),
                                                                                         span: span() }],
                                              terminator: Some(mir::Terminator::Return(Some(LocalId(1)))),
                                              span: span() }] };
  assert!(crate::mir::verify::verify_function(&function).is_empty());
  let xmir = crate::mir::text::function_to_xmir(&function);
  assert!(xmir.contains("statement not.bool"));
  let parsed_mir = crate::mir::text::parse_xmir_function(&xmir).expect("not.bool XMIR should parse");

  let lowered = MirToXlilLowerer::new().lower_function(&parsed_mir)
                                       .expect("not.bool should lower to XLIL");
  assert!(matches!(lowered.blocks[0].instructions[1], Instruction::NotBool { .. }));
  let mut module = Module::new("unary");
  module.add_function(lowered);
  assert!(verify_module(&module).is_empty());
  let text = module_to_string(&module);
  assert!(text.contains("not.bool"));
  assert_eq!(parse_module(&text).expect("not.bool XLIL should parse"), module);
}

#[test]
fn model_rejects_not_bool_on_i32()
{
  let mut function = Function::definition("invalid", Type::BOOL, vec![]);
  let block = function.append_block("entry");
  let integer = function.add_const_i32(block, 1).expect("i32 constant should exist");

  assert_eq!(function.not_bool(block, integer), None);
  assert_eq!(function.blocks[0].terminator, None::<Terminator>);
}
