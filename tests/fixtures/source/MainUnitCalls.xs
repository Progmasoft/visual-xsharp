// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

fn touch(value: Long) {
}

fn identity(value: Long) -> Long {
  return value;
}

fn countdown(value: Long) -> () {
  if (value == 0) {
    return;
  }
  countdown(value - 1);
}

fn main() -> Long {
  touch(1);
  identity(2);
  countdown(3);
  return 7;
}
