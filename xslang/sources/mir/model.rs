/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use std::collections::HashMap;

use crate::hir::async_check::Span;
use crate::xlil::{
  FloatBinaryOperation, FloatComparisonOperation, I32BinaryOperation, I64BinaryOperation, I64ComparisonOperation, Type,
};

mod control_flow;
mod diagnostic;
mod integer;

pub use control_flow::reachable_blocks;
pub use diagnostic::{Diagnostic, DiagnosticCode};
pub use integer::IntegerConstant;

#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
pub struct LocalId(pub u32);

#[derive(Clone, Copy, Debug, Default, Eq, Hash, PartialEq)]
pub struct BlockId(pub u32);

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct Local
{
  pub id: LocalId,
  pub name: String,
  pub value_type: Option<Type>,
  pub mutable: bool,
  pub span: Span,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct Parameter
{
  pub local: LocalId,
  pub name: String,
  pub value_type: Type,
  pub span: Span,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum Statement
{
  Use
  {
    local: LocalId, span: Span
  },
  Move
  {
    local: LocalId, span: Span
  },
  BorrowShared
  {
    local: LocalId, span: Span
  },
  BorrowMutable
  {
    local: LocalId, span: Span
  },
  EndBorrow
  {
    local: LocalId, span: Span
  },
  ConstI64
  {
    local: LocalId, value: i64, span: Span
  },
  ConstI32
  {
    local: LocalId, value: i32, span: Span
  },
  ConstU16
  {
    local: LocalId, value: u16, span: Span
  },
  ConstInteger
  {
    local: LocalId,
    value: IntegerConstant,
    span: Span,
  },
  ConstF32
  {
    local: LocalId, bits: u32, span: Span
  },
  ConstF64
  {
    local: LocalId, bits: u64, span: Span
  },
  ConstStr
  {
    local: LocalId,
    units: Vec<u32>,
    span: Span,
  },
  ConstBool
  {
    local: LocalId, value: bool, span: Span
  },
  BinaryInteger
  {
    operation: crate::xlil::IntegerBinaryOperation,
    value_type: Type,
    result: LocalId,
    left: LocalId,
    right: LocalId,
    span: Span,
  },
  BinaryFloat
  {
    operation: FloatBinaryOperation,
    value_type: Type,
    result: LocalId,
    left: LocalId,
    right: LocalId,
    span: Span,
  },
  CompareFloat
  {
    operation: FloatComparisonOperation,
    value_type: Type,
    result: LocalId,
    left: LocalId,
    right: LocalId,
    span: Span,
  },
  CompareStr
  {
    operation: crate::xlil::StrComparisonOperation,
    result: LocalId,
    left: LocalId,
    right: LocalId,
    span: Span,
  },
  StoreLocal
  {
    local: LocalId, value: LocalId, span: Span
  },
  LoadLocal
  {
    result: LocalId,
    local: LocalId,
    span: Span,
  },
  Aggregate
  {
    result: LocalId,
    value_type: Type,
    fields: Vec<LocalId>,
    field_types: Vec<Type>,
    span: Span,
  },
  Extract
  {
    result: LocalId,
    aggregate: LocalId,
    field: u32,
    field_type: Type,
    span: Span,
  },
  ArrayGet
  {
    result: LocalId,
    array: LocalId,
    index: LocalId,
    array_type: Type,
    element_type: Type,
    span: Span,
  },
  ArraySet
  {
    result: LocalId,
    array: LocalId,
    index: LocalId,
    value: LocalId,
    array_type: Type,
    element_type: Type,
    span: Span,
  },
  ArrayLength
  {
    result: LocalId,
    array: LocalId,
    array_type: Type,
    span: Span,
  },
  AddI64
  {
    result: LocalId,
    left: LocalId,
    right: LocalId,
    span: Span,
  },
  SubI64
  {
    result: LocalId,
    left: LocalId,
    right: LocalId,
    span: Span,
  },
  MulI64
  {
    result: LocalId,
    left: LocalId,
    right: LocalId,
    span: Span,
  },
  EqI64
  {
    result: LocalId,
    left: LocalId,
    right: LocalId,
    span: Span,
  },
  BinaryI64
  {
    operation: I64BinaryOperation,
    result: LocalId,
    left: LocalId,
    right: LocalId,
    span: Span,
  },
  CompareI64
  {
    operation: I64ComparisonOperation,
    result: LocalId,
    left: LocalId,
    right: LocalId,
    span: Span,
  },
  AddI32
  {
    result: LocalId,
    left: LocalId,
    right: LocalId,
    span: Span,
  },
  SubI32
  {
    result: LocalId,
    left: LocalId,
    right: LocalId,
    span: Span,
  },
  MulI32
  {
    result: LocalId,
    left: LocalId,
    right: LocalId,
    span: Span,
  },
  BinaryI32
  {
    operation: I32BinaryOperation,
    result: LocalId,
    left: LocalId,
    right: LocalId,
    span: Span,
  },
  EqI32
  {
    result: LocalId,
    left: LocalId,
    right: LocalId,
    span: Span,
  },
  LtI32
  {
    result: LocalId,
    left: LocalId,
    right: LocalId,
    span: Span,
  },
  LeI32
  {
    result: LocalId,
    left: LocalId,
    right: LocalId,
    span: Span,
  },
  GtI32
  {
    result: LocalId,
    left: LocalId,
    right: LocalId,
    span: Span,
  },
  GeI32
  {
    result: LocalId,
    left: LocalId,
    right: LocalId,
    span: Span,
  },
  NotBool
  {
    result: LocalId,
    operand: LocalId,
    span: Span,
  },
  Call
  {
    result: Option<LocalId>,
    function: String,
    arguments: Vec<LocalId>,
    return_type: Type,
    span: Span,
  },
  Drop
  {
    local: LocalId, span: Span
  },
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum Terminator
{
  Return(Option<LocalId>),
  Goto(BlockId),
  BranchIf
  {
    condition: LocalId,
    then_block: BlockId,
    else_block: BlockId,
  },
  Panic,
  Unreachable,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct BasicBlock
{
  pub id: BlockId,
  pub statements: Vec<Statement>,
  pub terminator: Option<Terminator>,
  pub span: Span,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct Function
{
  pub name: String,
  pub parameters: Vec<Parameter>,
  pub return_type: Type,
  pub locals: Vec<Local>,
  pub blocks: Vec<BasicBlock>,
}

#[derive(Clone, Debug, Default, Eq, PartialEq)]
struct BorrowState
{
  moved: bool,
  shared_count: u32,
  mutable_active: bool,
}

#[derive(Default)]
pub struct BorrowChecker
{
  diagnostics: Vec<Diagnostic>,
  local_defs: HashMap<LocalId, Local>,
  states: HashMap<LocalId, BorrowState>,
}

impl BorrowChecker
{
  #[must_use]
  pub fn new() -> Self
  {
    Self::default()
  }

  #[must_use]
  pub fn check_function(mut self, function: &Function) -> Vec<Diagnostic>
  {
    for parameter in &function.parameters
    {
      let local = Local { id: parameter.local,
                          name: parameter.name.clone(),
                          value_type: Some(parameter.value_type),
                          mutable: false,
                          span: parameter.span };
      self.local_defs.insert(local.id, local);
      self.states.insert(parameter.local, BorrowState::default());
    }
    for local in &function.locals
    {
      self.local_defs.insert(local.id, local.clone());
      self.states.insert(local.id, BorrowState::default());
    }
    for block in &function.blocks
    {
      self.check_block(block);
    }
    self.diagnostics
  }

  fn check_block(&mut self, block: &BasicBlock)
  {
    if block.terminator.is_none()
    {
      self.report(DiagnosticCode::MissingTerminator,
                  "MIR block is missing a terminator",
                  block.span);
    }
    for statement in &block.statements
    {
      self.check_statement(statement);
    }
    if let Some(Terminator::BranchIf { condition, .. }) = block.terminator
    {
      self.require_live(condition, block.span);
    }
  }

  fn check_statement(&mut self, statement: &Statement)
  {
    match *statement
    {
      Statement::Use { local,
                       span, } |
      Statement::ConstI64 { local,
                            span,
                            .. } |
      Statement::ConstI32 { local,
                            span,
                            .. } |
      Statement::ConstU16 { local,
                            span,
                            .. } |
      Statement::ConstInteger { local,
                                span,
                                .. } |
      Statement::ConstF32 { local,
                            span,
                            .. } |
      Statement::ConstF64 { local,
                            span,
                            .. } |
      Statement::ConstStr { local,
                            span,
                            .. } |
      Statement::ConstBool { local,
                             span,
                             .. } |
      Statement::Drop { local,
                        span, } => self.require_live(local, span),
      Statement::StoreLocal { local,
                              value,
                              span, } =>
      {
        self.require_live(value, span);
        let _ = self.state(local, span);
      }
      Statement::AddI64 { left,
                          right,
                          result,
                          span,
                          .. } |
      Statement::SubI64 { left,
                          right,
                          result,
                          span,
                          .. } |
      Statement::MulI64 { left,
                          right,
                          result,
                          span,
                          .. } |
      Statement::EqI64 { left,
                         right,
                         result,
                         span,
                         .. } |
      Statement::BinaryI64 { left,
                             right,
                             result,
                             span,
                             .. } |
      Statement::CompareI64 { left,
                              right,
                              result,
                              span,
                              .. } |
      Statement::BinaryFloat { left,
                               right,
                               result,
                               span,
                               .. } |
      Statement::CompareFloat { left,
                                right,
                                result,
                                span,
                                .. } |
      Statement::CompareStr { left,
                              right,
                              result,
                              span,
                              .. } |
      Statement::BinaryInteger { left,
                                 right,
                                 result,
                                 span,
                                 .. } |
      Statement::AddI32 { left,
                          right,
                          result,
                          span,
                          .. } |
      Statement::SubI32 { left,
                          right,
                          result,
                          span,
                          .. } |
      Statement::MulI32 { left,
                          right,
                          result,
                          span,
                          .. } |
      Statement::BinaryI32 { left,
                             right,
                             result,
                             span,
                             .. } |
      Statement::EqI32 { left,
                         right,
                         result,
                         span,
                         .. } |
      Statement::LtI32 { left,
                         right,
                         result,
                         span,
                         .. } |
      Statement::LeI32 { left,
                         right,
                         result,
                         span,
                         .. } |
      Statement::GtI32 { left,
                         right,
                         result,
                         span,
                         .. } |
      Statement::GeI32 { left,
                         right,
                         result,
                         span,
                         .. } =>
      {
        self.require_live(left, span);
        self.require_live(right, span);
        let _ = self.state(result, span);
      }
      Statement::NotBool { result,
                           operand,
                           span, } =>
      {
        self.require_live(operand, span);
        let _ = self.state(result, span);
      }
      Statement::Call { result,
                        ref arguments,
                        span,
                        .. } => self.call(result, arguments, span),
      Statement::LoadLocal { result,
                             local,
                             span, } =>
      {
        self.require_live(local, span);
        let _ = self.state(result, span);
      }
      Statement::Aggregate { result,
                             ref fields,
                             span,
                             .. } =>
      {
        for field in fields
        {
          self.require_live(*field, span);
        }
        let _ = self.state(result, span);
      }
      Statement::Extract { result,
                           aggregate,
                           span,
                           .. } =>
      {
        self.require_live(aggregate, span);
        let _ = self.state(result, span);
      }
      Statement::ArrayGet { result,
                            array,
                            index,
                            span,
                            .. } =>
      {
        self.require_live(array, span);
        self.require_live(index, span);
        let _ = self.state(result, span);
      }
      Statement::ArraySet { result,
                            array,
                            index,
                            value,
                            span,
                            .. } =>
      {
        self.require_live(array, span);
        self.require_live(index, span);
        self.require_live(value, span);
        let _ = self.state(result, span);
      }
      Statement::ArrayLength { result,
                               array,
                               span,
                               .. } =>
      {
        self.require_live(array, span);
        let _ = self.state(result, span);
      }
      Statement::Move { local,
                        span, } => self.move_local(local, span),
      Statement::BorrowShared { local,
                                span, } => self.borrow_shared(local, span),
      Statement::BorrowMutable { local,
                                 span, } => self.borrow_mutable(local, span),
      Statement::EndBorrow { local, .. } => self.end_borrow(local),
    }
  }

  fn require_live(&mut self, local: LocalId, span: Span)
  {
    let Some(state) = self.state(local, span)
    else
    {
      return;
    };
    if state.moved
    {
      self.report(DiagnosticCode::UseAfterMove, "local is used after it was moved", span);
    }
  }

  fn call(&mut self, result: Option<LocalId>, arguments: &[LocalId], span: Span)
  {
    for argument in arguments
    {
      self.require_live(*argument, span);
    }
    if let Some(result) = result
    {
      let _ = self.state(result, span);
    }
  }

  fn move_local(&mut self, local: LocalId, span: Span)
  {
    let Some(state) = self.state(local, span)
    else
    {
      return;
    };
    if state.moved
    {
      self.report(DiagnosticCode::UseAfterMove,
                  "local is moved after it was already moved",
                  span);
      return;
    }
    if state.shared_count != 0 || state.mutable_active
    {
      self.report(DiagnosticCode::MoveWhileBorrowed,
                  "local cannot be moved while borrowed",
                  span);
      return;
    }
    state.moved = true;
  }

  fn borrow_shared(&mut self, local: LocalId, span: Span)
  {
    let Some(state) = self.state(local, span)
    else
    {
      return;
    };
    if state.moved
    {
      self.report(DiagnosticCode::UseAfterMove,
                  "local is borrowed after it was moved",
                  span);
      return;
    }
    if state.mutable_active
    {
      self.report(DiagnosticCode::MutableBorrowConflict,
                  "shared borrow conflicts with active mutable borrow",
                  span);
      return;
    }
    state.shared_count += 1;
  }

  fn borrow_mutable(&mut self, local: LocalId, span: Span)
  {
    if !self.local_defs.get(&local).is_some_and(|local| local.mutable)
    {
      self.report(DiagnosticCode::ImmutableLocalMutableBorrow,
                  "immutable local cannot be mutably borrowed",
                  span);
      return;
    }
    let Some(state) = self.state(local, span)
    else
    {
      return;
    };
    if state.moved
    {
      self.report(DiagnosticCode::UseAfterMove,
                  "local is mutably borrowed after it was moved",
                  span);
      return;
    }
    if state.shared_count != 0 || state.mutable_active
    {
      self.report(DiagnosticCode::MutableBorrowConflict,
                  "mutable borrow conflicts with an active borrow",
                  span);
      return;
    }
    state.mutable_active = true;
  }

  fn end_borrow(&mut self, local: LocalId)
  {
    if let Some(state) = self.states.get_mut(&local)
    {
      if state.mutable_active
      {
        state.mutable_active = false;
      }
      else if state.shared_count != 0
      {
        state.shared_count -= 1;
      }
    }
  }

  fn state(&mut self, local: LocalId, span: Span) -> Option<&mut BorrowState>
  {
    if !self.local_defs.contains_key(&local)
    {
      self.report(DiagnosticCode::UnknownLocal, "MIR references an unknown local", span);
      return None;
    }
    self.states.get_mut(&local)
  }

  fn report(&mut self, code: DiagnosticCode, message: &str, span: Span)
  {
    self.diagnostics.push(Diagnostic { code,
                                       message: message.to_string(),
                                       span });
  }
}

#[cfg(test)]
mod tests
{
  use super::*;

  fn span(start: u32, end: u32) -> Span
  {
    Span::new(1, start, end)
  }

  fn local(id: u32, mutable: bool) -> Local
  {
    Local { id: LocalId(id),
            name: format!("local{id}"),
            value_type: None,
            mutable,
            span: span(0, 1) }
  }

  fn function(statements: Vec<Statement>, terminator: Option<Terminator>) -> Function
  {
    Function { name: "main".to_string(),
               parameters: vec![],
               return_type: Type::VOID,
               locals: vec![local(0, true), local(1, false)],
               blocks: vec![BasicBlock { id: BlockId(0),
                                         statements,
                                         terminator,
                                         span: span(0, 10) }] }
  }

  #[test]
  fn accepts_move_after_borrow_ends()
  {
    let function = function(vec![Statement::BorrowShared { local: LocalId(0),
                                                           span: span(1, 2) },
                                 Statement::EndBorrow { local: LocalId(0),
                                                        span: span(2, 3) },
                                 Statement::Move { local: LocalId(0),
                                                   span: span(3, 4) }],
                            Some(Terminator::Return(None)));

    assert!(BorrowChecker::new().check_function(&function).is_empty());
  }

  #[test]
  fn rejects_use_after_move()
  {
    let function = function(vec![Statement::Move { local: LocalId(0),
                                                   span: span(1, 2) },
                                 Statement::Use { local: LocalId(0),
                                                  span: span(3, 4) }],
                            Some(Terminator::Return(None)));

    let diagnostics = BorrowChecker::new().check_function(&function);

    assert_eq!(diagnostics.len(), 1);
    assert_eq!(diagnostics[0].code, DiagnosticCode::UseAfterMove);
  }

  #[test]
  fn rejects_branch_if_condition_after_move()
  {
    let function = function(vec![Statement::Move { local: LocalId(0),
                                                   span: span(1, 2) }],
                            Some(Terminator::BranchIf { condition: LocalId(0),
                                                        then_block: BlockId(0),
                                                        else_block: BlockId(0) }));

    let diagnostics = BorrowChecker::new().check_function(&function);

    assert_eq!(diagnostics.len(), 1);
    assert_eq!(diagnostics[0].code, DiagnosticCode::UseAfterMove);
  }

  #[test]
  fn rejects_move_while_borrowed()
  {
    let function = function(vec![Statement::BorrowShared { local: LocalId(0),
                                                           span: span(1, 2) },
                                 Statement::Move { local: LocalId(0),
                                                   span: span(3, 4) }],
                            Some(Terminator::Return(None)));

    let diagnostics = BorrowChecker::new().check_function(&function);

    assert_eq!(diagnostics.len(), 1);
    assert_eq!(diagnostics[0].code, DiagnosticCode::MoveWhileBorrowed);
  }

  #[test]
  fn rejects_mutable_borrow_conflict()
  {
    let function = function(vec![Statement::BorrowShared { local: LocalId(0),
                                                           span: span(1, 2) },
                                 Statement::BorrowMutable { local: LocalId(0),
                                                            span: span(3, 4) }],
                            Some(Terminator::Return(None)));

    let diagnostics = BorrowChecker::new().check_function(&function);

    assert_eq!(diagnostics.len(), 1);
    assert_eq!(diagnostics[0].code, DiagnosticCode::MutableBorrowConflict);
  }

  #[test]
  fn rejects_mutable_borrow_of_immutable_local()
  {
    let function = function(vec![Statement::BorrowMutable { local: LocalId(1),
                                                            span: span(1, 2) }],
                            Some(Terminator::Return(None)));

    let diagnostics = BorrowChecker::new().check_function(&function);

    assert_eq!(diagnostics.len(), 1);
    assert_eq!(diagnostics[0].code, DiagnosticCode::ImmutableLocalMutableBorrow);
  }

  #[test]
  fn treats_parameters_as_immutable_live_locals()
  {
    let function =
      Function { name: "main".to_string(),
                 parameters: vec![Parameter { local: LocalId(0),
                                              name: "input".to_string(),
                                              value_type: Type::I64,
                                              span: span(0, 1) }],
                 return_type: Type::VOID,
                 locals: vec![],
                 blocks: vec![BasicBlock { id: BlockId(0),
                                           statements: vec![Statement::BorrowMutable { local: LocalId(0),
                                                                                       span: span(1, 2) }],
                                           terminator: Some(Terminator::Return(None)),
                                           span: span(0, 2) }] };

    let diagnostics = BorrowChecker::new().check_function(&function);

    assert_eq!(diagnostics[0].code, DiagnosticCode::ImmutableLocalMutableBorrow);
  }

  #[test]
  fn reports_missing_terminator()
  {
    let diagnostics = BorrowChecker::new().check_function(&function(vec![], None));

    assert_eq!(diagnostics.len(), 1);
    assert_eq!(diagnostics[0].code, DiagnosticCode::MissingTerminator);
  }

  #[test]
  fn computes_reachable_blocks_through_goto()
  {
    let function = Function { name: "main".to_string(),
                              parameters: vec![],
                              return_type: Type::VOID,
                              locals: vec![],
                              blocks: vec![BasicBlock { id: BlockId(0),
                                                        statements: vec![],
                                                        terminator: Some(Terminator::Goto(BlockId(1))),
                                                        span: span(0, 1) },
                                           BasicBlock { id: BlockId(1),
                                                        statements: vec![],
                                                        terminator: Some(Terminator::Return(None)),
                                                        span: span(1, 2) },
                                           BasicBlock { id: BlockId(2),
                                                        statements: vec![],
                                                        terminator: Some(Terminator::Return(None)),
                                                        span: span(2, 3) }] };

    let reachable = reachable_blocks(&function);

    assert!(reachable.contains(&BlockId(0)));
    assert!(reachable.contains(&BlockId(1)));
    assert!(!reachable.contains(&BlockId(2)));
  }

  #[test]
  fn computes_reachable_blocks_through_branch_if()
  {
    let function =
      Function { name: "main".to_string(),
                 parameters: vec![],
                 return_type: Type::VOID,
                 locals: vec![],
                 blocks: vec![BasicBlock { id: BlockId(0),
                                           statements: vec![],
                                           terminator: Some(Terminator::BranchIf { condition: LocalId(0),
                                                                                   then_block: BlockId(1),
                                                                                   else_block: BlockId(2) }),
                                           span: span(0, 1) },
                              BasicBlock { id: BlockId(1),
                                           statements: vec![],
                                           terminator: Some(Terminator::Return(None)),
                                           span: span(1, 2) },
                              BasicBlock { id: BlockId(2),
                                           statements: vec![],
                                           terminator: Some(Terminator::Return(None)),
                                           span: span(2, 3) }] };

    let reachable = reachable_blocks(&function);

    assert!(reachable.contains(&BlockId(0)));
    assert!(reachable.contains(&BlockId(1)));
    assert!(reachable.contains(&BlockId(2)));
  }
}
