<!-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com> -->
<!-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0 -->

# Implementation status

## Summary

Visual X# now has one production source route for the implemented language subset. The C++20 `vxs` driver starts the private
Haskell frontend, receives bounded verified Core, and continues through CorePrep, Xpp, Xmm, and LLVM. The old C lexer/parser,
macro, HIR/MIR, and C-to-Rust syntax-packet route has been removed rather than retained as a fallback.

## Status vocabulary

| State | Meaning in this document |
| --- | --- |
| Connected | reachable through a supported command and covered by the owning tests |
| Implemented | code and focused tests exist, although the public route may stop earlier |
| Partial | a deliberately bounded subset is connected and unsupported cases fail explicitly |
| Registered | a public spelling/model exists, but execution reports that it is not connected |
| Planned | design direction only; programs cannot depend on it |
| Legacy | retained outside the production graph; no new compiler behavior belongs there |

The distinction matters most for the specification. A language rule can be designed in `Spec/`, modeled in an AST, and
still remain unavailable in native emission because a later representation has no layout or ABI contract.

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

### Frontend coverage boundaries

The connected frontend is strongest around scalar expressions, local control flow, calls, callable literals through
CorePrep, namespace merging, and entry validation. Fixed-width integer/radix/separator behavior, character packing, numeric
boolean context, source `void`, stable `SymbolId` identity, and constant range checks are represented before Core emission.

The full `Spec/` catalog is not implemented. Object/value layout, the complete standard-library surface, cross-namespace
imports, full generic/template behavior, exception lowering, ownership runtime operations, generators, FFI, assembly, and
many advanced declaration forms require additional semantic and native work. Unsupported forms must produce frontend or
backend diagnostics; they must not be approximated with C-family behavior.

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

### Native coverage matrix

| Capability | Status | Boundary |
| --- | --- | --- |
| bounded VXCR v2 decode | connected | C++20 Core reader |
| native Core semantic verification | connected | `Compiler/Core` |
| Core-to-CorePrep atomization/CFG | connected | dedicated adapter |
| CorePrep structural/semantic verification | connected | native CorePrep verifier |
| Xpp lowering/optimization/verification | connected | C++20 Xpp packages |
| Xmm lowering/optimization/verification | connected | C++20 Xmm packages |
| scalar/call/branch/return LLVM lowering | partial | LLVM backend |
| LLVM IR and bitcode output | connected | `.ll` and `.bc` writers |
| target object and assembly output | connected for supported values | target machine |
| `.vxse` link | connected for supported values | entry bridge plus typed LLD driver |
| closure object ABI | planned | LLVM/AARC runtime boundary |
| Xpp/Xmm disk codecs | registered, not connected | public artifact layer |
| project-wide per-source native outputs | registered contract, not connected | source ownership through Core |

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

## Artifact and command matrix

| Input | `check` | Core emit | LLVM IR/BC | object/assembly | binary/run |
| --- | --- | --- | --- | --- | --- |
| explicit `.vxs` | connected | connected | connected subset | connected subset | connected subset |
| project source set | connected | connected | connected subset | per-source route pending | binary connected subset |
| public `.core` | connected | not a conversion target | connected subset | connected subset | connected subset |
| `.xpp` | registered rejection | no | no | no | no |
| `.xmm` | registered rejection | no | no | no | no |
| object | not accepted | no | no | already native | build-only handling |

“Connected subset” means the route itself is real and never falls back to removed code. A source using a type or operation
without a native layout still fails before producing a misleading artifact.

## Ecosystem status

Analyzer, Formatter, and Linter have canonical top-level projects, separate Haskell/Kotlin ownership, and independent CI.
Their typed Kotlin configuration models are implemented without claiming evaluator completion. `vxs format` and `vxs lint`
dispatch installed `vfmt`/`vlint` across compiler-discovered project sources; they are not compiler-internal passes.

Visual Formatter and Visual Linter use their own version lines. Visual Analyzer is an LSP service and editor integration, not
a standalone user binary. See [Ecosystem tools](ECOSYSTEM.md) for the product boundary and current configuration surfaces.

## Data that is intentionally not duplicated

- Namespace identity is not encoded by directories.
- The project evaluator does not expand source globs into compilation units.
- CorePrep is not serialized as a public project artifact.
- The compiler does not contain formatter/linter rule configuration.
- A ViGet `.vipkg` does not add a second `MANIFEST.TOML` beside `Visual.XSharp.kts`.
- JVM support does not own an Xmm reader/writer.
- Standard-library namespaces are not repeated as package dependencies.
- The native driver does not shell-join LLD arguments or use DIMCLI.

## Verification

GitHub CI runs:

- Kotlin project-evaluator tests;
- Haskell build, behavior tests, and package checks;
- Windows ClangCL Bazel build and native CLI contract tests; and
- patch hygiene checks.
