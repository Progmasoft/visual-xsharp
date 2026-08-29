/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

pub(crate) mod aggregate_registry;
#[cfg(test)]
mod aggregate_registry_tests;
pub mod async_check;
pub(crate) mod collection_registry;
pub mod declarations;
pub mod enum_data;
pub mod generics;
pub mod inference;
pub mod match_model;
pub mod mir_lowering;
pub mod result_desugar;
pub mod send_sync;
pub mod symbols;
pub mod text;
pub mod type_check;

pub use async_check::Span;
pub use match_model::{MatchArm, MatchPattern};
pub use type_check::*;
