<!--
SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
SPDX-License-Identifier: MPL-2.0
-->

# Build and test guide

The supported native build uses Windows, the Visual Studio 2026 developer environment, ClangCL, CMake, Ninja, LLVM, and
LLD. Compiler code is being renewed as Haskell through CorePrep and C++20 from Xpp onward. Rust and Kotlin toolchains build
their existing monorepo components.

## Required tools

- Visual Studio 2026 with the MSVC SDK and developer command environment
- CMake 3.31 or newer and Ninja
- ClangCL and a matching LLVM development archive, including LLVM CMake package files and LLD
- libarchive, zstd, and fmt development packages
- Rustup and Cargo with the toolchain selected by `xslang/rust-toolchain.toml`
- GHC and Cabal for the Haskell workspace
- JRE 25 and the Kotlin script runner for project-system tests

Clone or initialize the pinned dependencies before configuring:

```text
git submodule update --init --recursive
```

Dependency roots are supplied through CMake discovery variables or the environment. Repository presets do not contain
machine-specific absolute installation paths. A local vcpkg installation may provide small binary dependencies, but LLVM is
discovered from an installed development archive rather than built through vcpkg.

## Debug build

Run these commands in a Visual Studio 2026 developer terminal:

```text
cmake --preset clangcl-debug
cmake --build --preset clangcl-debug
ctest --preset clangcl-debug --output-on-failure
```

The preset selects Ninja, `clang-cl` for C and C++, Debug configuration, the `xs` project, and the `xsrt` runtime. The build
directory is `build/clangcl-debug`.

If LLVM or another package is installed outside the default search locations, pass its prefix at configure time or define a
stable environment root. Do not add a workstation path to `CMakePresets.json` or a tracked CMake file.

## AddressSanitizer build

```text
cmake --preset clangcl-sanitize
cmake --build --preset clangcl-sanitize
ctest --preset clangcl-sanitize --output-on-failure
```

The sanitizer configuration derives the Clang resource directory from the selected compiler. It links the matching dynamic
AddressSanitizer runtime and places the runtime DLL beside test executables. Windows ASan uses the release DLL CRT, so the
imported fmt C++ target uses its Release binary even though project-owned code retains Debug symbols and checks. Because
Rust/CXX and binary dependencies are not instrumented by this CMake option, the build disables MSVC STL container annotations
uniformly while retaining ASan instrumentation for project-owned C and C++ code.

Do not impose a small virtual-memory limit on an AddressSanitizer run; its shadow-memory reservation is intentionally large.

## Haskell frontend

The Haskell workspace is rooted by `xs/cabal.project`. Use the pinned package plan and run the component tests when a module
from the lexer through CorePrep changes. Native integration is still required after Haskell-only tests pass because the final
contract crosses into C++20 Xpp.

## Kotlin project system

The project system evaluates one `Visual.XSharp.kts` per project. The Gradle wrapper builds its JVM distribution and tests:

```text
xs_kts\gradlew.bat --daemon --build-cache -p xs_kts test installDist
```

JVM-labelled CTests require JRE 25 and the Kotlin script runner on `PATH`. They are serialized when they share the script
runner to avoid startup races on constrained CI workers.

## Installation layout

Install the compiler component into a staging prefix with:

```text
cmake --install build/clangcl-debug --prefix C:\Temp\visual-xsharp --component compiler
```

The component contains `vxs`, compiler-owned public headers, intentional C ABI headers, license notices, and the runtime
files required by the selected configuration. CMake rejects colliding public-header destinations.

## Project selection

```text
cmake --preset clangcl-debug -DXS_ENABLE_PROJECTS=xs
cmake --preset clangcl-debug -DXS_ENABLE_PROJECTS=all
```

Projects that are not yet buildable fail configuration explicitly instead of silently producing an incomplete package.

## Repository checks

```text
git diff --check
rg -n "\\bNULL\\b|#include <stdbool\\.h>" xs xsrt tests include
```

Implementation, test, build, configuration, and internal files must not exceed 1500 lines. `Spec/`, third-party sources, and
generated files are outside this limit. Build directories and installed dependency trees are generated state and must not be
committed.
