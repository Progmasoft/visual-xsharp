/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::{function_to_xmir, parse_xmir_function};
use crate::hir::Span;
use crate::mir::{BasicBlock, BlockId, Function, Local, LocalId, Statement, Terminator};
use crate::xlil::Type;

fn span() -> Span
{
  Span::new(0, 0, 0)
}

#[test]
fn roundtrips_local_storage_statements()
{
  let local = |id, name: &str| Local { id: LocalId(id),
                                       name: name.to_string(),
                                       value_type: Some(Type::I32),
                                       mutable: id == 0,
                                       span: span() };
  let function =
    Function { name: "storage".to_string(),
               parameters: Vec::new(),
               return_type: Type::I32,
               locals: vec![local(0, "value"), local(1, "$tmp1"), local(2, "$tmp2")],
               blocks: vec![BasicBlock { id: BlockId(0),
                                         statements: vec![Statement::ConstI32 { local: LocalId(1),
                                                                                value: 7,
                                                                                span: span() },
                                                          Statement::StoreLocal { local: LocalId(0),
                                                                                  value: LocalId(1),
                                                                                  span: span() },
                                                          Statement::LoadLocal { result: LocalId(2),
                                                                                 local: LocalId(0),
                                                                                 span: span() }],
                                         terminator: Some(Terminator::Return(Some(LocalId(2)))),
                                         span: span() }] };

  let text = function_to_xmir(&function);
  let parsed = parse_xmir_function(&text).expect("storage XMIR should parse");

  assert!(text.contains("statement store.local"));
  assert!(text.contains("statement load.local"));
  assert_eq!(parsed, function);
}
