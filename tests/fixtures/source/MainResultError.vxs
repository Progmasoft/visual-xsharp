// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

fn Fail() -> Result<Long, Long>
{
  return Error(13);
}

fn ForwardFailure() -> Result<Long, Long>
{
  value: Long = Fail()@;
  return Ok(value);
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
  return Read(ForwardFailure());
}
