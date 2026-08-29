/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

use std::collections::HashMap;

use bit_set::BitSet;
use indexmap::IndexMap;

use super::ControlFlowGraph;
use crate::mir::BlockId;

/// Dominance relation, immediate-dominator tree, and dominance frontiers.
#[derive(Clone, Debug)]
pub struct DominatorTree
{
    entry: Option<BlockId>,
    order: Vec<BlockId>,
    indices: HashMap<BlockId, usize>,
    dominators: Vec<BitSet>,
    immediate: IndexMap<BlockId, Option<BlockId>>,
    children: IndexMap<BlockId, Vec<BlockId>>,
    frontiers: IndexMap<BlockId, Vec<BlockId>>,
}

impl DominatorTree
{
    /// Computes dominance for every block reachable from the CFG entry.
    #[must_use]
    pub fn new(cfg: &ControlFlowGraph) -> Self
    {
        let entry = cfg.entry();
        let order = cfg.reverse_postorder();
        let indices = order
            .iter()
            .enumerate()
            .map(|(index, block)| (*block, index))
            .collect::<HashMap<_, _>>();
        let mut dominators = initial_sets(order.len(), entry.and_then(|block| indices.get(&block).copied()));
        solve_dominators(cfg, &order, &indices, &mut dominators);
        let immediate = compute_immediate(entry, &order, &indices, &dominators);
        let function_order = cfg.blocks().collect::<Vec<_>>();
        let children = compute_children(&order, &function_order, &immediate);
        let frontiers = compute_frontiers(cfg, &order, &immediate);
        Self {
            entry,
            order,
            indices,
            dominators,
            immediate,
            children,
            frontiers,
        }
    }

    /// Entry block used by this tree.
    #[must_use]
    pub const fn entry(&self) -> Option<BlockId>
    {
        self.entry
    }

    /// Reachable blocks in reverse-postorder.
    #[must_use]
    pub fn blocks(&self) -> &[BlockId]
    {
        &self.order
    }

    /// Returns whether `dominator` dominates `block`.
    #[must_use]
    pub fn dominates(&self, dominator: BlockId, block: BlockId) -> bool
    {
        let (Some(dominator), Some(block)) = (self.indices.get(&dominator), self.indices.get(&block))
        else
        {
            return false;
        };
        self.dominators[*block].contains(*dominator)
    }

    /// Strict dominators ordered from entry toward `block`.
    #[must_use]
    pub fn strict_dominators(&self, block: BlockId) -> Vec<BlockId>
    {
        let Some(index) = self.indices.get(&block).copied()
        else
        {
            return Vec::new();
        };
        self.order
            .iter()
            .enumerate()
            .filter(|(candidate, _)| *candidate != index && self.dominators[index].contains(*candidate))
            .map(|(_, block)| *block)
            .collect()
    }

    /// Immediate dominator, or `None` for entry/unreachable blocks.
    #[must_use]
    pub fn immediate_dominator(&self, block: BlockId) -> Option<BlockId>
    {
        self.immediate.get(&block).copied().flatten()
    }

    /// Direct children in the immediate-dominator tree.
    #[must_use]
    pub fn children(&self, block: BlockId) -> &[BlockId]
    {
        self.children.get(&block).map_or(&[], Vec::as_slice)
    }

    /// Dominance frontier of `block` in stable function order.
    #[must_use]
    pub fn frontier(&self, block: BlockId) -> &[BlockId]
    {
        self.frontiers.get(&block).map_or(&[], Vec::as_slice)
    }

    /// Distance from the entry in the immediate-dominator tree.
    #[must_use]
    pub fn depth(&self, block: BlockId) -> Option<usize>
    {
        self.indices.get(&block)?;
        let mut depth = 0;
        let mut current = block;
        while let Some(parent) = self.immediate_dominator(current)
        {
            depth += 1;
            current = parent;
        }
        Some(depth)
    }

    /// Nearest common dominator of two reachable blocks.
    #[must_use]
    pub fn nearest_common_dominator(&self, left: BlockId, right: BlockId) -> Option<BlockId>
    {
        let left = *self.indices.get(&left)?;
        let right = *self.indices.get(&right)?;
        self.order
            .iter()
            .enumerate()
            .filter(|(index, _)| self.dominators[left].contains(*index) && self.dominators[right].contains(*index))
            .max_by_key(|(_, block)| self.depth(**block).unwrap_or_default())
            .map(|(_, block)| *block)
    }
}

fn initial_sets(count: usize, entry: Option<usize>) -> Vec<BitSet>
{
    let all = (0..count).collect::<BitSet>();
    (0..count)
        .map(|index| {
            if Some(index) == entry
            {
                [index].into_iter().collect()
            }
            else
            {
                all.clone()
            }
        })
        .collect()
}

fn solve_dominators(
    cfg: &ControlFlowGraph,
    order: &[BlockId],
    indices: &HashMap<BlockId, usize>,
    dominators: &mut [BitSet],
)
{
    if order.is_empty()
    {
        return;
    }
    let mut changed = true;
    while changed
    {
        changed = false;
        for block in order.iter().skip(1)
        {
            let predecessors = cfg
                .predecessors(*block)
                .iter()
                .filter_map(|predecessor| indices.get(predecessor).copied())
                .collect::<Vec<_>>();
            let Some(first) = predecessors.first().copied()
            else
            {
                continue;
            };
            let mut next = dominators[first].clone();
            for predecessor in predecessors.iter().skip(1)
            {
                next.intersect_with(&dominators[*predecessor]);
            }
            next.insert(indices[block]);
            if next != dominators[indices[block]]
            {
                dominators[indices[block]] = next;
                changed = true;
            }
        }
    }
}

fn compute_immediate(
    entry: Option<BlockId>,
    order: &[BlockId],
    indices: &HashMap<BlockId, usize>,
    dominators: &[BitSet],
) -> IndexMap<BlockId, Option<BlockId>>
{
    order
        .iter()
        .copied()
        .map(|block| {
            if Some(block) == entry
            {
                return (block, None);
            }
            let block_index = indices[&block];
            let immediate = dominators[block_index]
                .iter()
                .filter(|candidate| *candidate != block_index)
                .max_by_key(|candidate| dominators[*candidate].iter().count())
                .map(|candidate| order[candidate]);
            (block, immediate)
        })
        .collect()
}

fn compute_children(
    order: &[BlockId],
    function_order: &[BlockId],
    immediate: &IndexMap<BlockId, Option<BlockId>>,
) -> IndexMap<BlockId, Vec<BlockId>>
{
    let positions = function_order
        .iter()
        .enumerate()
        .map(|(index, block)| (*block, index))
        .collect::<HashMap<_, _>>();
    let mut children = order
        .iter()
        .copied()
        .map(|block| (block, Vec::new()))
        .collect::<IndexMap<_, _>>();
    for (block, parent) in immediate
    {
        if let Some(parent) = parent
        {
            children[parent].push(*block);
        }
    }
    for values in children.values_mut()
    {
        values.sort_by_key(|block| positions[block]);
    }
    children
}

fn compute_frontiers(
    cfg: &ControlFlowGraph,
    order: &[BlockId],
    immediate: &IndexMap<BlockId, Option<BlockId>>,
) -> IndexMap<BlockId, Vec<BlockId>>
{
    let positions = order
        .iter()
        .enumerate()
        .map(|(index, block)| (*block, index))
        .collect::<HashMap<_, _>>();
    let mut frontiers = order
        .iter()
        .copied()
        .map(|block| (block, Vec::new()))
        .collect::<IndexMap<_, _>>();
    for join in order
    {
        if cfg.predecessors(*join).len() < 2
        {
            continue;
        }
        let stop = immediate[join];
        for predecessor in cfg.predecessors(*join)
        {
            let mut runner = Some(*predecessor);
            while runner.is_some() && runner != stop
            {
                let current = runner.expect("runner was checked");
                if let Some(frontier) = frontiers.get_mut(&current) &&
                    !frontier.contains(join)
                {
                    frontier.push(*join);
                }
                runner = immediate.get(&current).copied().flatten();
            }
        }
    }
    for frontier in frontiers.values_mut()
    {
        frontier.sort_by_key(|block| positions[block]);
    }
    frontiers
}
