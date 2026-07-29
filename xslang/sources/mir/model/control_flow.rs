/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use std::collections::{HashMap, HashSet};

use super::{BlockId, Function, Terminator};

#[must_use]
pub fn reachable_blocks(function: &Function) -> HashSet<BlockId>
{
  let known = function.blocks
                      .iter()
                      .map(|block| (block.id, block))
                      .collect::<HashMap<_, _>>();
  let Some(entry) = function.blocks.first()
  else
  {
    return HashSet::new();
  };
  let mut reachable = HashSet::new();
  let mut stack = vec![entry.id];
  while let Some(block_id) = stack.pop()
  {
    if !reachable.insert(block_id)
    {
      continue;
    }
    match known.get(&block_id).and_then(|block| block.terminator.as_ref())
    {
      Some(Terminator::Goto(next)) => stack.push(*next),
      Some(Terminator::BranchIf { then_block,
                                  else_block,
                                  .. }) =>
      {
        stack.push(*then_block);
        stack.push(*else_block);
      }
      _ =>
      {}
    }
  }
  reachable
}
