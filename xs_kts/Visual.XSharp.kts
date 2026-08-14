/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

project {
  name = "Example"
  version = "0.3.1"
  stability = Stability.BETA
}

compiler {
  version = "0.3.1"
  standard = "26"
  backend = Backend.LLVM
  buildMode = BuildMode.RELEASE
  emit = Emit.BINARY
  warningsAsErrors = true
  warnings = Warnings.ALL
  experimentalWarnings = false
  shadowWarnings = true
  undefinedWarnings = true

  unsafe {
    xppOptimizationPasses = true
    xmmOptimizationPasses = true
    typeSafeFormat = true
  }

  llvm {
    optLevel = LlvmOptLevel.O3
    compiler = LlvmCompiler.AOT
    lto = LlvmLto.THIN
  }
}

outdirs {
  release = "build/release"
  debug = "build/debug"
}

targets {
  target(
    "x86_64-unknown-linux-gnu",
    "aarch64-apple-darwin",
    "x86_64-pc-windows-msvc",
    "aarch64-pc-windows-msvc",
  )
}

authors {
  author("Leitwolf", "leitwolf@example.me")
  author("Helmut", "helmut@example.me")
}

pml {
  enabled = true
}

dependencies {
  dependency("Publisher") {
    name = "Name"
    version = "0.1.0"
    stability = Stability.STABLE
  }
}

sources {
  viget {
    publish = false
    exclude("build/**")
  }

  main {
    srcDir = "Sources"
    entry = "Example.Main"
    exclude("Tests/**")
  }

  test("unit") {
    testDir = "Tests/Unit"
    framework = "tests"
  }
}
