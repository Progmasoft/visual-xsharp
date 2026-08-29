/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

//! Resolution of inherited and overloaded enum-data variants.
//!
//! This module assigns deterministic flattened tags without changing the
//! declaration-local tags retained by structural HIR.

mod error;
mod registry;

pub use error::{EnumDataDiagnostic, EnumDataError, VariantSelectionError};
pub use registry::{EnumDataRegistry, ResolvedVariant};

#[cfg(test)]
mod tests;
