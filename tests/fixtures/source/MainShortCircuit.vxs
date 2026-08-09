// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

fn fail_if_called() -> Bool {
  return 1 / 0 == 0;
}

fn main() -> Long {
  left: Bool = false && fail_if_called();
  right := true || fail_if_called();
  if (!left && right) {
    return 7;
  }
  return 1;
}
