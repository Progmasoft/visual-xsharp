/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

project("Example", "BETA", "0.1.0")

set("XS_VERSION", "0.2.6")
set("XS_BACKEND", "LLVM")
set("PUBLISH", false)
set("BUILD_MODE", "Release")
set("RELEASE_OUTDIR", "build/release")
set("DEBUG_OUTDIR", "build/debug")

set(
  "TARGET",
  "x86_64-unknown-linux-gnu",
  "x86_64-unknown-linux-musl",
  "aarch64-unknown-linux-gnu",
  "armv7h-unknown-linux-gnueabihf",
  "riscv64gc-unknown-linux-gnu",
  "x86_64-apple-darwin",
  "aarch64-apple-darwin",
  "x86_64-pc-windows-msvc",
  "aarch64-pc-windows-msvc",
  "x86_64-unknown-freebsd",
  "aarch64-unknown-freebsd",
)

println("Target: ${get("TARGET")}")
println("Project: ${get("PROJECT")}")
println("X# version: ${get("XS_VERSION")}")
println("Backend: ${get("XS_BACKEND")}")

authors(
  arrayOf("Leitwolf", "leitwolf@example.me"),
  arrayOf("Helmut", "helmut@example.me"),
)

dependencies {
  addModule("XSharp.JSON", "STABLE", "0.1.0")
  addModule("XSharp.XML", "STABLE", "0.1.0")
  addOptionalModule("TLS", "XSharp.Network", "BETA", "0.1.0")
}

features {
  dependency("XSharp.Network") {
    disable("TLS")
  }
}

source {
  include("Sources")
  exclude("Sources/Tests/**")
}

test {
  include("Sources/Tests")
  framework("xs-test")
}

compiler {
  warnings("all")
  werror(true)
  verbose(false)
}
