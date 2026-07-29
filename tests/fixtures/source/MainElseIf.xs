// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

fn main() -> Long {
  value: Long = 2;
  if (value == 0) {
    return 1;
  }
  else if (value == 1) {
    return 2;
  }
  else if (value == 2) {
    return 7;
  }
  else {
    return 3;
  }
  return 0;
}
