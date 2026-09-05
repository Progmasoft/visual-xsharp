<!-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com> -->
<!-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0 -->

# Building Visual X#

The repository has one native build graph: Bazel. The portable Go developer command provides host detection, toolchain
diagnostics, native-suite execution, and sanitizer selection without becoming a second graph. Cabal and Gradle remain the
direct build interfaces for their Haskell and Kotlin ownership boundaries.

## Official hosts

Official Visual X# development is supported on:

- Windows 10 and Windows 11, using standalone LLVM `clang-cl` and LLD;
- macOS 15 Sequoia, using Clang/LLD and the Apple SDK; and
- macOS 26 Tahoe, using Clang/LLD and the Apple SDK.

Bazel chooses `windows` or `macos` settings automatically. A developer does not pass `--config=windows` or
`--config=macos`. Visual Studio-bundled compilers are not part of the Windows toolchain, and the full Xcode IDE is not a
macOS prerequisite.

Required tools:

- Bazelisk;
- Clang and LLD from an LLVM installation appropriate to the host;
- an LLVM development package containing LLVM headers, libraries, and `llvm-config`;
- GHC 9.10 and Cabal;
- JDK 25; and
- the Kotlin command used by project-evaluator tests.

Windows additionally needs Windows SDK headers/import libraries and MSVC CRT/STL development files. These provide platform
headers and libraries only; ClangCL, LLD, and Bazelisk remain independent tools. macOS needs the Xcode Command Line Tools,
which provide `xcrun` and the selected Apple SDK.

## LLVM discovery

Do not write a machine-specific LLVM path into the repository. Use one of these mechanisms:

- set `LLVM_ROOT` to an LLVM development installation prefix; or
- put the development package's `llvm-config` on `PATH`.

The Bazel repository rule fails during analysis if it cannot discover a complete LLVM development tree.

The preferred preflight is identical in PowerShell and macOS Terminal:

```powershell
go run scripts/develop.go doctor
```

Doctor recognizes `LLVM_ROOT/bin/llvm-config` even when that directory is intentionally absent from global `PATH`. On
macOS it also validates the OS release and asks `xcrun` for the active SDK.

If `llvm-config` is intentionally not on `PATH`, verify that `LLVM_ROOT` names the installation prefix rather than its
`bin`, `include`, or `lib/cmake/llvm` child. The repository rule derives component paths from the prefix.

## Submodules

Initialize all nested dependencies before configuring:

```powershell
git submodule update --init --recursive
```

The native CLI uses its own typed C++20 command schema. Catch2 remains the only native test submodule.

## Production C++20 build and tests

```powershell
go run scripts/develop.go build
go run scripts/develop.go test
```

The command builds `vxs` and all eight component-owned native suites, then executes each native program directly. This
avoids introducing Git Bash/MSYS solely for Bazel's POSIX-oriented `cc_test` launcher on Windows while retaining the same
suite set on macOS.

Useful component targets for incremental work are:

```powershell
bazelisk build //Compiler/Core:core
bazelisk build //Compiler/Codegen/Xpp:xpp
bazelisk build //Compiler/Codegen/Xmm:xmm
bazelisk build //Compiler/Backend/LLVM:llvm_backend
bazelisk build //Compiler/Linker:native_linker
bazelisk build //Compiler/Driver:core_pipeline
```

Bazel owns only the native graph. It does not replace Cabal for Haskell packages or Gradle for Kotlin configuration and
project evaluation.

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

The repository wrapper pins the Gradle distribution used by the project. Use it instead of a machine-global Gradle command.
JDK 25 must be visible to the wrapper process. Kotlin Script Runner remains an external runtime requirement; it is not
embedded into `vxs`.

## Building the private frontend

The Haskell driver package produces the private frontend consumed by `vxs`. Build all compiler packages together so Cabal
selects one coherent local package set:

```powershell
Push-Location Compiler
try {
  cabal build all
  cabal list-bin exe:vxs-frontend
} finally {
  Pop-Location
}
```

The production/install layout places the frontend relative to `vxs`. The native driver does not search the current project
directory or accept an arbitrary frontend command from `PATH`. When running directly from a build tree, keep the artifacts in
the layout expected by the driver or use the repository's tested build targets rather than copying one binary alone.

## Build configuration

The native graph is configured by tracked Bazel files and a small environment discovery surface:

| Input | Purpose |
| --- | --- |
| `.bazelversion` | Bazelisk-selected Bazel version |
| `.bazelrc` | automatically selected Windows/ClangCL or macOS/Clang options |
| `MODULE.bazel` | module identity and native dependencies |
| `Compiler/Build/Bazel/llvm.bzl` | LLVM development-tree discovery |
| `LLVM_ROOT` | optional LLVM installation prefix |
| `PATH` | discovery of `clang-cl`, `lld-link`, `llvm-config`, and Bazelisk |

`fmt` is a normal Bazel module dependency used by the C++ CLI/driver. Catch2 is pinned through the recursive
`third_party/catch2` submodule and a local module override so native test sources do not depend on registry availability.

The rules_cc patch under `Compiler/Build/Bazel` is a narrow ClangCL toolchain workaround. It is not permission to accumulate
general third-party patches in the compiler tree; remove it when the selected upstream release contains the fix.

## Sanitizers

Do not memorize Bazel profile names or manually combine compile/link flags. Select the diagnostic by purpose:

```powershell
go run scripts/develop.go sanitize address
go run scripts/develop.go sanitize undefined
go run scripts/develop.go sanitize thread
```

AddressSanitizer is available on both official host families. UndefinedBehaviorSanitizer and ThreadSanitizer are exposed on
macOS; requesting either on Windows fails before a build and explains the supported alternative. The command rebuilds all
native suites with matching compiler and linker instrumentation, sets fail-fast runtime options, identifies the exact suite
being executed, and returns a nonzero status at the first violation. It accepts `asan`, `ubsan`, and `tsan` as convenient
aliases.

Advanced Bazel options may follow a `--` separator, for example
`go run scripts/develop.go sanitize address -- --jobs=4`. Platform and sanitizer `--config` values are deliberately
owned by the command so compile and link instrumentation cannot accidentally diverge.

## Clean rebuilds

Generated trees are disposable. Prefer deleting only the build system's known output rather than source or workspace roots:

```powershell
bazelisk clean
Push-Location Compiler
try { cabal clean } finally { Pop-Location }
.\ProjectSystem\gradlew.bat -p ProjectSystem clean
```

For the native-only cleanup, `go run scripts/develop.go clean` performs an expunging Bazel clean.

Do not commit `bazel-*`, Cabal `dist-newstyle`, Gradle `.gradle`/`build`, IDE caches, local service state, or compiler-emitted
artifacts used only for smoke tests. Before removing a large tree manually, resolve the exact absolute target and verify that
it is a generated directory inside this checkout.

## Troubleshooting

### LLVM development files are not found

An LLVM runtime installation containing only executables is insufficient. The native backend needs C++ headers, LLVM
libraries, bitcode/support/target components, and `llvm-config` metadata. Point `LLVM_ROOT` at a complete development prefix
or expose that prefix's `llvm-config` on `PATH`. Do not add `LLVM_DIR` with a personal absolute path to tracked files.

### Windows headers or CRT libraries are not found

ClangCL is the compiler, but Windows SDK and MSVC CRT/C++ development files still provide platform headers and import/static
libraries. Install those components and start a PowerShell environment that exposes their include and library directories.
The Visual Studio IDE and Visual Studio's compiler executable are not part of the Visual X# build contract.

### `clang_rt` or runtime DLL is missing

Treat a missing compiler runtime as a toolchain-layout problem. Confirm the selected ClangCL resource directory and the
runtime libraries shipped with the same LLVM version. Do not copy a random DLL beside every test executable or make a build
depend on a temporary directory.

### Private frontend is missing

Build the Haskell workspace and confirm the frontend is placed in the layout expected by `vxs`. Installing a same-named
command globally is not a supported fix because it could mismatch the Core wire contract.

### `xs_lil.dll` or another retired library is requested

The production pipeline must not require retired compatibility DLLs. A request for one indicates a stale executable,
generated build tree, or old installation. Clean the owning generated output and rebuild the current Bazel/Cabal graph.

### Native test launcher requests a POSIX shell

Build the Catch2 test target with Bazel and execute the resulting `.exe` directly from PowerShell as shown above. A Git Bash
or fish installation is not a repository prerequisite.

### Link succeeds but `run` starts an old program

This is a compiler bug, not an accepted workflow. The current driver validates the artifact produced by the current
invocation. Capture the command, output path, and diagnostic, then add a native regression test; do not work around it by
pre-deleting unrelated source.

## File-size gate

Implementation, test, build, configuration, and internal source files must remain at or below 1500 lines. Topic-oriented
public `Spec/` example suites are exempt because each file aggregates independent examples.

See [Testing and verification](TESTING.md) for the full test matrix, workflow ownership, formatting checks, smoke tests, and
documentation validation.
