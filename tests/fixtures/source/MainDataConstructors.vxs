// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

data Point {
    x: Long;
    y: Long;

    Point(x: Long, y: Long) {
        self.x = x;
        self.y = y;
    }

    Point(value: Long) {
        self.x = value;
        self.y = value;
    }

    Point(value: Int) {
        self.x = 1;
        self.y = 2;
    }
}

fn total(point: Point) -> Long {
    return point.x + point.y;
}

fn main() -> Long {
    first: Point = new Point(2, 3);
    second_value: Long = 4;
    second: Point = new Point(second_value);
    wide_value: Int = 0;
    third: Point = new Point(wide_value);
    return total(first) + total(second) + total(third);
}
