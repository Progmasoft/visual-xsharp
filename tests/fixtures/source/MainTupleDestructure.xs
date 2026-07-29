// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

fn values() -> ((Long, Long), Long)
{
  return ((3, 2), 4);
}

fn main() -> Long
{
  ((left, else), right): ((Long, Long), Long) = values();
  (unused, else) := (0, 9);
  return left + right;
}
