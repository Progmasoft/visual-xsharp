/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

//! XLIL v0 construction, inspection, parsing, verification, and canonical text emission.
//!
//! Third-party Rust frontends should normally construct modules with
//! [`Builder`](crate::xlil::Builder), call
//! [`Builder::finish`](crate::xlil::Builder::finish) to verify them, and
//! serialize them with [`module_to_string`](crate::xlil::module_to_string).
//! Readers can use [`parse_module`](crate::xlil::parse_module) and inspect the public
//! model records without depending on LLVM.

mod builder;
mod checked;
mod inspect;
mod io;
mod model;
mod operations;
mod type_names;
/// Type-checked Rust producer facade over the raw XLIL builder.
pub mod typed;
/// Rust value types and helpers that select explicit XLIL value types.
pub mod types;

pub use builder::{BuildError, Builder};
pub use checked::{ModuleError, VerifiedModule, parse_verified};
pub use io::{ModuleIoError, read_module, read_verified, write_module_io, write_verified};
pub use model::*;
pub use operations::*;
pub use type_names::*;

pub(crate) mod lowering;
mod parser;
mod verify;
mod writer;

pub use parser::{Diagnostic as ParseDiagnostic, DiagnosticCode as ParseDiagnosticCode, parse_module};
pub use verify::{Diagnostic as VerifyDiagnostic, DiagnosticCode as VerifyDiagnosticCode, verify_module};
pub use writer::{module_to_string, write_module};

#[cfg(test)]
mod float_operator_tests;
#[cfg(test)]
mod operator_tests;
#[cfg(test)]
mod string_comparison_tests;
#[cfg(test)]
mod unary_tests;
