/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

//! Optional convenience aliases for Rust consumers.
//!
//! XLIL's structured error types remain the canonical API. These aliases are
//! useful at application boundaries where callers prefer one thread-safe,
//! dynamically dispatched error type.

mod printf;

#[doc(hidden)]
pub use printf::{_printf, PrintfArgument, PrintfArguments, PrintfValue};

/// Thread-safe dynamically dispatched error used by [`XSResult`].
pub type XSError = Box<dyn std::error::Error + Send + Sync + 'static>;

/// Convenience result alias for Rust applications using xslang.
pub type XSResult<T> = std::result::Result<T, XSError>;
