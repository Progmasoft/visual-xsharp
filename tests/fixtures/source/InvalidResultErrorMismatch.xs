// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

fn Produce() -> Result<Long, Bool>
{
  return Error(true);
}

fn Forward() -> Result<Long, Long>
{
  value: Long = Produce()@;
  return Ok(value);
}

fn main() -> Long
{
  return 0;
}
