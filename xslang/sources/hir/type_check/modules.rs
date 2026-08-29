/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

mod binary_type;
mod block_check;
mod collection_check;
mod expression_type;
mod for_check;
#[cfg(test)]
mod for_each_tests;
mod match_check;
mod nominal_check;
#[cfg(test)]
mod nominal_tests;
mod result_type;
mod tuple;
mod type_semantics;
mod unary_type;

pub(crate) use result_type::result_type_parts;
use type_semantics::literal_default_type;
pub use type_semantics::{ValueOwnership, literal_matches_type};
