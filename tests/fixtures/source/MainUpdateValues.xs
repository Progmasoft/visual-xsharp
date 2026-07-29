// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

fn main() -> Long {
  value: Long = 5;
  old_increment: Long = value++;
  new_increment: Long = ++value;
  old_decrement: Long = value--;
  new_decrement: Long = --value;
  return old_increment + new_increment + old_decrement + new_decrement;
}
