/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use std::collections::{HashMap, HashSet};

use super::{ControlFlowGraph, DominatorTree};
use crate::mir::BlockId;

/// One natural loop discovered from a dominance back edge.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct NaturalLoop
{
    header: BlockId,
    latches: Vec<BlockId>,
    blocks: Vec<BlockId>,
    exits: Vec<(BlockId, BlockId)>,
    parent: Option<usize>,
    children: Vec<usize>,
}

impl NaturalLoop
{
    /// Dominating loop header.
    #[must_use]
    pub const fn header(&self) -> BlockId
    {
        self.header
    }

    /// Blocks with a back edge into the header.
    #[must_use]
    pub fn latches(&self) -> &[BlockId]
    {
        &self.latches
    }

    /// All blocks in stable function order.
    #[must_use]
    pub fn blocks(&self) -> &[BlockId]
    {
        &self.blocks
    }

    /// Edges that leave the loop body.
    #[must_use]
    pub fn exits(&self) -> &[(BlockId, BlockId)]
    {
        &self.exits
    }

    /// Smallest containing loop.
    #[must_use]
    pub const fn parent(&self) -> Option<usize>
    {
        self.parent
    }

    /// Direct nested loops.
    #[must_use]
    pub fn children(&self) -> &[usize]
    {
        &self.children
    }

    /// Returns whether this loop contains `block`.
    #[must_use]
    pub fn contains(&self, block: BlockId) -> bool
    {
        self.blocks.contains(&block)
    }
}

/// Forest of natural loops in a MIR CFG.
#[derive(Clone, Debug, Default, Eq, PartialEq)]
pub struct LoopForest
{
    loops: Vec<NaturalLoop>,
    depths: HashMap<BlockId, usize>,
}

impl LoopForest
{
    /// Discovers and nests natural loops using CFG dominance.
    #[must_use]
    pub fn new(cfg: &ControlFlowGraph, dominators: &DominatorTree) -> Self
    {
        let order = cfg
            .blocks()
            .enumerate()
            .map(|(index, block)| (block, index))
            .collect::<HashMap<_, _>>();
        let mut by_header = HashMap::<BlockId, (HashSet<BlockId>, Vec<BlockId>)>::new();
        for source in cfg.blocks()
        {
            for target in cfg.successors(source)
            {
                if dominators.dominates(*target, source)
                {
                    let entry = by_header
                        .entry(*target)
                        .or_insert_with(|| (HashSet::from([*target]), Vec::new()));
                    entry.1.push(source);
                    collect_natural_loop(cfg, *target, source, &mut entry.0);
                }
            }
        }

        let mut loops = by_header
            .into_iter()
            .map(|(header, (members, mut latches))| {
                let mut blocks = members.into_iter().collect::<Vec<_>>();
                blocks.sort_by_key(|block| order[block]);
                latches.sort_by_key(|block| order[block]);
                latches.dedup();
                let member_set = blocks.iter().copied().collect::<HashSet<_>>();
                let mut exits = blocks
                    .iter()
                    .flat_map(|source| {
                        cfg.successors(*source)
                            .iter()
                            .filter(|target| !member_set.contains(target))
                            .map(|target| (*source, *target))
                    })
                    .collect::<Vec<_>>();
                exits.sort_by_key(|(source, target)| (order[source], order[target]));
                exits.dedup();
                NaturalLoop {
                    header,
                    latches,
                    blocks,
                    exits,
                    parent: None,
                    children: Vec::new(),
                }
            })
            .collect::<Vec<_>>();
        loops.sort_by_key(|natural_loop| order[&natural_loop.header]);
        nest_loops(&mut loops);
        let mut depths = HashMap::<BlockId, usize>::new();
        for (index, natural_loop) in loops.iter().enumerate()
        {
            let depth = loop_depth(&loops, index);
            for block in &natural_loop.blocks
            {
                depths
                    .entry(*block)
                    .and_modify(|current| *current = (*current).max(depth))
                    .or_insert(depth);
            }
        }
        Self {
            loops,
            depths,
        }
    }

    /// Natural loops in header order.
    #[must_use]
    pub fn loops(&self) -> &[NaturalLoop]
    {
        &self.loops
    }

    /// Loop containing `block` with the smallest body.
    #[must_use]
    pub fn innermost_loop(&self, block: BlockId) -> Option<usize>
    {
        self.loops
            .iter()
            .enumerate()
            .filter(|(_, natural_loop)| natural_loop.contains(block))
            .min_by_key(|(_, natural_loop)| natural_loop.blocks.len())
            .map(|(index, _)| index)
    }

    /// Nesting depth, where blocks outside loops have depth zero.
    #[must_use]
    pub fn depth(&self, block: BlockId) -> usize
    {
        self.depths.get(&block).copied().unwrap_or_default()
    }
}

fn collect_natural_loop(cfg: &ControlFlowGraph, header: BlockId, latch: BlockId, members: &mut HashSet<BlockId>)
{
    let mut stack = vec![latch];
    while let Some(block) = stack.pop()
    {
        if !members.insert(block)
        {
            continue;
        }
        if block != header
        {
            stack.extend(cfg.predecessors(block));
        }
    }
}

fn nest_loops(loops: &mut [NaturalLoop])
{
    for child in 0..loops.len()
    {
        let child_blocks = loops[child].blocks.iter().copied().collect::<HashSet<_>>();
        let parent = (0..loops.len())
            .filter(|parent| *parent != child)
            .filter(|parent| child_blocks.iter().all(|block| loops[*parent].contains(*block)))
            .min_by_key(|parent| loops[*parent].blocks.len());
        loops[child].parent = parent;
    }
    for child in 0..loops.len()
    {
        if let Some(parent) = loops[child].parent
        {
            loops[parent].children.push(child);
        }
    }
}

fn loop_depth(loops: &[NaturalLoop], index: usize) -> usize
{
    let mut depth = 1;
    let mut current = loops[index].parent;
    while let Some(parent) = current
    {
        depth += 1;
        current = loops[parent].parent;
    }
    depth
}
