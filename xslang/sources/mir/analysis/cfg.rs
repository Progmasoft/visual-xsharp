/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use std::collections::{HashMap, HashSet};

use indexmap::{IndexMap, IndexSet};
use petgraph::algo::kosaraju_scc;
use petgraph::graph::{DiGraph, NodeIndex};

use crate::mir::{BlockId, Function, Terminator};

/// One maximal set of blocks that can mutually reach one another.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct StronglyConnectedComponent
{
  blocks: Vec<BlockId>,
  cyclic: bool,
}

impl StronglyConnectedComponent
{
  /// Blocks in stable function order.
  #[must_use]
  pub fn blocks(&self) -> &[BlockId]
  {
    &self.blocks
  }

  /// Returns whether the component contains a cycle or self edge.
  #[must_use]
  pub const fn is_cyclic(&self) -> bool
  {
    self.cyclic
  }
}

/// Indexed control-flow graph for a MIR function.
#[derive(Clone, Debug)]
pub struct ControlFlowGraph
{
  entry: Option<BlockId>,
  blocks: IndexSet<BlockId>,
  successors: IndexMap<BlockId, Vec<BlockId>>,
  predecessors: IndexMap<BlockId, Vec<BlockId>>,
  dangling_targets: Vec<(BlockId, BlockId)>,
  graph: DiGraph<BlockId, ()>,
  nodes: HashMap<BlockId, NodeIndex>,
}

impl ControlFlowGraph
{
  /// Builds a graph without assuming that MIR verification already succeeded.
  ///
  /// Edges to missing blocks are retained in [`Self::dangling_targets`] but are
  /// not inserted into the traversable graph.
  #[must_use]
  pub fn new(function: &Function) -> Self
  {
    let blocks = function.blocks.iter().map(|block| block.id).collect::<IndexSet<_>>();
    let entry = function.blocks.first().map(|block| block.id);
    let mut graph = DiGraph::new();
    let nodes = blocks.iter()
                      .copied()
                      .map(|block| (block, graph.add_node(block)))
                      .collect::<HashMap<_, _>>();
    let mut successors = blocks.iter()
                               .copied()
                               .map(|block| (block, Vec::new()))
                               .collect::<IndexMap<_, _>>();
    let mut predecessors = successors.clone();
    let mut dangling_targets = Vec::new();

    for block in &function.blocks
    {
      for target in terminator_targets(block.terminator.as_ref())
      {
        if !blocks.contains(&target)
        {
          dangling_targets.push((block.id, target));
          continue;
        }
        push_unique(&mut successors[&block.id], target);
        push_unique(&mut predecessors[&target], block.id);
        graph.add_edge(nodes[&block.id], nodes[&target], ());
      }
    }

    Self { entry,
           blocks,
           successors,
           predecessors,
           dangling_targets,
           graph,
           nodes }
  }

  /// Entry block, which is the first block in function order.
  #[must_use]
  pub const fn entry(&self) -> Option<BlockId>
  {
    self.entry
  }

  /// All known block identifiers in function order.
  #[must_use]
  pub fn blocks(&self) -> impl ExactSizeIterator<Item = BlockId> + '_
  {
    self.blocks.iter().copied()
  }

  /// Returns whether the graph contains `block`.
  #[must_use]
  pub fn contains(&self, block: BlockId) -> bool
  {
    self.blocks.contains(&block)
  }

  /// Outgoing targets in terminator order.
  #[must_use]
  pub fn successors(&self, block: BlockId) -> &[BlockId]
  {
    self.successors.get(&block).map_or(&[], Vec::as_slice)
  }

  /// Incoming blocks in function order.
  #[must_use]
  pub fn predecessors(&self, block: BlockId) -> &[BlockId]
  {
    self.predecessors.get(&block).map_or(&[], Vec::as_slice)
  }

  /// Edges whose target does not exist in the function registry.
  #[must_use]
  pub fn dangling_targets(&self) -> &[(BlockId, BlockId)]
  {
    &self.dangling_targets
  }

  /// Blocks reachable from the entry in stable depth-first preorder.
  #[must_use]
  pub fn preorder(&self) -> Vec<BlockId>
  {
    let Some(entry) = self.entry
    else
    {
      return Vec::new();
    };
    let mut result = Vec::new();
    let mut visited = HashSet::new();
    let mut stack = vec![entry];
    while let Some(block) = stack.pop()
    {
      if !visited.insert(block)
      {
        continue;
      }
      result.push(block);
      stack.extend(self.successors(block).iter().rev().copied());
    }
    result
  }

  /// Reachable blocks in reverse postorder, suitable for forward dataflow.
  #[must_use]
  pub fn reverse_postorder(&self) -> Vec<BlockId>
  {
    let Some(entry) = self.entry
    else
    {
      return Vec::new();
    };
    let mut postorder = Vec::new();
    let mut visited = HashSet::new();
    self.visit_postorder(entry, &mut visited, &mut postorder);
    postorder.reverse();
    postorder
  }

  /// Blocks that cannot be reached from the entry.
  #[must_use]
  pub fn unreachable_blocks(&self) -> Vec<BlockId>
  {
    let reachable = self.preorder().into_iter().collect::<HashSet<_>>();
    self.blocks
        .iter()
        .filter(|block| !reachable.contains(block))
        .copied()
        .collect()
  }

  /// Reachable blocks without successors.
  #[must_use]
  pub fn exit_blocks(&self) -> Vec<BlockId>
  {
    self.preorder()
        .into_iter()
        .filter(|block| self.successors(*block).is_empty())
        .collect()
  }

  /// Maximal strongly connected components in deterministic block order.
  #[must_use]
  pub fn strongly_connected_components(&self) -> Vec<StronglyConnectedComponent>
  {
    let order = self.blocks
                    .iter()
                    .enumerate()
                    .map(|(index, block)| (*block, index))
                    .collect::<HashMap<_, _>>();
    let mut components =
      kosaraju_scc(&self.graph).into_iter()
                               .map(|nodes| {
                                 let mut blocks = nodes.into_iter().map(|node| self.graph[node]).collect::<Vec<_>>();
                                 blocks.sort_by_key(|block| order[block]);
                                 let cyclic = blocks.len() > 1 ||
                                              blocks.first()
                                                    .is_some_and(|block| self.successors(*block).contains(block));
                                 StronglyConnectedComponent { blocks,
                                                              cyclic }
                               })
                               .collect::<Vec<_>>();
    components.sort_by_key(|component| component.blocks.first().map_or(usize::MAX, |block| order[block]));
    components
  }

  /// Returns whether an edge exists between two known blocks.
  #[must_use]
  pub fn has_edge(&self, source: BlockId, target: BlockId) -> bool
  {
    let (Some(source), Some(target)) = (self.nodes.get(&source), self.nodes.get(&target))
    else
    {
      return false;
    };
    self.graph.find_edge(*source, *target).is_some()
  }

  fn visit_postorder(&self, block: BlockId, visited: &mut HashSet<BlockId>, result: &mut Vec<BlockId>)
  {
    if !visited.insert(block)
    {
      return;
    }
    for successor in self.successors(block)
    {
      self.visit_postorder(*successor, visited, result);
    }
    result.push(block);
  }
}

fn terminator_targets(terminator: Option<&Terminator>) -> Vec<BlockId>
{
  match terminator
  {
    Some(Terminator::Goto(target)) => vec![*target],
    Some(Terminator::BranchIf { then_block,
                                else_block,
                                .. }) => vec![*then_block, *else_block],
    _ => Vec::new(),
  }
}

fn push_unique(values: &mut Vec<BlockId>, value: BlockId)
{
  if !values.contains(&value)
  {
    values.push(value);
  }
}
