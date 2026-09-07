/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1
 */

//! Retained Rust semantic-analysis experiments for Visual X#.

#![warn(missing_docs)]
#![allow(deprecated)]

#[allow(
    missing_docs,
    reason = "existing HIR API documentation is being completed incrementally"
)]
#[deprecated(since = "0.3.5", note = "the production frontend is implemented in Compiler/Haskell")]
pub mod hir;
#[allow(
    missing_docs,
    reason = "existing MIR API documentation is being completed incrementally"
)]
#[deprecated(since = "0.3.5", note = "the production compiler uses Core, Xpp, and Xmm")]
pub mod mir;
#[allow(
    missing_docs,
    reason = "existing monomorphization API documentation is being completed incrementally"
)]
#[deprecated(
    since = "0.3.5",
    note = "production monomorphization is owned by the Haskell frontend"
)]
pub mod mono;
pub(crate) mod text;

// The old XLIL producer and textual artifact surface has been retired. A small
// crate-private structural vocabulary remains only because the retained MIR
// algorithms use its exact-width scalar and aggregate descriptors internally.
pub(crate) mod xlil;

/// Version of the `xslang` crate that built this compiler core.
pub const VERSION: &str = env!("CARGO_PKG_VERSION");
/// Highest XHIR text format version accepted by this release.
pub const XHIR_VERSION: &str = "1";
/// Highest XMIR text format version accepted by this release.
pub const XMIR_VERSION: &str = "1";
