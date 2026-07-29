// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

fn sum(values: [Long]) -> Long
{
    total: Long = 0;
    for (value in values)
    {
        total += value;
    }
    return total;
}

fn main() -> Long
{
    values: [Long] = [2, 3, 4];
    values[1] = 5;
    if (values.count != 3)
    {
        return 99;
    }
    return sum(values);
}
