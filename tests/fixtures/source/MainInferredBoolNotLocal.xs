// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

fn main() -> Long {
  limit := 4;
  ok := !(limit < 4);
  return if (ok) { 7 } else { 2 };
}
