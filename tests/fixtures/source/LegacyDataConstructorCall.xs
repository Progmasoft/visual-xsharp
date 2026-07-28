// SPDX-FileCopyrightText: 2026 Leitwolf <xs-lang.chess031@slmails.com>
// SPDX-License-Identifier: MPL-2.0

data Point {
    x: Long;

    Point(x: Long) {
        self.x = x;
    }
}

fn main() -> Long {
    point: Point = Point(7);
    return point.x;
}
