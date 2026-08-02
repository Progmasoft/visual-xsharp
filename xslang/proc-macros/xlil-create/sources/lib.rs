/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

//! Procedural macros for the public Rust XLIL producer API.

#![warn(missing_docs)]

extern crate proc_macro;

mod config;
mod expand;
mod infer;
mod lower;
mod types;

use proc_macro::TokenStream;

/// Preserves a Rust function and generates module and canonical-text producers.
///
/// The lowering subset accepts non-generic free functions over `bool` and
/// explicit XLIL numeric types. It supports immutable locals, integer and
/// floating-point operations, value-producing conditionals, and short-circuit
/// boolean control flow. The optional `module`, `producer`, and `text`
/// string arguments configure generated names.
#[proc_macro_attribute]
pub fn xlil_create(attribute: TokenStream, item: TokenStream) -> TokenStream
{
    expand::expand(attribute.into(), item.into()).into()
}
