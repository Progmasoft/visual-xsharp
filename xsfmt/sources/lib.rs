/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

mod config;
mod formatter;

pub use config::{Config, ConfigError, NewlineStyle};
pub use formatter::{FormatResult, format_source};
