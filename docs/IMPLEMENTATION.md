<!-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com> -->
<!-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0 -->

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

Project compilation now enters the Haskell frontend as source roots plus project-relative exclusion patterns. The frontend
owns recursive `.vxs` discovery, strict UTF-8 decoding, canonical root containment, overlapping-root de-duplication, and
stable path ordering. Each physical file is parsed independently. Files declaring the same namespace are merged before the
Renamer, so duplicate declarations and cross-file members share one semantic namespace without requiring directory names to
mirror namespace segments.

Every discovered namespace passes through Renamer, Name Resolution, Type Checker, Desugarer, Core optimization, Core
verification, CorePrep, and CorePrep verification. The configured namespace-qualified class then selects the one Core module
sent over the current private process boundary. Cross-namespace imports and a multi-module Core link unit remain later
semantic work; an unrelated namespace is validated but is not silently folded into the entry namespace.

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
- an Xpp-owned verifier for module/function identity, storage declarations, typed operands, and CFG targets;
- Xpp-to-Xmm lowering; and
- Xmm virtual-register move optimization;
- an Xmm-owned verifier with register, signature, call, operand, result, and control-flow diagnostics, exposed through the
  existing LLVM verification API for compatibility;
- C++20 Xmm-to-LLVM lowering for the implemented scalar, call, branch, jump, and return operations;
- LLVM O0/O1/O2/O3 pass-pipeline selection followed by module verification;
- Unicode-scalar `String` constant storage; and
- in-memory LLVM IR and bitcode serialization with explicit `.ll`/`.bc` writers.

The production process boundary uses public `VXCR` Core. The internal `VXCP` codec remains tested for in-process and golden
contract coverage, but the CLI does not expose CorePrep. LLVM target-machine emission and typed C++20 LLD invocation now
produce `.o`, `.asm`, and `.vxse` artifacts. Remaining work includes cross-namespace Haskell name resolution, a multi-module
Core link unit, source ownership for project-wide per-file artifacts, and explicit Xpp/Xmm writers and readers.

## Retiring Rust compiler core

The Rust compiler core is no longer a supported production layer or a required CI gate. Nothing in the native executable,
Haskell frontend, project evaluator, or Bazel graph links it. Its remaining source tree is transitional reference material:
useful algorithms and tests may be adapted deliberately, but new compiler behavior must be implemented in the owning
Haskell or C++20 layer. The tree will be reduced in reviewed slices rather than becoming a second implementation again.

## C23 migration

The C language frontend and duplicate semantic/middle-end implementation have been removed. Retained C code is limited to
still-used compatibility/package surfaces and is migrated only when a tested replacement exists.

## CLI and project evaluation

The native C++20 CLI uses a declarative typed schema for command scope, option arity, duplicate rejection, defaults, and value
conversion. It has no DIMCLI dependency. The Kotlin project evaluator continues to own project discovery, configuration, and
the SQLite lock file. It is not a second executable: `vxs` starts the evaluator main class from bundled JVM libraries. VXDC
remains a separate command.

`vxs check` without `-File` now evaluates the project, passes its source policy to the private Haskell frontend, resolves the
entry by namespace/class identity, and consumes verified Core through the native LLVM boundary. Project
`build -Emit core|llvmll|llvmbc` uses the same route and writes an artifact named after the entry class. No source file is
guessed from the entry spelling.

Known gaps include:

- Core input can be checked through LLVM and can emit `.ll`, `.bc`, `.o`, `.asm`, or a native `.vxse`;
- explicit Core emission from source is connected; Xpp/Xmm emission is not;
- Xpp/Xmm and other later non-source `-Build` inputs are registered but not connected;
- package publication and installation require a ViGet client not linked into this build;
- cross-namespace imports and the multi-module Core link unit are not connected yet; and
- project-wide per-source object/assembly emission and named test-suite execution remain intentionally unavailable rather
  than violating their output contracts or routing through the removed frontend.

## Verification

GitHub CI runs:

- Kotlin project-evaluator tests;
- Haskell build, behavior tests, and package checks;
- Windows ClangCL Bazel build and native CLI contract tests; and
- patch hygiene checks.
