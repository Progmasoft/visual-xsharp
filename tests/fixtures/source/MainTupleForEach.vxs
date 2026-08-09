// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

fn main() -> Long
{
  values: [((Long, Long), Long); 2] = [((1, 2), 1), ((1, 1), 1)];
  total: Long = 0;
  for (((left, right), tail): ((Long, Long), Long) in values)
  {
    total += left + right + tail;
  }
  right_total: Long = 0;
  for (((else, right), else) in values)
  {
    right_total += right;
  }
  return total + right_total;
}
