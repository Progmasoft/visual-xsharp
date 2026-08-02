/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

//! Rust-side values that explicitly select XLIL value types.
//!
//! This module separates host-language storage from the target-independent
//! [`Type`](crate::xlil::Type) registry. Native Rust scalars remain aliases;
//! formats without a stable host arithmetic type retain their exact bits.

mod marker;
mod utf32;
mod value;

pub use marker::{Bool, F16, F32, F64, F128, FloatType, I8, I16, I32, I64, I128, IntegerType, XlilType};
pub use utf32::Utf32Builder;
pub use value::{AnyValue, TypeMismatch, Value};
