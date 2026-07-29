// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

data Left {
    value: Long;
}

data Right {
    value: Long;
}

data Combined : Left, Right {
}

fn main() -> Long {
    combined: Combined = Combined {
        value: 1,
    };
    return combined.value;
}
