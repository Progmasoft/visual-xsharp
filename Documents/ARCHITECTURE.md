<!-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com> -->
<!-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0 -->

# Compiler architecture

## Target pipeline

```text
.vxs source
→ Lexer
→ Parser
→ Parsed AST
→ Renamer
→ Name Resolution
→ Resolved AST
→ Type Checker
→ Typed AST
→ Desugarer
→ Core
→ Core Optimizer
→ CorePrep
→ Xpp
→ Xpp Optimizer
→ Xmm
→ Xmm Optimizer
→ LLVM bitcode
→ object file
→ .vxse executable
```

Stage ownership is deliberate:

- Lexer through CorePrep belongs to Haskell.
- Xpp, Xmm, and the native lowering boundary belong to C++20.
- LLVM types and handles belong only to the backend.
- The retiring Rust tree is not linked, is not a production implementation, and receives no new compiler behavior.
- C23 implementation code is migrated subsystem by subsystem after replacement behavior is verified.

## Component ownership

| Component | Language/build owner | Responsibility |
| --- | --- | --- |
| `Compiler/Haskell/Syntax` | Haskell/Cabal | positioned tokens and parsed AST vocabulary |
| `Compiler/Haskell/Frontend` | Haskell/Cabal | Lexer, Parser, Renamer, Name Resolution, Type Checker, and Desugarer |
| `Compiler/Haskell/Core` | Haskell/Cabal | Core/CorePrep models, optimization, verification, and codecs |
| `Compiler/Haskell/Driver` | Haskell/Cabal | source loading, namespace merge, entry selection, and frontend process |
| `Compiler/Core` | C++20/Bazel | bounded Core reader, verifier, and CorePrep adapter |
| `Compiler/Codegen/Xpp` | C++20/Bazel | CorePrep-to-Xpp lowering, optimization, and verification |
| `Compiler/Codegen/Xmm` | C++20/Bazel | Xpp-to-Xmm lowering, optimization, and verification |
| `Compiler/Backend/LLVM` | C++20/Bazel | verified Xmm lowering, LLVM optimization, serialization, and target emission |
| `Compiler/Linker` | C++20/Bazel | typed LLD invocation and executable validation |
| `Compiler/Cli` | C++20/Bazel | command schema, dispatch, output, and exit status |
| `ProjectSystem` | Kotlin/Gradle | project DSL, plugins, plan, SQLite lockfile, and VXDC |

This table is a dependency rule as well as a directory map. The frontend must not include LLVM concepts. LLVM must not parse
source syntax. The project evaluator must not discover `.vxs` files. The CLI coordinates these owners but does not duplicate
their semantic decisions.

## Frontend stages

The frontend does not use one mutable syntax tree for every pass. Parsed, renamed, resolved, and typed forms are distinct
types. Renaming assigns stable identities within lexical scope; name resolution binds references; type checking validates
expressions, calls, conditions, returns, and assignments; desugaring produces target-independent Core.

CorePrep converts nested Core expressions into explicit atoms and operations, then represents control flow with basic blocks
and terminators. A verifier rejects malformed symbols, blocks, branch targets, and unresolved types before native lowering.

## Native middle end

Xpp is a target-independent typed representation consumed from verified CorePrep. Its stage verifier checks optimized
control flow, storage declarations, typed operands, call identity, and terminators before Xmm is constructed. Xmm then lowers
verified Xpp into a lower-level virtual-register model and verifies register storage, signatures, instruction shapes, calls,
results, and control flow before the backend boundary. Function identities, signatures, instruction result types, and the
distinction between function symbols and ordinary registers remain explicit across both stages. Lowering, optimization, and
verification are owned by separate Xpp/Xmm translation units and remain independent of LLVM.

Core has a shared versioned `VXCR` binary contract and equivalent semantic verifiers in Haskell and C++20. The native reader
decodes a bounded `.core` document, verifies it, and applies a dedicated Core-to-CorePrep adapter before Xpp lowering. The
adapter atomizes nested calls and primitive expressions, creates deterministic temporary symbols, and makes branches and
joins explicit without optimizing or reconstructing types. CorePrep can also cross the in-process frontend boundary through
the separate internal `VXCP` contract. Both transports enforce resource and Unicode-scalar limits. CorePrep exists only in
RAM and has no file extension, artifact API, CLI input, or emit option.

## Verification boundaries

Every representation is verified at the boundary that owns it:

```text
Typed AST --desugar--> Core --Core verifier--> VXCR
VXCR --bounded reader/Core verifier--> CorePrep --CorePrep verifier-->
Xpp --Xpp optimizer/verifier--> Xmm --Xmm optimizer/verifier-->
LLVM module --LLVM verifier--> target artifact
```

Verification is intentionally repeated across process and library boundaries. A `.core` file may come from the maintained
frontend or another producer; an embedding client may construct native IR directly; an optimizer may introduce an invalid
CFG even when its input was valid. No stage treats validation by an earlier, optional caller as proof.

Stable `SymbolId` identity is preserved from semantic binding through CorePrep and the native IRs. Human-readable spelling is
diagnostic metadata, not a replacement identity. Function symbols remain distinct from local values and Xmm registers.

## Backend

LLVM is the compiler backend. It consumes Xmm only after the Xmm-owned verifier succeeds and repeats that verifier through a
compatibility adapter for direct embedding clients. It then constructs an LLVM module, lowers supported scalar and
control-flow operations, runs the selected LLVM optimization pipeline, verifies the resulting module, and serializes LLVM IR
and bitcode in memory. Visual X# `String` constants are represented as Unicode scalar (`i32`) storage plus a 64-bit scalar
count; they are not encoded as UTF-8 byte strings. LLVM types and handles do not escape the backend API.

The Bazel graph discovers LLVM through `LLVM_ROOT` or `llvm-config`. The renewed C++20 backend uses LLVM's C++ IR, bitcode, support,
new-pass-manager, target-machine, and native-code-generation libraries; LLVM C handles do not enter this pipeline. A target
machine emits COFF objects or target assembly from verified Xmm. The C++20 driver passes a typed argument vector directly
to LLD, without a shell or DIMCLI, and validates the resulting `.vxse` artifact.

## Entry point

Project entry selection names a namespace-qualified class. That class must contain:

```text
public static void Main()
```

The method has no parameters and returns `void`. Top-level runtime functions and integer-returning `Main` methods are not
valid project entry points.

Entry lookup starts from namespace/type identity in the complete discovered source set. It never derives a path such as
`Namespace/Main.vxs`, and it never requires the final class name to be literally `Main` or `Program`. Those are conventional
class names only; the selected class may have another valid Visual X# name as long as its method contract is correct.

## Intermediate artifacts

The current public artifact names are:

```text
.core
.xpp
.xmm
```

Normal compilation keeps these representations in memory. Haskell writes real `.core` artifacts and C++20 consumes them
through the full verified pipeline. Explicit `.ll`, `.bc`, `.o`, and `.asm` emission is available after source or Core
input. Binary emission adds the platform entry bridge, writes a temporary object, links one `.vxse`, and removes the
temporary object. Xpp/Xmm readers and writers remain later work.

## Process and temporary-file model

The private frontend executable is located relative to `vxs` in the build or installed layout. The current working directory
is project input, not an executable search mechanism. The driver owns temporary Core and link artifacts through scoped
cleanup objects so partial runs do not leak files or accidentally reuse an older artifact.

`vxs run` executes only the `.vxse` produced by its current successful build. A failed compile or link cannot fall through to
an executable left by a previous invocation.

## Extension points

The Kotlin project plugin API extends project configuration and deterministic plan metadata. It is trusted JVM build logic,
not a compiler backend API. A CMake DSL plugin may remain available for projects that need CMake integration even though the
Visual X# compiler itself uses Bazel.

Analyzer, Formatter, and Linter share frontend/source-policy contracts but remain independently versioned products. They do
not become compiler passes merely because `vxs format` and `vxs lint` dispatch them for a project.

## Architecture constraints

- One shipped `vxs` command coordinates frontend and backend; frontend/backend are not separate user binaries.
- `vxdc` is intentionally separate because it is a lockfile dump tool, not compilation.
- CorePrep remains internal and cannot appear in `-Emit` or `-Build`.
- Public artifacts require an explicit, versioned reader/writer contract.
- Machine-specific LLVM installation paths never enter tracked files.
- Native C++ uses the LLVM C++ API and `Visual::XSharp` naming for renewed code.
- New compiler behavior is not added to the retiring Rust tree or a removed C lexer/parser compatibility route.
