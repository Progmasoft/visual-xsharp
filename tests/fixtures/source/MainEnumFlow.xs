// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

enum Color {
    Red,
    Green,
    Blue,
}

fn next(value: Color) -> Color {
    match (value) {
        Color::Red -> {
            return Color::Green;
        },
        Color::Green -> {
            return Color::Blue;
        },
        else -> {
            return Color::Red;
        },
    }
}

fn main() -> Long {
    color: Color = next(Color::Red);
    if (color == Color::Green) {
        return 7;
    }
    return 1;
}
