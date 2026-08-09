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

fn total(point: Point) -> Long {
    return point.position.x + point.position.y + point.weight;
}

fn main() -> Long {
    point: Point = Point {
        position: Coordinates {
            x: 2,
            y: 3,
        },
        weight: 5,
    };
    replacement: Point = Point {
        position: Coordinates {
            x: 4,
            y: 6,
        },
        weight: 7,
    };
    point = replacement;
    point.position = Coordinates {
        x: 2,
        y: 3,
    };
    point.position.x += point.weight;
    direct: Long = total(Point {
        position: Coordinates {
            x: 1,
            y: 1,
        },
        weight: 1,
    });
    return total(point) + direct;
}
