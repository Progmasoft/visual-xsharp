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
register model. Both stages have optimization boundaries and remain independent of LLVM.

The final production transfer from Haskell-owned CorePrep into C++20 is not complete. Existing C++20 tests construct the
transfer model directly, so passing those tests proves the native slice itself, not a complete end-to-end production route.

## Backend

LLVM is the compiler backend. The build discovers LLVM through its CMake package and links the LLVM C API. Native output is
linked with LLD through the Windows ClangCL toolchain.

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

Normal compilation keeps these representations in memory. The CLI already reserves explicit emission selections, but the
production path for emitting them is not connected yet.
