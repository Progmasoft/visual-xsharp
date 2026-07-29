/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::Type;

/// Exact-width integer constant represented as an unsigned bit pattern.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct IntegerConstant
{
  /// Signed or unsigned integer type.
  pub value_type: Type,
  /// Width-limited two's-complement bit pattern.
  pub bits: u128,
}

impl IntegerConstant
{
  /// Creates a constant when `bits` fits the requested integer width.
  #[must_use]
  pub const fn new(value_type: Type, bits: u128) -> Option<Self>
  {
    let Some(width) = value_type.integer_width()
    else
    {
      return None;
    };
    if width < 128 && bits >= (1_u128 << width)
    {
      return None;
    }
    Some(Self { value_type,
                bits })
  }

  /// Returns the fixed hexadecimal digit count for canonical text output.
  #[must_use]
  pub fn hexadecimal_digits(self) -> usize
  {
    self.value_type.integer_width().unwrap_or(0) as usize / 4
  }
}
