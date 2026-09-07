/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

//! Private value vocabulary retained by the historical MIR algorithms.
//!
//! This is not an artifact format and has no producer, parser, writer, typed
//! facade, or public API. New compiler stages use Core, Xpp, and Xmm.

mod model;
mod operations;
mod type_names;

pub use model::*;
pub use operations::*;
pub use type_names::*;
