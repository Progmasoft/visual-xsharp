<!--
SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
SPDX-License-Identifier: MPL-2.0
-->

# LLVM backend infrastructure status

The LLVM backend is the `xs_backend_llvm` library, separate from the frontend. It does not accept AST or HIR directly. HIR
and MIR do not depend on LLVM, but they may use the target-independent XLIL type/data vocabulary. LLVM C API concepts such
as target triples and data layouts exist only in the backend layer. Real function body generation will be added after typed,
borrow-checked, monomorphized MIR nodes are lowered to XLIL.

LLVM is the only implemented and supported backend today, but the HIR/MIR/XLIL design must remain portable to future
backends. Target-specific
assembly, if needed, is isolated in backend/runtime layers. NASM `.asm`/`.inc` files may be used only without locking the
design to x86-64; ARM64 compatibility must be preserved.

## Implemented components

- LLVM context and target-machine lifetime management
- Explicit target triple or native target triple selection
- Target data layout generation
- Independent LLVM module per codegen unit
- Numeric X# primitive type mapping to LLVM types
- Borrowed `Str` mapping to a `{ pointer, target-sized UTF-32 code-point count }` view
- Body-less function declaration and signature lowering
- XLIL type mapping for function declarations
- Direct `.xlil` parser/model-driven `.extern`/`.func` lowering to verified and optimized LLVM IR, objects, and local
  native `.xse` executable artifacts for `.func main : () -> i32`
- Initial XLIL body lowering for parameters, constants including 16-bit character code units, i32 arithmetic/bitwise/shift/comparison, i64
  arithmetic/bitwise/shift/comparison, f32/f64 arithmetic and ordered comparisons, explicit UTF-32 string constants,
  typed stack-slot `load`/`store`,
  `call`, `br`, `br_if`, `panic`, `ret`, and `ret %rN`
- LLVM optimization pipeline selection from `default<O0>` through `default<O3>`
- LLVM module verification
- Object file emission per codegen unit
- Linker invocation layer that does not use a shell and receives arguments from the caller

Object file emission and linker invocation are wired into direct `.xlil` native builds and the supported `.xs`
compiler-core slice. Unsupported source constructs are diagnosed or remain outside that incremental slice; they are not
lowered by inventing LLVM-only semantics.

The supported source slice includes `Optional<T>` construction, `??`, mutable-local `??=`, postfix `!`, and safe
data-field access for payload types already representable in the XLIL aggregate model. These operations are lowered
before the backend to aggregate construction/extraction, conditional branches, function-local merge storage, and the
existing panic terminator. LLVM therefore receives ordinary XLIL records rather than an Optional-specific ABI or
instruction. An absent `Optional<Data>` uses a recursively constructed deterministic aggregate payload; only its false
discriminant is semantically observable.

Structured `Result<T, E>` values use the same backend-neutral aggregate route. Compiler-core checks `Ok` and `Error`
payloads, lowers postfix `@` to explicit MIR control flow and early return, and lowers exhaustive Result matches to
discriminant branches plus payload extraction. LLVM sees only named aggregate values, `extractvalue`, ordinary conditional
branches, and returns. No LLVM type or instruction is exposed in HIR or MIR.

The currently supported native Result slice includes:

- same-module functions accepting and returning concrete Result types;
- direct `Ok(value)` and `Error(value)` construction;
- postfix propagation when the enclosing function has the same error payload;
- nested Result and Optional payload layouts already representable by the aggregate registry;
- value-producing and statement Result matches with `Ok(binding)` and `Error(binding)` arms;
- native observability through a `Long` reader function that converts either payload to the process exit result.

Imported function resolution, method calls returning Result, runtime-owned standard Error objects, and unwinding are not
part of this aggregate path. Recoverable failure does not require an exception ABI.

## String mapping and deferred owned strings

`Str` is an immutable borrowed UTF-32 view with an implicit static lifetime. LLVM represents the view as a pointer plus
the target's pointer-sized integer count of Unicode code points. XLIL `const.str utf32le|utf32be` lowering emits an
immutable private `i32` array in the requested byte order and does not append a null terminator.

Fixed-array constant indices lower to LLVM aggregate extraction. Calculated `Int` (`i64`) indices lower through temporary
array storage and `getelementptr`; generated control flow checks both index bounds and traps on an invalid index before any
element load or store. Runtime-sized `[T]` arrays lower as an element pointer plus an `i64` count. Their construction,
checked reads/writes, `count`, same-module calls, and `for` iteration now reach native `.xse` output. This first storage
slice uses hosted allocation; ownership and reclamation remain later runtime work.
Native objects use position-independent relocation so the view can safely refer to that static data from a PIE `.xse`.

`Optional<Str>` is the canonical boxed, owned optional-string type. Its allocator, ownership, discriminant, and runtime
layout remain deferred; the borrowed `Str` view does not invent those semantics.

`Bool` has an unsigned 8-bit value representation. LLVM branch lowering compares it with zero to obtain an `i1`
condition where required.

`Byte` and `SByte` are separate unsigned/signed 8-bit primitive types at the HIR level. Their LLVM storage type is `i8` for
both; signedness is selected later by typed MIR operations such as comparisons, conversions, and arithmetic.

`Char` lowers to LLVM `i32` because it stores one Unicode scalar as `u32`.

## Preserved stage boundary

The backend order is preserved as:

```text
Borrow-checked and optimized MIR
    → monomorphization
    → codegen unit splitting
    → XLIL
    → LLVM module and function signature lowering
    → XLIL function body lowering
    → LLVM optimization
    → object file emission
    → linker invocation
```

XLIL function body lowering currently covers explicit body parameters, every documented fixed-width integer constant,
exact-bit f32/f64 constants, and
boolean and UTF-32 string constants, UTF-32 string equality/inequality, fixed-width integer arithmetic/bitwise/shift/comparison instructions, f32/f64
arithmetic and ordered comparisons,
direct calls, nominal aggregate construction/extraction, unconditional `br`, conditional `br_if`, `panic`, `ret`,
typed stack slots with `load`/`store`, and `ret %rN`. Stack slots are allocated in the LLVM entry block and remain eligible
for normal LLVM promotion and scalar optimization. The current source-native bridge uses this path for `Long` and `Bool`
local initialization, reads, and simple mutable reassignment. `panic` emits an `llvm.trap` call followed by LLVM
`unreachable`. Floating constants are bitcast from integer constants so their XLIL bit patterns reach LLVM without a
locale-sensitive decimal conversion. Ordered floating comparisons lower to LLVM ordered predicates, so NaN makes each
supported comparison false. Native Linux links include the platform math library because optimized floating remainder may
become an `fmod`/`fmodf` runtime call.
`eq.str`/`ne.str` compare the `{pointer, code-point length}` views by length and UTF-32 storage content.
The current hosted native ABI emits a bounded `memcmp` call after selecting the shorter byte length, then combines the
content result with exact code-unit-length equality.

Integer constants from 8 through 64 bits lower with `LLVMConstInt`. The project-owned two-word C23 representation carries
u128/i128 constants into `LLVMConstIntOfArbitraryPrecision`; no compiler-specific C integer extension is involved.
Integer operations retain their declared width. Signed types select LLVM signed division, remainder, arithmetic right
shift, and signed predicates; unsigned types select their unsigned/logical counterparts. No LLVM `nsw` or `nuw` flag is
attached to modular add, subtract, or multiply.

The source-native bridge lowers supported `if`, `while`, classic `for`, fixed-array for-each, and statement-level `match`
control flow into MIR
branches before XLIL and LLVM lowering. A supported `match` over `Long` or `Bool` becomes ordered literal tests and branch
blocks with a required final `else` arm. This remains a narrow source slice; general iterator protocols, general expressions, and arbitrary
runtime-backed statements are not implemented yet.
Parameter values are read from the declared
LLVM function; calls use declarations emitted for the same XLIL registry module. The backend emits declarations from the
public C API and direct `.xlil` files after they are parsed into the XLIL C model, can write verified LLVM IR text for the
current codegen unit, runs the configured LLVM verification/optimization pipeline, emits an object through LLVM, and invokes
the configured Clang driver with LLD for a native-host direct XLIL `.xse` executable. Direct XLIL currently uses the O0 pipeline;
no CLI optimization flag is exposed yet. It rejects unsupported body forms instead of inventing semantics. This prevents AST
or unfinished HIR behavior from being lowered directly to LLVM IR. Cross-target direct builds stop after object emission, and
runtime or external library resolution is not configured yet.

XLIL aggregate registry entries lower to named LLVM structure types. The backend creates all opaque named types before
setting their field bodies, then lowers `aggregate` and `extract` records with LLVM `insertvalue` and `extractvalue`.
This supports aggregate signatures, direct XLIL native builds, and non-recursive source `data` returns without coupling HIR
or MIR models to LLVM APIs. Structural tuple values use the same registry and instructions while retaining their tuple field
identity in typed HIR and XHIR. Aggregate-returning source calls may initialize local `data` or tuple places; tuple-valued
parameters and returns retain their named structure identity through generated LLVM function signatures. Nested tuple layouts
and fixed arrays containing tuples are declared before use. Tuple element updates are rebuilt with target-independent
aggregate operations and lower to LLVM `insertvalue`; source array access remains ordinary indexing syntax.
Payload-free normal enums reuse this aggregate route as a named single-`i32` tag structure. Their nominal identity stays
in HIR/XHIR, while MIR/XLIL construction and extraction lower to ordinary LLVM aggregate operations. This representation
does not define arbitrary user `enum data` layout. The compiler-provided Result layout described above is a bounded,
specialized middle-end contract for the implemented recoverable-failure path.
