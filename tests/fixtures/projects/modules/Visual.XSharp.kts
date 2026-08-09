// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

project("Modules", "BETA", "0.2.1")

source {
  include("Sources")
}

module {
  name("Math")
  members {
    add("Modules/Math/*.vxs")
  }
}
