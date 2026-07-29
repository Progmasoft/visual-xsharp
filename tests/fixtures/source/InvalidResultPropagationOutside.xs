// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

fn Produce() -> Result<Long, Long>
{
  return Ok(7);
}

fn main() -> Long
{
  return Produce()@;
}
