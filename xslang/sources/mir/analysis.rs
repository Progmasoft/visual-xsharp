/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

//! Reusable, target-independent analyses over verified MIR functions.

mod cfg;
mod dominators;
mod effects;
mod liveness;
mod loops;

pub use cfg::{ControlFlowGraph, StronglyConnectedComponent};
pub use dominators::DominatorTree;
pub use effects::{StatementEffects, statement_effects, terminator_uses};
pub use liveness::{BlockLiveness, Liveness};
pub use loops::{LoopForest, NaturalLoop};

#[cfg(test)]
mod tests;
