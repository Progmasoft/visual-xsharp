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

Core has its own versioned `VXCR` binary contract, semantic verifier, and explicit `.core` artifact layer in Haskell.
CorePrep crosses the Haskell/C++20 boundary through the separate internal `VXCP` contract. Both transports enforce byte,
collection, string, and recursive-type limits. CorePrep is an adapting stage from Core to Xpp and exists only in RAM; it has
no file extension, artifact API, CLI input, or emit option.

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

Normal compilation keeps these representations in memory. The real `.core` writer and reader exist in the Haskell layer,
but native `.core` input remains disconnected until the C++20 side consumes `VXCR` and lowers verified Core into CorePrep.
Xpp/Xmm readers and their public artifact writers remain later work.
