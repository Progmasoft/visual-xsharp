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
- The Rust compiler core remains a supported implementation asset during the transition.
- C23 implementation code is migrated subsystem by subsystem after replacement behavior is verified.

## Frontend stages

The frontend does not use one mutable syntax tree for every pass. Parsed, renamed, resolved, and typed forms are distinct
types. Renaming assigns stable identities within lexical scope; name resolution binds references; type checking validates
expressions, calls, conditions, returns, and assignments; desugaring produces target-independent Core.

CorePrep converts nested Core expressions into explicit atoms and operations, then represents control flow with basic blocks
and terminators. A verifier rejects malformed symbols, blocks, branch targets, and unresolved types before native lowering.

## Native middle end

Xpp is a target-independent typed representation consumed from verified CorePrep. Xmm lowers Xpp into a lower-level virtual
register model. Function identities, signatures, instruction result types, and the distinction between function symbols and
ordinary registers remain explicit across both stages. Both stages have optimization boundaries and remain independent of
LLVM.

Core has a shared versioned `VXCR` binary contract and equivalent semantic verifiers in Haskell and C++20. The native reader
decodes a bounded `.core` document, verifies it, and applies a dedicated Core-to-CorePrep adapter before Xpp lowering. The
adapter atomizes nested calls and primitive expressions, creates deterministic temporary symbols, and makes branches and
joins explicit without optimizing or reconstructing types. CorePrep can also cross the in-process frontend boundary through
the separate internal `VXCP` contract. Both transports enforce resource and Unicode-scalar limits. CorePrep exists only in
RAM and has no file extension, artifact API, CLI input, or emit option.

## Backend

LLVM is the compiler backend. The C++20 backend verifies Xmm before constructing an LLVM module, lowers supported scalar and
control-flow operations, runs the selected LLVM optimization pipeline, verifies the resulting module, and serializes LLVM IR
and bitcode in memory. Visual X# `String` constants are represented as Unicode scalar (`i32`) storage plus a 64-bit scalar
count; they are not encoded as UTF-8 byte strings. LLVM types and handles do not escape the backend API.

The build discovers LLVM through its CMake package and links the LLVM C API. The compatibility route still owns object-file
and executable production. Connecting verified Xmm bitcode to object emission and LLD is separate work.

## Entry point

Project entry selection names a namespace-qualified class. That class must contain:

```text
public static void Main()
```

The method has no parameters and returns `void`. Top-level runtime functions and integer-returning `Main` methods are not
valid project entry points.

## Intermediate artifacts

The current public artifact names are:

```text
.core
.xpp
.xmm
```

Normal compilation keeps these representations in memory. Haskell writes real `.core` artifacts and C++20 consumes them
through the full verified pipeline. Explicit `.ll` and `.bc` emission is available after Core input. Native object/linking
from that route and Xpp/Xmm readers and writers remain later work.
