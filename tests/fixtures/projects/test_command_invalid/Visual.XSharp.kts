// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

project { name = "InvalidTestCommand"; version = "0.2.1"; stability = Stability.BETA }
sources {
  main { srcDir = "Sources"; entry = "InvalidTestCommand.Main"; exclude("Test/**") }
  test("unit") { testDir = "Sources/Test"; framework = "tests" }
}
