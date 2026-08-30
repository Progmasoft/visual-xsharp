<!-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com> -->
<!-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0 -->

# Building and testing

The repository has no secondary orchestration wrapper. Bazel is the top-level native build and test interface; Cabal and
Gradle remain the direct build interfaces for their Haskell and Kotlin ownership boundaries.

## Supported host

The supported native development host is Windows. Production C++20 code is compiled with standalone LLVM `clang-cl` under
Bazel and linked with LLD. Visual Studio-bundled build executables are not part of the toolchain.

Required tools:

- Bazelisk;
- ClangCL and LLD from a standalone LLVM installation;
- Windows SDK headers and import libraries;
- MSVC CRT and C++ standard-library development files;
- an LLVM development package containing LLVM headers, libraries, and `llvm-config`;
- GHC 9.10 and Cabal;
- JDK 25; and
- the Kotlin command used by project-evaluator tests.

The Windows SDK and MSVC development files provide platform headers and libraries only. Make their `include`, `lib`, and
tool directories available to the PowerShell build environment; ClangCL, LLD, and Bazelisk remain independent tools.

## LLVM discovery

Do not write a machine-specific LLVM path into the repository. Use one of these mechanisms:

- set `LLVM_ROOT` to an LLVM development installation prefix; or
- put the development package's `llvm-config` on `PATH`.

The Bazel repository rule fails during analysis if it cannot discover a complete LLVM development tree.

## Submodules

Initialize all nested dependencies before configuring:

```powershell
git submodule update --init --recursive
```

The native CLI uses its own typed C++20 command schema. Catch2 remains the only native test submodule.

## Production C++20 build

```powershell
bazelisk build //Compiler/Cli:vxs
bazelisk build //tests:cli_parser_tests
.\bazel-bin\tests\cli_parser_tests.exe
```

The native Catch2 program is executed directly from PowerShell. This avoids introducing a Git Bash/MSYS runtime solely for
Bazel's POSIX-oriented `cc_test` launcher on Windows.

## Language-layer tests

Haskell:

```powershell
Set-Location Compiler
cabal build all
cabal test all
Get-ChildItem Haskell -Filter *.cabal -Recurse | ForEach-Object {
  Push-Location $_.DirectoryName
  try { cabal check } finally { Pop-Location }
}
```

Kotlin:

```powershell
.\ProjectSystem\gradlew.bat -p ProjectSystem test
```

## File-size gate

Implementation, test, build, configuration, and internal source files must remain at or below 1500 lines. Topic-oriented
public `Spec/` example suites are exempt because each file aggregates independent examples.
