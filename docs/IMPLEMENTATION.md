# Implementation status

## Summary

Visual X# has a working compatibility compiler and a tested slice of the intended architecture. These are not yet the same
route.

The current `vxs` executable builds from C23, C++20, and Rust components. It can validate supported `.vxs` source behavior and
produce native artifacts for the subset handled by the Rust compiler core and LLVM backend. The Haskell frontend and C++20
Xpp/Xmm implementation exists and consumes a decoded, verified CorePrep module. The compatibility compiler remains the source
route used by existing production `.vxs` builds, so Haskell-owned CorePrep is not yet the sole production frontend.

## Haskell frontend

The Haskell package exposes separate modules for:

- AST;
- lexer and parser;
- renamer and name resolution;
- type checker;
- desugarer;
- Core and Core optimization;
- CorePrep and CorePrep verification; and
- pipeline diagnostics and orchestration.

The current language slice covers namespace and class declarations, member methods, typed and inferred local bindings,
assignments, calls, returns, conditionals, core operator precedence, entry-point validation, and basic CorePrep control flow.
It does not yet implement the complete language catalog in `Spec/`.

## C++20 middle end

The repository contains:

- verified Haskell and C++20 Core models with a shared bounded `VXCR` `.core` codec contract;
- a native Core semantic verifier and Core-to-CorePrep adapter that atomizes expressions and constructs explicit CFGs;
- matching bounded Haskell and C++20 internal CorePrep wire codecs;
- recursive type, symbol spelling, qualified-name, and UTF-32 string preservation;
- RAM-only CorePrep transport; no CorePrep file extension, reader, writer, CLI input, or emit option exists;
- structural and semantic native CorePrep verifiers;
- CorePrep-to-Xpp lowering;
- Xpp control-flow and self-copy optimization;
- Xpp-to-Xmm lowering; and
- Xmm virtual-register move optimization;
- an Xmm verifier with register, signature, call, operand, result, and control-flow diagnostics;
- C++20 Xmm-to-LLVM lowering for the implemented scalar, call, branch, jump, and return operations;
- LLVM O0/O1/O2/O3 pass-pipeline selection followed by module verification;
- Unicode-scalar `String` constant storage; and
- in-memory LLVM IR and bitcode serialization with explicit `.ll`/`.bc` writers.

The versioned Haskell-to-C++20 boundaries are implemented for both public VXCR Core input and internal VXCP CorePrep input.
The remaining integration work is to make the Haskell frontend the production `.vxs` source route, connect Core input to
native object/link production, and add explicit Xpp/Xmm writers and later-stage readers.

## Rust compiler core

Rust remains an active compiler implementation asset and is built into the native compiler. It provides substantial semantic,
lowering, verification, and test coverage. Migration does not mean deleting Rust or copying its implementation wholesale into
another language. Useful algorithms and tested behavior should be adapted to the target architecture without restoring old
public format names.

## C23 migration

C23 is the implementation layer scheduled for gradual removal. Each subsystem must first acquire an owner in Haskell, C++20,
Rust, or a deliberately retained C ABI shim. The old implementation is removed only after replacement tests, Debug tests, and
sanitizer tests pass.

## CLI and project runtime

The native CLI parser and Kotlin project runtime implement the current command and configuration vocabulary. Source discovery,
entry selection, compiler settings, dependency declarations, and the SQLite lock file have concrete implementations.

Known gaps include:

- Core input can be checked through LLVM and can emit `.ll` or `.bc`, but not yet a native object or executable;
- explicit Core emission from source and Xpp/Xmm emission are registered but not connected;
- Xpp/Xmm and other later non-source `-Build` inputs are registered but not connected;
- package publication and installation require a ViGet client not linked into this build; and
- the production source route does not yet use the Haskell frontend as its sole owner.

## Verification

GitHub CI runs:

- Kotlin project-runtime tests;
- Haskell build, behavior tests, and package checks;
- Rust formatting, tests, and lints;
- Windows ClangCL Debug configure, build, and CTest;
- Windows ClangCL AddressSanitizer configure, build, and CTest; and
- patch hygiene checks.
