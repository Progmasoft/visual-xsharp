// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

fn main() -> Long {
  sum: Long = 0;
  for (i: Long = 0; i < 5; i++) {
    if (i == 2) {
      continue;
    }
    sum += i;
  }
  return sum;
}
