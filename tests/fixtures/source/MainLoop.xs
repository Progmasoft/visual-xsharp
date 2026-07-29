// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

fn main() -> Long {
    value: Long = 0;
    loop {
        value += 1;
        if (value == 7) {
            break;
        }
    }
    return value;
}
