// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

fn identity<T>(value: T) -> T {
    return value;
}

fn hidden<T>(value: T) -> T {
    return value;
}

fn forward<T>(value: T) -> T {
    return hidden::<T>(value);
}

fn first<T>(left: T, right: T) -> T {
    return left;
}

fn main() -> Long {
    value: Long = forward::<Long>(7);
    wide: Int = identity::<Int>(9);
    return first::<Long>(value, 4);
}
