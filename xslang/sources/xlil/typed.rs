/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

//! Type-checked Rust producer facade over [`crate::xlil::Builder`].
//!
//! The facade keeps XLIL as the single model and verifier implementation. It
//! adds Rust marker types to common producer operations and can always expose
//! or return the underlying raw builder.

mod builder;
mod error;
mod signature;
mod slot;

pub use builder::TypedBuilder;
pub use error::TypedBuildError;
pub use signature::Signature;
pub use slot::Slot;

#[cfg(test)]
mod tests;
