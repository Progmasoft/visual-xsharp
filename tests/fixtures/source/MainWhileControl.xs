// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

fn main() -> Long {
  value: Long = 0;
  while (true) {
    value += 1;
    if (value == 3) {
      continue;
    }
    if (value == 5) {
      break;
    }
  }
  return value + 2;
}
