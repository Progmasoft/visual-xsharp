// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

data Point
{
  x: Long;
  y: Long;
}

fn main() -> Long
{
  point: Point? = nil;
  return point?.y ?? 13;
}
