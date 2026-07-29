// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

data Node {
    next: Node;
    value: Long;
}

fn node_value(node: Node) -> Long {
    return node.value;
}

fn main() -> Long {
    return 0;
}
