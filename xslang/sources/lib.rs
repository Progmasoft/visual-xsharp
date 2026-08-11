/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

//! Target-independent semantic analysis, HIR, MIR, and XLIL foundations for Visual X#.

#![warn(missing_docs)]

/// Generates a companion XLIL module producer from a supported Rust function.
///
/// A function named `max` receives a `max_xlil` companion. The original Rust
/// function remains unchanged. This export is available with the
/// `proc-macros` Cargo feature.
#[cfg(feature = "proc-macros")]
pub use xslang_proc_macros::xlil_create;

#[allow(
    missing_docs,
    reason = "existing codegen API documentation is being completed incrementally"
)]
pub mod codegen;
#[allow(
    missing_docs,
    reason = "existing compiler-core API documentation is being completed incrementally"
)]
pub mod compiler_core;
#[allow(
    missing_docs,
    reason = "existing HIR API documentation is being completed incrementally"
)]
pub mod hir;
/// Compatibility queries for the native C++20 compiler-core bridge.
pub mod interop;
#[allow(
    missing_docs,
    reason = "existing MIR API documentation is being completed incrementally"
)]
pub mod mir;
#[allow(
    missing_docs,
    reason = "existing monomorphization API documentation is being completed incrementally"
)]
pub mod mono;
/// Optional Rust integration aliases for application-facing compiler tools.
pub mod rust;
pub(crate) mod text;

/// Public, target-independent XLIL model, producer, reader, verifier, and text API.
pub mod xlil;

/// Version of the `xslang` crate that built this compiler core.
pub const VERSION: &str = env!("XSLANG_BUILD_VERSION");
/// Highest XHIR text format version accepted by this release.
pub const XHIR_VERSION: &str = env!("XSLANG_XHIR_VERSION");
/// Highest XMIR text format version accepted by this release.
pub const XMIR_VERSION: &str = env!("XSLANG_XMIR_VERSION");
/// Highest XLIL text format version accepted by this release.
pub const XLIL_VERSION: &str = env!("XSLANG_XLIL_VERSION");
