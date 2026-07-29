// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

fn main() -> Long {
  value: Long = 2;
  result: Long = match (value) {
    0 -> { 1 },
    2 -> { 7 },
    else -> { 3 },
  };
  return result;
}
