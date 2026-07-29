// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

fn Produce() -> Result<Long, Long>
{
  return Ok(7);
}

fn Read(input: Result<Long, Long>) -> Long
{
  return match (input)
  {
    Success(value) -> { value },
    Error(error) -> { error },
  };
}

fn main() -> Long
{
  return Read(Produce());
}
