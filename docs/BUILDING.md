<!-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com> -->
<!-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0 -->

# Building and testing

The optional top-level `justfile` composes the existing CMake and Gradle owners without replacing either build system:

```powershell
just check
just check sanitize
just format-kotlin
just format-native xs/sources/driver/Cli.cpp
```

The recipes intentionally remain thin. The commands below are still the canonical native and Kotlin build interfaces.

## Supported host

The supported native development host is Windows. Retained C compatibility libraries and C++20 code are compiled with a
standalone LLVM `clang-cl`; standalone Ninja is the supported CMake generator and LLD is the linker. Visual Studio-bundled
build executables are not part of the toolchain.

Required tools:

- Kitware CMake 3.31 or newer;
- standalone Ninja;
- ClangCL and LLD from a standalone LLVM installation;
- Windows SDK headers and import libraries;
- MSVC CRT and C++ standard-library development files;
- an LLVM development package containing `LLVMConfig.cmake` and the LLVM C library;
- vcpkg;
- Rustup and Cargo;
- GHC 9.10 and Cabal;
- JDK 25; and
- the Kotlin command used by project-evaluator tests.

The Windows SDK and MSVC development files provide platform headers and libraries only. Make their `include`, `lib`, and
tool directories available to the PowerShell build environment; CMake, Ninja, ClangCL, and LLD still come from the
independent installations listed above.

## LLVM discovery

Do not write a machine-specific LLVM path into the repository. Use one of these mechanisms:

- set `LLVM_DIR` to the directory containing `LLVMConfig.cmake`;
- set `LLVM_ROOT` to an LLVM development installation prefix; or
- expose the package through `CMAKE_PREFIX_PATH`.

The CMake build fails at configuration time if the LLVM package or LLVM C library cannot be found.

## Submodules

Initialize all nested dependencies before configuring:

```powershell
git submodule update --init --recursive
```

The native CLI uses its own typed C++20 command schema. Catch2 remains the only native test submodule.

## Native dependencies

The vcpkg manifest provides a minimal LibArchive build with zstd support. LLVM is not built through vcpkg.

```powershell
& "$env:VCPKG_ROOT\vcpkg.exe" install `
  --triplet x64-windows `
  --x-manifest-root .
```

## Debug build

```powershell
cmake --preset clangcl-debug `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake"
cmake --build --preset clangcl-debug --parallel 4
ctest --preset clangcl-debug --output-on-failure --parallel 2
```

## Sanitizer build

```powershell
cmake --preset clangcl-sanitize `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake"
cmake --build --preset clangcl-sanitize --parallel 4
ctest --preset clangcl-sanitize --output-on-failure --parallel 2
```

The sanitizer configuration dynamically discovers the Clang runtime directory and copies the required AddressSanitizer DLL
beside test executables.

## Language-layer tests

Haskell:

```powershell
Set-Location xs
cabal build all
cabal test all
Set-Location haskell\visual-xsharp-compiler
cabal check
```

Rust:

```powershell
Set-Location xslang
cargo +nightly-2026-07-10 fmt --check
cargo +nightly-2026-07-10 test
cargo +nightly-2026-07-10 clippy -- -D warnings
```

Kotlin:

```powershell
.\vxs_kts\gradlew.bat -p vxs_kts test
```

## File-size gate

Implementation, test, build, configuration, and internal source files must remain at or below 1500 lines. Topic-oriented
public `Spec/` example suites are exempt because each file aggregates independent examples.
