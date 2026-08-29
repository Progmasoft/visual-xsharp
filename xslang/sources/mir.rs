/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

mod model;

pub use model::*;

/// Control-flow, dominance, loop, and local-liveness analyses for MIR.
pub mod analysis;
pub mod optimizer;
pub mod text;
pub mod verify;

#[cfg(test)]
mod operator_optimizer_tests;
