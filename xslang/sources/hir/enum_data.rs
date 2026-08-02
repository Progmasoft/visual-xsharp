/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
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
