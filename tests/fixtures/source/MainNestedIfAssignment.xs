// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

fn main() -> Long {
    value: Long = 0;
    outer: Bool = true;
    inner: Bool = false;
    if (outer) {
        if (inner) {
            value = 1;
        } else {
            value = 7;
        }
    } else {
        value = 3;
    }
    return value;
}
