// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

fn main() -> Long {
  invalid: Bool = "not a boolean" && true;
  if (invalid) {
    return 1;
  }
  return 0;
}
