// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

fn main() -> Long {
  value: Long = 2;
  result: Long = 0;
  match (value) {
    0 -> {
      result = 1;
    },
    2 -> {
      result = 7;
    },
    else -> {
      result = 3;
    },
  }
  return result;
}
