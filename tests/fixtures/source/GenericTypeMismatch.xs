// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

fn identity<T>(value: T) -> T {
    return value;
}

fn main() -> Long {
    return identity::<Long>(true);
}
