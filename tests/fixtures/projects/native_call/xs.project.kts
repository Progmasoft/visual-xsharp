// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

project("NativeCall", "BETA", "0.2.1")

dependencies {
  addModule("JSON", "stable", "0.1.0")
}

source {
  include("sources")
}
