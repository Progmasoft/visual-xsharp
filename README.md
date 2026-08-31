<!--
SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
-->

# Visual X#

Visual X# is an experimental native programming language and compiler project. This repository contains the compiler,
project DSL, runtime components, language examples, tests, and supporting developer tools.

The repository is under an architectural transition. The production `.vxs` route now uses the Haskell lexer-through-Core
frontend and hands a verified `VXCR` Core artifact to the C++20 CorePrep-to-Xpp-to-Xmm-to-LLVM pipeline. The previous C
lexer/parser, semantic tree, macro, HIR/MIR duplicate, Rust FFI session bridge, and DIMCLI dependency have been removed.
The remaining Rust tree is transitional reference material, not a production dependency or a second supported compiler.

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

- Bazelisk for the production C++20 graph;
- ClangCL and LLD from an LLVM installation;
- Windows SDK headers and import libraries;
- MSVC CRT and C++ standard-library development files;
- an LLVM development package containing headers, libraries, and `llvm-config`;
- GHC 9.10 and Cabal for the Haskell frontend;
- JDK 25 and the Kotlin runner for the project DSL.

Repository configuration does not contain a machine-specific LLVM installation path. Set `LLVM_ROOT` or put
`llvm-config` on `PATH`.

## Build

Initialize the recursive submodules first:

```powershell
git submodule update --init --recursive
```

Build the production C++20 compiler and its first native contract suite from PowerShell after making the Windows SDK and
MSVC library/include directories available in the environment:

```powershell
bazelisk build //Compiler/Cli:vxs
bazelisk build //Compiler/Cli/Tests:cli_parser_tests
.\bazel-bin\tests\cli_parser_tests.exe
```

The Bazel graph discovers LLVM from `LLVM_ROOT` or `llvm-config`; it does not store a machine-specific installation path.

## Command-line status

The compiler executable is `vxs`. Its C++20 command parser uses one typed schema for command scope, option arity, duplicate
rejection, defaults, and value conversion; it has no third-party CLI dependency. For `.vxs`, `check` runs the Haskell
frontend and the complete in-memory Core/CorePrep/Xpp/Xmm/LLVM validation route. `build` produces a native `.vxse` by
default; `-Emit core|object|assembly|llvmll|llvmbc` selects another supported artifact. `run` builds and executes the
native binary. Public Xpp/Xmm readers and writers remain later work.
CorePrep wire bytes are never accepted under the public `.core` extension.

The reliable single-file validation form is:

```powershell
.\bazel-bin\Compiler\Cli\vxs.exe check -File .\path\to\Main.vxs
```

From a directory containing `Visual.XSharp.kts`, project validation uses the configured source roots and namespace-qualified
entry directly:

```powershell
.\path\to\vxs.exe check
.\path\to\vxs.exe build -Emit core
.\path\to\vxs.exe format
.\path\to\vxs.exe lint
```

The compiler recursively discovers case-sensitive `.vxs` files, applies project-relative exclusions, merges files by
declared namespace, validates every namespace, and selects the configured entry class. It does not require namespace and
directory layouts to match.

Project-wide `format` and `lint` reuse that compiler-owned source discovery. They require the separately installed Visual
Formatter and Visual Linter packages, respectively; the compiler does not embed either tool.

See [CLI](Documents/CLI.md) for the exact accepted surface and implementation status.

## Project files

A Visual X# project uses one `Visual.XSharp.kts` file. `sources.main.entry` names a namespace-qualified class; its final
segment is a class name and need not be `Main` or `Program`. The selected class must provide a parameterless
`public static void Main()` method; a top-level runtime function is not an entry point.
The entry is resolved from namespace and type identity. It does not name a source file, and source file names or directory
layout do not have to mirror the namespace. The Kotlin runtime passes source roots and exclusions to the compiler without
walking the project for `.vxs` files. The Haskell frontend now owns that discovery, strict UTF-8 decoding, deterministic
ordering, exclusion matching, namespace merge, and entry validation.

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

See [Project files](Documents/PROJECT_FILES.md) for the current Kotlin DSL.

The binary project lockfile can be inspected without changing the normal project-evaluation output. The separate `vxdc`
tool evaluates `Visual.XSharp.kts` and writes deterministic, replayable SQL to the requested output path. VXDC does not
require a particular output filename extension.

## Language specification examples

The [Spec](Spec/README.md) directory contains 24 topic-oriented `.vxs` example suites. They record current language design
intent, including valid and invalid fragments, but they are not concatenated applications and must not be treated as a claim
that every rule is already implemented by `vxs`.

## Documentation

- [Documentation index](Documents/README.md)
- [Architecture](Documents/ARCHITECTURE.md)
- [Compiler pipeline](Documents/COMPILER-PIPELINE.md)
- [Implementation status](Documents/IMPLEMENTATION.md)
- [Building](Documents/BUILDING.md)
- [Testing](Documents/TESTING.md)
- [CLI](Documents/CLI.md)
- [Project files](Documents/PROJECT_FILES.md)
- [Diagnostics](Documents/DIAGNOSTICS.md)
- [Ecosystem tools](Documents/ECOSYSTEM.md)
- [Specification guide](Documents/SPECIFICATION.md)
- [Repository layout](Documents/MONOREPO.md)
- [Contributing](Documents/CONTRIBUTING.md)

## License

Project-owned source files use `MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0`. The exception permits static and
dynamic linking with independent components under licenses of their choice, including proprietary licenses, without
removing MPL-2.0 obligations from covered files or modifications to those files. See `LICENSE.txt`,
`LICENSES/AdditionRef-Progmasoft-Exception-1.0.txt`, `PATENTS`,
`LICENSES/AdditionRef-Progmasoft-Patent-Grant-1.0.txt`, and `NOTICE.txt`.
