// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

fn main() -> Long {
    value: Long = 0;
    enabled: Bool = true;
    if (enabled) {
        value = 7;
    } else {
        value = 3;
    }
    return value;
}
