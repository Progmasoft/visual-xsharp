// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

fn single_add(a: LFloat, b: LFloat) -> LFloat {
    return a + b;
}

fn single_remainder(a: LFloat, b: LFloat) -> LFloat {
    return a % b;
}

fn double_divide(a: Float, b: Float) -> Float {
    return a / b;
}

fn double_less(a: Float, b: Float) -> Bool {
    return a < b;
}

fn double_not_equal(a: Float, b: Float) -> Bool {
    return a != b;
}

fn main() -> Long {
    if (double_less(1.5, 2.5) && double_not_equal(1.5, 2.5)) {
        return 0;
    } else {
        return 1;
    }
}
