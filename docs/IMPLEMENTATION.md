<!--
SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
SPDX-License-Identifier: MPL-2.0
-->

# Current compiler implementation

Visual X# is moving from its compatibility compiler to the architecture described in
[ARCHITECTURE.md](ARCHITECTURE.md). The destination is Haskell through CorePrep and C++20 from Xpp onward. Rust services are
retained; C compiler code is retired as its owning stage is replaced and verified.

This page distinguishes implemented infrastructure from the destination architecture. A documented stage is not presented
as complete merely because its module or data model exists.

## Buildable system today

- The monorepo root selects projects and runtimes through CMake.
- `vxs` is the compiler command and `.vxs` is the Visual X# source suffix.
- Native executable artifacts use `.vxse`.
- ClangCL, LLVM tools, LLD, CMake, and Ninja form the supported Windows toolchain.
- The compiler links the existing Rust semantic core and the Kotlin project-system runtime.
- The native LLVM path can emit, link, and execute the currently supported source subset.
- Debug and AddressSanitizer builds are separate CMake presets.
- The test registry covers native libraries, source fixtures, direct artifacts, the Kotlin DSL, and package behavior.

## Renewed frontend

The Haskell workspace owns the modules from `Visual.XSharp.Lexer` through `Visual.XSharp.Core.CorePrep`. The layer sequence
and ownership rules are defined in [ARCHITECTURE.md](ARCHITECTURE.md).

The connection is incremental, but the models are not collapsed for convenience:

- parsed, resolved, and typed ASTs remain separate;
- renaming and name resolution remain separate passes;
- type checking does not happen in the parser;
- desugaring completes before Core is formed;
- CorePrep is the final Haskell-owned stage;
- diagnostics and source spans survive across every boundary.

The Haskell modules must expose real pass inputs and outputs before the old stage they replace is removed. Placeholder module
names alone are not considered an implementation milestone.

## Xpp and Xmm

Xpp and Xmm are C++20, target-independent compiler layers. They remain in memory in normal builds and use `.xpp` and `.xmm`
only for an explicit artifact request.

Each layer requires:

- an owned data model;
- construction and validation APIs;
- a verifier that rejects malformed input before optimization;
- deterministic optimization passes;
- diagnostics that identify the responsible source or generated unit;
- focused unit tests and end-to-end pipeline tests;
- a versioned reader/writer only where explicit artifact emission requires one.

Xmm optimization is enabled by default for both project and direct-file compilation. The LLVM backend consumes verified Xmm;
it does not reach backward into Core, AST, or source parsing.

## Project system and command line

Each project has one `Visual.XSharp.kts`. The DSL declares project identity, source roots, the class-based entry point,
dependencies, compiler options, and output policy. Standard-library facilities do not need to be restated as project
dependencies.

The command-line driver and Kotlin DSL share one current configuration model. Removed setters, aliases, input formats, and
compatibility defaults are not reintroduced into the renewed surface. `-Emit` controls explicit intermediate output; without
it, the compiler keeps Core, Xpp, and Xmm in memory.

See [PROJECT_FILES.md](PROJECT_FILES.md) and [CLI.md](CLI.md) for the public contracts.

## Runtime and packages

The runtime remains a selectable monorepo runtime with a deliberately small C ABI where interoperability requires it. A C
ABI does not require the compiler implementation behind it to remain written in C.

Package archives are deterministic `.xspkg.tar.zst` containers. Archive verification rejects unsafe paths, duplicates,
unsupported entry types, and resource-limit violations. Windows cryptographic operations use the operating-system API.
Registry, lockfile, authentication, and publication behavior are documented separately from compiler IR details.

## LLVM backend

The LLVM backend owns:

- LLVM context and module lifetime;
- target triple, target machine, and data layout;
- Xmm-to-LLVM lowering;
- LLVM verification and optimization;
- object and assembly emission;
- LLD-based native linking;
- backend diagnostics.

No LLVM type or handle belongs in the Haskell AST/Core models or the C++20 Xpp/Xmm public models. See
[LLVM_BACKEND.md](LLVM_BACKEND.md) for backend-specific status.

## C retirement policy

C removal is driven by ownership, not mechanical extension changes. Replaced frontend, semantic, and obsolete intermediate
subsystems are deleted once the renewed path passes their relevant tests. Remaining driver, package, backend, and runtime
implementation is moved to C++20 where C++ ownership is appropriate. Intentional public C ABI shims may remain small and
isolated.

The removal gates are:

1. Haskell lexer/parser and separated AST passes accept the supported language fixtures.
2. Name resolution, type checking, desugaring, Core, and CorePrep preserve diagnostics and entry-point semantics.
3. CorePrep connects to verified C++20 Xpp and Xmm models.
4. Xmm reaches LLVM bitcode or VPI without a fallback through an old compiler stage.
5. Debug, sanitizer, Kotlin project, package, and native artifact tests pass on the replacement route.

Old C compiler files are not translated when their design is no longer part of this route. This keeps migration work focused
on the renewed compiler rather than preserving historical layers under a different source extension.

## Source and repository rules

- New compiler implementation is Haskell through CorePrep and C++20 from Xpp onward.
- Rust code remains unless its owning design is explicitly replaced.
- C-only headers use `.h`, C++-only headers use `.hpp`, and intentional shared C/C++ headers use `.hh`.
- Implementation, tests, build files, configuration, and internal notes stay at or below 1500 lines per file. Public `Spec/`
  material and third-party/generated files are exempt.
- New public documentation is written in English.
- Generated files and local dependency/build trees are not committed.

## Verification

The minimum native verification sequence is:

```text
cmake --preset clangcl-debug
cmake --build --preset clangcl-debug
ctest --preset clangcl-debug --output-on-failure

cmake --preset clangcl-sanitize
cmake --build --preset clangcl-sanitize
ctest --preset clangcl-sanitize --output-on-failure
```

Language-specific Haskell, Rust, and Kotlin checks are run when their layers change. A migration milestone is complete only
when the integrated native route and its CI checks pass.
