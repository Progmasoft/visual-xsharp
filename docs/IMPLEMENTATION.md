# Implementation status

## Summary

Visual X# now has one production source route for the implemented language subset. The C++20 `vxs` driver starts the private
Haskell frontend, receives bounded verified Core, and continues through CorePrep, Xpp, Xmm, and LLVM. The old C lexer/parser,
macro, HIR/MIR, and C-to-Rust syntax-packet route has been removed rather than retained as a fallback.

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

The production process boundary uses public `VXCR` Core. The internal `VXCP` codec remains tested for in-process and golden
contract coverage, but the CLI does not expose CorePrep. Remaining work is native object/link production, multi-unit Haskell
name resolution, and explicit Xpp/Xmm writers and readers.

## Rust compiler core

Rust remains an active implementation and test asset with substantial semantic, lowering, verification, and IR coverage. It
is no longer linked into the native driver, and its obsolete direct-IR C FFI session wrapper has been removed. The underlying
Rust HIR, MIR, XLIL models, algorithms, and tests remain available for selective adaptation.

## C23 migration

The C language frontend and duplicate semantic/middle-end implementation have been removed. Retained C code is limited to
still-used compatibility/package surfaces and is migrated only when a tested replacement exists.

## CLI and project evaluation

The native C++20 CLI uses a declarative typed schema for command scope, option arity, duplicate rejection, defaults, and value
conversion. It has no DIMCLI dependency. The Kotlin project evaluator continues to own project discovery, configuration, and
the SQLite lock file. It is not a second executable: `vxs` starts the evaluator main class from bundled JVM libraries. VXDC
remains a separate command.

Known gaps include:

- Core input can be checked through LLVM and can emit `.ll` or `.bc`, but not yet a native object or executable;
- explicit Core emission from source is connected; Xpp/Xmm emission is not;
- Xpp/Xmm and other later non-source `-Build` inputs are registered but not connected;
- package publication and installation require a ViGet client not linked into this build;
- multi-file project compilation waits for a module-aware Haskell merge; and
- native object/link, `run`, and `test` are intentionally unavailable rather than routed through the removed frontend.

## Verification

GitHub CI runs:

- Kotlin project-evaluator tests;
- Haskell build, behavior tests, and package checks;
- Rust formatting, tests, and lints;
- Windows ClangCL Debug configure, build, and CTest;
- Windows ClangCL AddressSanitizer configure, build, and CTest; and
- patch hygiene checks.
