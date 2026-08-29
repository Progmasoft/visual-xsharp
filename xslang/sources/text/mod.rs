/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

mod literal;
mod utf32;

pub(crate) use literal::{decode_character, format_character};
pub(crate) use utf32::{format_encoded, format_units, parse_encoded, parse_units};
