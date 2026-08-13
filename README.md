<!--
SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
SPDX-License-Identifier: MPL-2.0
-->

# Visual X#

Visual X# is an experimental native programming language and compiler project. This repository contains the compiler,
project DSL, runtime components, language examples, tests, and supporting developer tools.

The repository is under an architectural transition. The production `vxs` executable currently combines the established
Rust compiler core with C23 and C++20 components. In parallel, a tested Haskell frontend through CorePrep and a C++20
CorePrep-to-Xpp-to-Xmm-to-LLVM-bitcode slice are being developed and connected. The new route is real code, but it is not yet the sole
production compilation path.

## Intended compiler pipeline

```text
.vxs source
→ Haskell Lexer
→ Haskell Parser
→ Parsed AST
→ Renamer
→ Name Resolution
→ Resolved AST
→ Type Checker
→ Typed AST
→ Desugarer
→ Core
→ Core optimizations
→ CorePrep
→ C++20 Xpp
→ Xpp optimizations
→ C++20 Xmm
→ Xmm optimizations
→ LLVM bitcode
→ native object and .vxse executable
```

Core, CorePrep, Xpp, and Xmm are target-independent. Their public artifact extensions are `.core`, `.xpp`, and `.xmm`;
normal compilation keeps intermediate data in memory unless explicit emission is requested.

## Supported development environment

The supported native build is Windows with:

- Visual Studio 2026 developer environment;
- ClangCL for C23 and C++20;
- LLD and Ninja;
- CMake 3.31 or newer;
- an LLVM development package containing `LLVMConfig.cmake`;
- Rustup and Cargo;
- GHC 9.10 and Cabal for the Haskell frontend;
- JDK 25 and the Kotlin runner for the project DSL; and
- vcpkg for the small native dependency set declared by `vcpkg.json`.

Repository configuration does not contain a machine-specific LLVM installation path. Set `LLVM_ROOT` or `LLVM_DIR`, or
make the LLVM CMake package discoverable through the normal CMake prefix search.

## Build

Initialize the recursive submodules first:

```powershell
git submodule update --init --recursive
```

Install the manifest dependencies using an existing vcpkg installation:

```powershell
& "$env:VCPKG_ROOT\vcpkg.exe" install --triplet x64-windows --x-manifest-root .
```

Configure, build, and test from a Visual Studio 2026 developer terminal:

```powershell
cmake --preset clangcl-debug `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake"
cmake --build --preset clangcl-debug --parallel 4
ctest --preset clangcl-debug --output-on-failure --parallel 2
```

The preset intentionally uses tool names and environment-based discovery rather than absolute installation paths.

## Command-line status

The compiler executable is `vxs`. The current parser recognizes `check`, `build`, `run`, `test`, `resolve`, `update`,
`install`, `viget`, and `version`. Some commands and renewed intermediate input/output selections are registered before their
production implementation is complete. Version `0.3.0` defines verified Haskell Core and a distinct bounded `VXCR` `.core`
artifact contract. Native `.core` consumption remains deliberately disconnected until that contract reaches the C++20
CorePrep boundary; CorePrep wire bytes are never accepted under the public `.core` extension. Core/Xpp/Xmm emission and
non-source `-Build` inputs are not yet connected, while package publication requires a ViGet client that is not linked into
the current compiler build.

The reliable single-file validation form is:

```powershell
.\build\clangcl-debug\vxs.exe check -File .\path\to\Main.vxs
```

See [CLI](docs/CLI.md) for the exact accepted surface and implementation status.

## Project files

A Visual X# project uses one `Visual.XSharp.kts` file. `sources.main.entry` names a namespace-qualified class; its final
segment is a class name and need not be `Main` or `Program`. The selected class must provide a parameterless
`public static void Main()` method; a top-level runtime function is not an entry point.

```kotlin
project {
  name = "Example"
  version = "0.1.0"
  stability = Stability.DEV
}

sources {
  main {
    srcDir = "Sources"
    entry = "Example.Main"
  }
}
```

See [Project files](docs/PROJECT_FILES.md) for the current Kotlin DSL.

## Language specification examples

The [Spec](Spec/README.md) directory contains 24 topic-oriented `.vxs` example suites. They record current language design
intent, including valid and invalid fragments, but they are not concatenated applications and must not be treated as a claim
that every rule is already implemented by `vxs`.

## Documentation

- [Documentation index](docs/README.md)
- [Architecture](docs/ARCHITECTURE.md)
- [Implementation status](docs/IMPLEMENTATION.md)
- [Building and testing](docs/BUILDING.md)
- [CLI](docs/CLI.md)
- [Project files](docs/PROJECT_FILES.md)
- [Specification guide](docs/SPECIFICATION.md)
- [Repository layout](docs/MONOREPO.md)
- [Contributing](docs/CONTRIBUTING.md)

## License

See `LICENSE.txt` and `NOTICE.txt`.
