// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

fn Maybe() -> Result<Optional<Long>, Long>
{
  return Ok(Some(7));
}

fn Read() -> Result<Long, Long>
{
  value: Optional<Long> = Maybe()@;
  return Ok(value ?? 3);
}

fn Value(input: Result<Long, Long>) -> Long
{
  return match (input)
  {
    Ok(value) -> { value },
    Error(error) -> { error },
  };
}

fn main() -> Long
{
  return Value(Read());
}
