// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

fn Produce() -> Result<Long, Long>
{
  return Ok(5);
}

fn Forward() -> Result<Long, Long>
{
  value: Long = Produce()@;
  return Ok(value + 2);
}

fn Read(input: Result<Long, Long>) -> Long
{
  return match (input)
  {
    Ok(value) -> { value },
    Error(error) -> { error },
  };
}

fn main() -> Long
{
  return Read(Forward());
}
