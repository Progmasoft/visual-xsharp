<!--
SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
SPDX-License-Identifier: MPL-2.0
-->

# Contribution and workflow rules

Visual X# is compiler infrastructure. Changes should preserve the renewed stage boundaries and leave an integrated,
testable system.

## Architecture rules

- Haskell owns the lexer, parser, parsed/resolved/typed ASTs, renamer, name resolution, type checker, desugarer, Core, Core
  optimizations, and CorePrep.
- C++20 owns Xpp, Xmm, their optimization passes, and native backend integration.
- Rust components remain in place unless an explicit design decision replaces their responsibility.
- New language behavior is implemented in the renewed pipeline, not added to a compatibility frontend.
- Replaced C compiler subsystems are deleted; they are not mechanically renamed or translated when their design is obsolete.
- Intentional public C ABI shims remain isolated from compiler ownership and use C-only headers.
- LLVM concepts do not appear in AST, Core, Xpp, or Xmm public models.

## Source rules

- C++20 files use `.cpp` and `.hpp`; C-only headers use `.h`; intentional shared C/C++ headers use `.hh`.
- Use `fmt` for native formatting rather than iostreams.
- Use CMake and Ninja on the supported ClangCL/LLVM path.
- Do not add persistent Unix shell scripts for the Windows development workflow.
- Do not add machine-specific absolute paths to tracked configuration.
- Keep implementation, test, build, configuration, and internal files at or below 1500 lines. Public `Spec/`, generated
  files, and third-party code are exempt.
- Do not rename public APIs as a side effect of moving an implementation between languages.

## Change shape

A compiler-stage change should be coherent enough to prove its boundary. Include the model, pass behavior, verifier,
integration, tests, and public documentation needed for the selected slice. Avoid placeholder-only modules and avoid a long
series of tiny commits that leave the pipeline disconnected.

When an intermediate model changes, update its reader/writer, verifier, optimizer, lowering code, and tests in the same
change. Invalid fixtures should identify the expected rejection; valid fixtures should exercise the integrated route.

## Verification

Run checks in proportion to the changed layer, followed by the integrated native presets:

```text
cmake --preset clangcl-debug
cmake --build --preset clangcl-debug
ctest --preset clangcl-debug --output-on-failure

cmake --preset clangcl-sanitize
cmake --build --preset clangcl-sanitize
ctest --preset clangcl-sanitize --output-on-failure
```

Run Cabal tests for Haskell frontend work, Cargo formatting/tests for Rust work, and Gradle/Kotlin integration tests for DSL
or resolver work. Always run `git diff --check` before publishing.

## Documentation

Public documentation and code examples are written in English. Keep current architecture and current implementation status
separate: an intended stage is not described as implemented until tests exercise it. Historical compatibility names are not
part of the current public compiler catalog.
