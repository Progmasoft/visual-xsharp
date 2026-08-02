/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

//! Procedural macros for the public Rust XLIL producer API.

#![warn(missing_docs)]

extern crate proc_macro;

mod expand;
mod lower;
mod types;

use proc_macro::TokenStream;

/// Preserves a Rust function and generates a `<name>_xlil` companion producer.
///
/// The initial lowering subset accepts non-generic free functions over `bool`
/// and explicit XLIL Rust value types, integer arithmetic and comparisons, and
/// value-producing `if` expressions with an `else` arm.
#[proc_macro_attribute]
pub fn xlil_create(attribute: TokenStream, item: TokenStream) -> TokenStream
{
  expand::expand(attribute.into(), item.into()).into()
}
