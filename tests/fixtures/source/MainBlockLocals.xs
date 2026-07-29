// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

fn main() -> Long {
  value: Long = 0;
  if (true) {
    delta: Long = 7;
    value = delta;
  }
  while (value < 10) {
    step: Long = 1;
    value += step;
  }
  return value;
}
