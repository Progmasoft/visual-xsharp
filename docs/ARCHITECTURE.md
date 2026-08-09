<!--
SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
SPDX-License-Identifier: MPL-2.0
-->

# Compiler architecture

The Visual X# compiler is developed as a staged, testable pipeline. Each stage produces its own data model; later stages must not
guess missing semantics inside the backend.

## Pipeline

```text
.vxs sources
    → lexer
    → parser
    → structural AST
    → macro validation and expansion view
    → semantic analysis and type checking
    → HIR (THIR + XHIR)
    → MIR
    → borrow checker
    → MIR optimization
    → monomorphization
    → codegen unit planning
    → XLIL
    → LLVM IR lowering
    → object emission
    → link
```

## Layer boundaries

### Frontend

The frontend owns `.vxs` source text, tokens, parsing, and the structural AST. The parser produces syntax; it must not invent
LLVM or MIR behavior for unfinished semantics.

### Macro layer

The macro system currently provides validation, token expansion, synthetic reparse, and expanded views. In-place AST
replacement is not complete yet. HIR consumers read macro-call replacements through expanded-view APIs.

### HIR

HIR has two coordinated parts rather than two unrelated intermediate representations:

- THIR is its typed, source-oriented semantic side. It retains the structure needed for diagnostics and records resolved
  symbols, nominal types, overload choices, generic substitutions, and checked expressions.
- XHIR is its normalized operational side. It makes the checked operations needed by MIR lowering explicit while remaining
  high-level and target-independent.

HIR construction is responsible for:

- module/namespace/import resolution
- symbol table generation
- visibility checks
- user-defined and primitive type resolution
- generic arity and constraint resolution
- early expression/mutability diagnostics

`.xhir` is the versioned, human-readable text representation of the XHIR side. It is not a THIR dump. HIR does not depend on
the LLVM API. Type information remains target-independent and may reference the XLIL type vocabulary when needed.

### MIR

MIR is lowered from HIR's XHIR side and is the control-flow, local/place/value, and terminator model. The borrow checker and
MIR optimizer operate on this model.
MIR does not yet provide complete statement/expression lowering or async state-machine generation.

### XLIL

XLIL is the shared input language for backends. `.xlil` is a text registry format and will not be binary. Third-party
frontends will eventually be able to generate XLIL through the `xs/lil.h` public C23 API.

### Backend

The LLVM backend is separate from the frontend. LLVM context, target machine, module, data layout, function declaration
lowering, optimization pipeline, object emission, and linker invocation live here. HIR/MIR/XLIL headers do not expose LLVM C
API concepts.

## Major unfinished pieces

- Full AST macro replacement
- General expression type inference and overload resolution
- Nominal interface membership validation
- Send/Sync, async/await, and Result propagation checks
- MIR statement/expression lowering
- Full borrow-checker region/loan/drop model
- Monomorphization and incremental cache
- XLIL → LLVM function body lowering
- End-to-end `xs build` / `xs run` executable generation

These gaps are summarized in [IMPLEMENTATION.md](IMPLEMENTATION.md) and [TODO.md](TODO.md).
