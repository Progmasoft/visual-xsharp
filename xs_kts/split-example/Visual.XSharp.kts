/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

project("CombinedExample", "BETA", "0.1.0")
set("XS_BACKEND", "LLVM")
set("XS_LLVM_COMPILER", "AOT")
authors(arrayOf("Leitwolf", "leitwolf@example.me"))

source {
  include("sources")
}

compiler {
  warnings("medium")
  werror(false)
  verbose(false)
}
