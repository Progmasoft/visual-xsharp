// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

fn factorial(value: Long) -> Long {
  if (value <= 1) {
    return 1;
  }
  return value * factorial(value - 1);
}

fn main() -> Long {
  return factorial(5);
}
