// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

project { name = "ShouldPanicTestCommand"; version = "0.2.1"; stability = Stability.BETA }
sources {
  main { srcDir = "Sources"; entry = "ShouldPanicTestCommand.Main"; exclude("Test/**") }
  test("unit") { testDir = "Sources/Test"; framework = "tests" }
}
