// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

interface Marker {
}

interface DerivedMarker : Marker {
}

class Worker : DerivedMarker {
}

fn constrained<T: Marker>() -> Long {
    return 7;
}

fn main() -> Long {
    return constrained::<Worker>();
}
