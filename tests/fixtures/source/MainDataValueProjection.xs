// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

data Coordinates {
    x: Long;
    y: Long;
}

data Point {
    position: Coordinates;
    weight: Long;
}

fn make_point() -> Point {
    return Point {
        position: Coordinates {
            x: 2,
            y: 3,
        },
        weight: 5,
    };
}

fn main() -> Long {
    return make_point().position.x + make_point().position.y + make_point().weight;
}
