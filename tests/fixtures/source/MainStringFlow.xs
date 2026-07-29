// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

fn identity(value: &Str) -> &Str {
  return value;
}

fn greeting() -> &Str {
  value: &Str = "Leitwolf";
  return identity(value);
}

fn main() -> Long {
  return 0;
}
