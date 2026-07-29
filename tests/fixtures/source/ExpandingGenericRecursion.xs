// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

fn grow<T>(value: T) -> T {
  return grow::<[T]>([value]);
}

fn main() -> Long {
  return grow::<Long>(7);
}
