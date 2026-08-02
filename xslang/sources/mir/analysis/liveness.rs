/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use std::collections::{HashMap, HashSet};

use indexmap::IndexMap;

use super::ControlFlowGraph;
use crate::mir::{BlockId, Function, LocalId, Statement, Terminator};

/// Local-liveness summary at the boundary of one block.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct BlockLiveness
{
  live_in: Vec<LocalId>,
  live_out: Vec<LocalId>,
  uses: Vec<LocalId>,
  definitions: Vec<LocalId>,
  before_statements: Vec<Vec<LocalId>>,
  before_terminator: Vec<LocalId>,
}

impl BlockLiveness
{
  /// Locals needed before the first statement executes.
  #[must_use]
  pub fn live_in(&self) -> &[LocalId]
  {
    &self.live_in
  }

  /// Locals needed along at least one outgoing edge.
  #[must_use]
  pub fn live_out(&self) -> &[LocalId]
  {
    &self.live_out
  }

  /// Locals read before their first definition in this block.
  #[must_use]
  pub fn upward_exposed_uses(&self) -> &[LocalId]
  {
    &self.uses
  }

  /// Locals defined by statements in this block.
  #[must_use]
  pub fn definitions(&self) -> &[LocalId]
  {
    &self.definitions
  }

  /// Locals live immediately before statement `index`.
  #[must_use]
  pub fn before_statement(&self, index: usize) -> Option<&[LocalId]>
  {
    self.before_statements.get(index).map(Vec::as_slice)
  }

  /// Locals live immediately before the terminator executes.
  #[must_use]
  pub fn before_terminator(&self) -> &[LocalId]
  {
    &self.before_terminator
  }
}

/// Backward local-liveness solution for all reachable MIR blocks.
#[derive(Clone, Debug, Default, Eq, PartialEq)]
pub struct Liveness
{
  blocks: IndexMap<BlockId, BlockLiveness>,
}

impl Liveness
{
  /// Solves the standard backward `in = use ∪ (out - def)` equations.
  #[must_use]
  pub fn new(function: &Function, cfg: &ControlFlowGraph) -> Self
  {
    let function_blocks = function.blocks
                                  .iter()
                                  .map(|block| (block.id, block))
                                  .collect::<HashMap<_, _>>();
    let reachable = cfg.reverse_postorder();
    let facts = reachable.iter()
                         .filter_map(|block| {
                           let block_record = function_blocks.get(block)?;
                           Some((*block, local_facts(&block_record.statements, block_record.terminator.as_ref())))
                         })
                         .collect::<IndexMap<_, _>>();
    let mut live_in = reachable.iter()
                               .copied()
                               .map(|block| (block, HashSet::new()))
                               .collect::<HashMap<_, _>>();
    let mut live_out = live_in.clone();

    let mut changed = true;
    while changed
    {
      changed = false;
      for block in reachable.iter().rev()
      {
        let next_out = cfg.successors(*block)
                          .iter()
                          .filter_map(|successor| live_in.get(successor))
                          .flatten()
                          .copied()
                          .collect::<HashSet<_>>();
        let mut next_in = facts[block].uses.clone();
        next_in.extend(next_out.difference(&facts[block].definitions).copied());
        if live_out[block] != next_out
        {
          live_out.insert(*block, next_out);
          changed = true;
        }
        if live_in[block] != next_in
        {
          live_in.insert(*block, next_in);
          changed = true;
        }
      }
    }

    let blocks = reachable.into_iter()
                          .filter_map(|block| {
                            let record = function_blocks.get(&block)?;
                            let out = live_out.remove(&block).unwrap_or_default();
                            let (before_statements, before_terminator) = statement_liveness(record, &out);
                            Some((block,
                                  BlockLiveness { live_in: sorted(live_in.remove(&block).unwrap_or_default()),
                                                  live_out: sorted(out),
                                                  uses: sorted(facts[&block].uses.clone()),
                                                  definitions: sorted(facts[&block].definitions.clone()),
                                                  before_statements,
                                                  before_terminator }))
                          })
                          .collect();
    Self { blocks }
  }

  /// Summary for one reachable block.
  #[must_use]
  pub fn block(&self, block: BlockId) -> Option<&BlockLiveness>
  {
    self.blocks.get(&block)
  }

  /// Reachable block summaries in reverse-postorder.
  pub fn blocks(&self) -> impl ExactSizeIterator<Item = (BlockId, &BlockLiveness)>
  {
    self.blocks.iter().map(|(block, liveness)| (*block, liveness))
  }

  /// Returns whether a local is live on entry to `block`.
  #[must_use]
  pub fn is_live_in(&self, block: BlockId, local: LocalId) -> bool
  {
    self.block(block).is_some_and(|facts| facts.live_in.contains(&local))
  }

  /// Returns whether a local is live on exit from `block`.
  #[must_use]
  pub fn is_live_out(&self, block: BlockId, local: LocalId) -> bool
  {
    self.block(block).is_some_and(|facts| facts.live_out.contains(&local))
  }
}

#[derive(Clone, Debug, Default)]
struct LocalFacts
{
  uses: HashSet<LocalId>,
  definitions: HashSet<LocalId>,
}

fn local_facts(statements: &[Statement], terminator: Option<&Terminator>) -> LocalFacts
{
  let mut facts = LocalFacts::default();
  for statement in statements
  {
    for local in statement_uses(statement)
    {
      if !facts.definitions.contains(&local)
      {
        facts.uses.insert(local);
      }
    }
    facts.definitions.extend(statement_definitions(statement));
  }
  for local in terminator_uses(terminator)
  {
    if !facts.definitions.contains(&local)
    {
      facts.uses.insert(local);
    }
  }
  facts
}

fn statement_liveness(block: &crate::mir::BasicBlock, live_out: &HashSet<LocalId>)
                      -> (Vec<Vec<LocalId>>, Vec<LocalId>)
{
  let mut live = live_out.clone();
  live.extend(terminator_uses(block.terminator.as_ref()));
  let before_terminator = sorted(live.clone());
  let mut snapshots = vec![Vec::new(); block.statements.len()];
  for (index, statement) in block.statements.iter().enumerate().rev()
  {
    for definition in statement_definitions(statement)
    {
      live.remove(&definition);
    }
    live.extend(statement_uses(statement));
    snapshots[index] = sorted(live.clone());
  }
  (snapshots, before_terminator)
}

fn statement_uses(statement: &Statement) -> Vec<LocalId>
{
  match statement
  {
    Statement::Use { local, .. } |
    Statement::Move { local, .. } |
    Statement::BorrowShared { local, .. } |
    Statement::BorrowMutable { local, .. } |
    Statement::EndBorrow { local, .. } |
    Statement::Drop { local, .. } => vec![*local],
    Statement::StoreLocal { value, .. } => vec![*value],
    Statement::LoadLocal { local, .. } => vec![*local],
    Statement::BinaryInteger { left,
                               right,
                               .. } |
    Statement::BinaryFloat { left,
                             right,
                             .. } |
    Statement::CompareFloat { left,
                              right,
                              .. } |
    Statement::CompareStr { left,
                            right,
                            .. } |
    Statement::AddI64 { left,
                        right,
                        .. } |
    Statement::SubI64 { left,
                        right,
                        .. } |
    Statement::MulI64 { left,
                        right,
                        .. } |
    Statement::EqI64 { left,
                       right,
                       .. } |
    Statement::BinaryI64 { left,
                           right,
                           .. } |
    Statement::CompareI64 { left,
                            right,
                            .. } |
    Statement::AddI32 { left,
                        right,
                        .. } |
    Statement::SubI32 { left,
                        right,
                        .. } |
    Statement::MulI32 { left,
                        right,
                        .. } |
    Statement::BinaryI32 { left,
                           right,
                           .. } |
    Statement::EqI32 { left,
                       right,
                       .. } |
    Statement::LtI32 { left,
                       right,
                       .. } |
    Statement::LeI32 { left,
                       right,
                       .. } |
    Statement::GtI32 { left,
                       right,
                       .. } |
    Statement::GeI32 { left,
                       right,
                       .. } => vec![*left, *right],
    Statement::NotBool { operand, .. } => vec![*operand],
    Statement::Call { arguments, .. } => arguments.clone(),
    Statement::Aggregate { fields, .. } => fields.clone(),
    Statement::Extract { aggregate, .. } => vec![*aggregate],
    Statement::ArrayGet { array,
                          index,
                          .. } => vec![*array, *index],
    Statement::ArrayLength { array, .. } => vec![*array],
    Statement::ArraySet { array,
                          index,
                          value,
                          .. } => vec![*array, *index, *value],
    Statement::ConstI64 { .. } |
    Statement::ConstI32 { .. } |
    Statement::ConstU16 { .. } |
    Statement::ConstInteger { .. } |
    Statement::ConstF32 { .. } |
    Statement::ConstF64 { .. } |
    Statement::ConstStr { .. } |
    Statement::ConstBool { .. } => Vec::new(),
  }
}

fn statement_definitions(statement: &Statement) -> Vec<LocalId>
{
  match statement
  {
    Statement::ConstI64 { local, .. } |
    Statement::ConstI32 { local, .. } |
    Statement::ConstU16 { local, .. } |
    Statement::ConstInteger { local, .. } |
    Statement::ConstF32 { local, .. } |
    Statement::ConstF64 { local, .. } |
    Statement::ConstStr { local, .. } |
    Statement::ConstBool { local, .. } |
    Statement::StoreLocal { local, .. } => vec![*local],
    Statement::BinaryInteger { result, .. } |
    Statement::BinaryFloat { result, .. } |
    Statement::CompareFloat { result, .. } |
    Statement::CompareStr { result, .. } |
    Statement::LoadLocal { result, .. } |
    Statement::Aggregate { result, .. } |
    Statement::Extract { result, .. } |
    Statement::ArrayGet { result, .. } |
    Statement::ArraySet { result, .. } |
    Statement::ArrayLength { result, .. } |
    Statement::AddI64 { result, .. } |
    Statement::SubI64 { result, .. } |
    Statement::MulI64 { result, .. } |
    Statement::EqI64 { result, .. } |
    Statement::BinaryI64 { result, .. } |
    Statement::CompareI64 { result, .. } |
    Statement::AddI32 { result, .. } |
    Statement::SubI32 { result, .. } |
    Statement::MulI32 { result, .. } |
    Statement::BinaryI32 { result, .. } |
    Statement::EqI32 { result, .. } |
    Statement::LtI32 { result, .. } |
    Statement::LeI32 { result, .. } |
    Statement::GtI32 { result, .. } |
    Statement::GeI32 { result, .. } |
    Statement::NotBool { result, .. } => vec![*result],
    Statement::Call { result, .. } => result.iter().copied().collect(),
    Statement::Use { .. } |
    Statement::Move { .. } |
    Statement::BorrowShared { .. } |
    Statement::BorrowMutable { .. } |
    Statement::EndBorrow { .. } |
    Statement::Drop { .. } => Vec::new(),
  }
}

fn terminator_uses(terminator: Option<&Terminator>) -> Vec<LocalId>
{
  match terminator
  {
    Some(Terminator::Return(Some(value))) => vec![*value],
    Some(Terminator::BranchIf { condition, .. }) => vec![*condition],
    _ => Vec::new(),
  }
}

fn sorted(values: HashSet<LocalId>) -> Vec<LocalId>
{
  let mut values = values.into_iter().collect::<Vec<_>>();
  values.sort_by_key(|local| local.0);
  values
}
