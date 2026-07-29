/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

mod literal;
mod utf32;

pub(crate) use literal::{decode_character, format_character};
pub(crate) use utf32::{format_encoded, format_units, parse_encoded, parse_units};
