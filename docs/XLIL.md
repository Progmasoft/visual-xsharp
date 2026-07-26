<!--
SPDX-FileCopyrightText: 2026 Leitwolf <xs-lang.chess031@slmails.com>
SPDX-License-Identifier: MPL-2.0
-->

# XLIL

XLIL, the X# Low-Level Intermediate Language, is the compiler project's official low-level intermediate language.

XLIL sits before LLVM IR and is designed as the shared input point for all native backends.

## Purpose

- Provide a standard low-level intermediate language for X#.
- Remove direct LLVM IR coupling from frontend and middle-end layers.
- Give every backend a shared intermediate input.
- Keep the compiler's target-independent IR boundary separate from LLVM APIs.
- Allow other programming languages to generate XLIL and use the XS native backend pipeline.

## Shape

XLIL is:

- lower level than CIL,
- higher level than raw assembly,
- assembly-like without being assembly,
- target-independent,
- backend-independent,
- designed for native machine-code generation,
- not bytecode,
- not a virtual-machine instruction stream,
- always text,
- never binary.

## Registry format

`.xlil` files store registry-style records. Module records, function declarations, function definitions, types, and
future body records are stored as text.

An `.xlil` file is not executable bytecode. It is backend input.

Current text records intentionally look closer to assembly than to C-like declarations:

```text
.xlil version 0
.xlil module App
.type %t0 Pair : (i32, i32)
.array %a0 : i32 x 3
.array %a1 : i32
.extern Next : (i64) -> i64
.func Answer : () -> i64
bb0.entry:
  %r0:i64 = const.i64 42
  %r1:i64 = call Next(%r0)
  ret %r1
.end
```

Format notes:

- `.xlil version 0` starts a registry file and declares the XLIL text grammar version that `xs build` should parse.
- `.xlil module <name>` declares the module name after the version header.
- `.type %tN <name> : (<fields>)` adds a sequential nominal aggregate layout to the module type registry. Aggregate
  fields may reference primitive types or previously declared `%tN` types.
- `.array %aN : <element> x <length>` adds a sequential fixed-array layout. The positive element count is part of the type;
  fixed arrays remain compiler-known structural types and are not aliases for nominal collection types.
- `.array %aN : <element>` adds a runtime-sized array layout. Its value carries a data reference and an `i64` element
  count; it is still distinct from the count-changing `ArrayList<T>` collection.
- `.extern <symbol> : (<params>) -> <return>` declares an external function.
- `.func <symbol> : (<params>) -> <return>` starts a function body.
- `.param %rN:type` maps a signature parameter to a body register. Records occur before the first basic block, in
  signature order, and repeat the signature type.
- `.slot %sN:type` declares a typed function-local stack slot after parameters and before the first basic block.
- `bbN.<label>:` starts a basic block.
- `%rN:type` names a typed SSA value.
- `%rN:bool = const.bool true|false` creates a boolean SSA value.
- `%rN:u16 = const.u16 0xXXXX` creates one unsigned 16-bit value. X# `Char` uses this record for one UTF-16 code unit;
  the hexadecimal immediate always has exactly four digits.
- `%rN:T = const.T 0x...` creates fixed-width integer bit patterns for `u8`, `i8`, `i16`, `u32`, `u64`, `u128`, and
  `i128`. The hexadecimal field has exactly two digits per byte. Signed records use two's-complement bits, so XLIL does
  not depend on a host C implementation's native 128-bit extension.
- `%rN:str = const.str utf16le [0x004c, ...]` and its `utf16be` form create a borrowed static string view from
  explicit UTF-16 code units. The tag fixes the byte order used by the target data object; the numeric list always
  contains Unicode UTF-16 code-unit values. Untagged string constants are invalid.
- `%rN:bool = eq.str %rA, %rB` and `ne.str` compare two `str` views by UTF-16 code-unit length and content. Pointer
  identity is not observable through these instructions.
- `%rN:i32 = const.i32 N` creates a signed 32-bit integer constant.
- `%rN:i64 = const.i64 N` creates a signed 64-bit integer constant. The explicit width is part of the opcode; the
  untyped `const N` spelling is not accepted.
- `%rN:f32 = const.f32 0xXXXXXXXX` and `%rN:f64 = const.f64 0xXXXXXXXXXXXXXXXX` create IEEE-754 constants from exact
  bit patterns. Fixed-width hexadecimal spelling preserves negative zero, infinities, and NaN payloads across text
  round trips.
- `%rN:f32 = add.f32 %rA, %rB`, `sub.f32`, `mul.f32`, `div.f32`, and `rem.f32` perform IEEE-754 binary32 arithmetic.
  The corresponding `.f64` records perform IEEE-754 binary64 arithmetic.
- `%rN:bool = eq.f32 %rA, %rB`, `ne.f32`, `lt.f32`, `le.f32`, `gt.f32`, and `ge.f32` use ordered IEEE comparisons. Their `.f64`
  forms have the same semantics. Every listed comparison is false when either operand is NaN.
- `%rN:i32 = add.i32 %rA, %rB`, `sub.i32`, `mul.i32`, `div.i32`, and `rem.i32` perform signed 32-bit integer arithmetic.
- `%rN:i32 = and.i32 %rA, %rB`, `or.i32`, `xor.i32`, `shl.i32`, and arithmetic `shr.i32` perform bitwise 32-bit integer
  operations.
- `%rN:bool = eq.i32 %rA, %rB` and `ne.i32` compare two `i32` values for equality or inequality.
- `%rN:bool = lt.i32 %rA, %rB`, `le.i32`, `gt.i32`, and `ge.i32` perform signed `i32` comparisons.
- `%rN:i64 = add.i64 %rA, %rB`, `sub.i64`, `mul.i64`, `div.i64`, and `rem.i64` perform signed 64-bit integer arithmetic.
- `%rN:i64 = and.i64 %rA, %rB`, `or.i64`, `shl.i64`, and arithmetic `shr.i64` perform bitwise 64-bit integer operations.
- The same `add`, `sub`, `mul`, `div`, `rem`, `and`, `or`, `xor`, `shl`, and `shr` records are available for every
  fixed-width integer type. For example, `%rN:u8 = add.u8 %rA, %rB` and
  `%rN:i128 = shr.i128 %rA, %rB` preserve their operand width. Add, subtract, and multiply are modular at that width.
  Division, remainder, right shift, and ordered comparisons use signed semantics for `i*` types and unsigned semantics
  for `u*` types. Shift counts must be smaller than the operand width.
- `eq.T`, `ne.T`, `lt.T`, `le.T`, `gt.T`, and `ge.T` produce `bool` for every fixed-width integer operand type `T`.
- `%rN:bool = eq.i64 %rA, %rB` and `ne.i64` compare two `i64` values for equality or inequality.
- `%rN:bool = lt.i64 %rA, %rB`, `le.i64`, `gt.i64`, and `ge.i64` perform signed `i64` comparisons.
- `%rN:bool = not.bool %rA` negates a boolean SSA value.
- `%rN:type = call <symbol>(%rA, %rB)` calls another function and stores a typed result.
- `%rN:%tA = aggregate %rB, %rC` builds a registry-defined aggregate value. Field count and register types must match
  the `%tA` layout exactly.
- `%rN:type = extract %rA, F` extracts zero-based field `F` from an aggregate register. Its result type must match the
  registered field type.
- `%rN:%aA = array %rB, %rC` constructs an array. For a fixed layout, the register count must equal the `%aA` length.
  A runtime-sized layout accepts the encoded number of initial elements. Every register must have the element type.
- `%rN:type = extract.array %rA, I` reads the zero-based constant index `I` from a fixed-array register. The index must
  be in bounds and the result type must equal the array element type.
- `%rN:type = array.get %rA, %rI` reads an element with an `i64` runtime index. `%rA` must use a registered array layout.
- `%rN:%aA = array.set %rA, %rI, %rV` creates an updated array value with one element replaced. The runtime index is `i64`,
  `%rV` must match the registered element type, and `%rN` preserves the source array type.
- `%rN:i64 = len.array %rA` reads the element count from either a fixed or runtime-sized registered array.
- `call <symbol>(%rA, %rB)` calls a void function and discards the result.
- `store %rN, %sA` writes a typed register value to a matching stack slot.
- `%rN:type = load %sA` reads a matching stack slot into a new typed register value.
- `br bbN` transfers control to another basic block.
- `br_if %rN, bbA, bbB` branches to `bbA` when the `bool` condition `%rN` is true, otherwise to `bbB`.
- `ret` and `ret %rN` are the current return terminators.
- `panic` terminates the current path. The LLVM backend lowers it to `llvm.trap` followed by `unreachable`.
- `.end` closes a function body.

A parameter-returning definition therefore has an explicit body mapping:

```text
.func Identity : (i64) -> i64
.param %r0:i64
bb0.entry:
  ret %r0
.end
```

Call targets resolve only within the registry module. Forward references are accepted, but every target must be declared by
`.extern` or `.func` and its argument and return types must match the call record.

Stack slots are function-local and target-independent. Slot ids are sequential, `void` slots are invalid, and load/store
value types must exactly match the slot type. The LLVM backend currently materializes them with entry-block `alloca`
instructions and typed `load`/`store` operations.

MIR remains target-independent and writes string constants as `utf16 [0x004c, ...]`. Endianness becomes concrete only
when MIR is lowered to XLIL. XHIR remains closer to X# source and represents the same value as a quoted string literal.
Similarly, XHIR preserves a source-like character literal, XMIR writes its 16-bit code-unit value as `const.u16`, and
XLIL carries that value as a target-independent `u16` register.

The XLIL v0 `str` value is a borrowed view containing a pointer and a target-sized UTF-16 code-unit count. A constant's
backing data is immutable and has static storage duration. Its length excludes any terminator; XLIL string constants do
not add or require a null terminator. `Optional<Str>` ownership and allocation are separate higher-level semantics and
are not represented by this `str` constant record.

Version `0` is the only supported XLIL version today. The header exists so `xs build --xlil -file <input.xlil>` can select
the correct XLIL grammar as the format evolves. It is not a bytecode VM version and does not make `.xlil` binary.

## Public API

XLIL exposes its C23 producer and consumer API through:

```c
#include <xs/lil.h>
```

This header is the public API surface for AOT production and XLIL registry generation.

The API is intended to let:

- other programming languages generate XLIL,
- third-party compilers target XLIL,
- alternative frontends exist,
- every XLIL-producing compiler reuse the same backend infrastructure.

The C23 API can construct every record in the currently implemented XLIL v0 model: scalar and registry types, external
and defined function signatures, parameter values, stack slots, blocks, constants, calls, integer and floating-point
operations, strings, aggregates, arrays, memory operations, branches, returns, and panic terminators. It also provides
module verification, canonical owned-text emission, bounded v0 text parsing, mutable producer lookup, and read-only
inspection of every instruction payload. Direct `xs build --xlil -file <input.xlil>` uses the same parser and verifier
before emitting optimized LLVM IR, an object file, and a native `.xse` executable for the supported local-target subset.

The model API is the precise layer; `XsLilBuilder` is the convenience layer. A builder keeps an insertion block, infers
operand, field, array-element, and call-result types from the registry, resolves call signatures by name, accepts block
handles for branches, and reports invalid construction immediately:

```c
XsLilBuilder *builder = nullptr;
XsLilFunction *main_function = nullptr;
XsLilValueId value = XS_LIL_INVALID_VALUE_ID;

xs_lil_module_add_function_definition(
    module, "main", xs_lil_scalar_type(XS_LIL_TYPE_I32), nullptr, 0, &main_function, &error);
xs_lil_builder_create(module, &builder, &error);
xs_lil_builder_append_block(builder, main_function, "entry", nullptr, &error);
xs_lil_builder_const_i32(builder, 0, &value, &error);
xs_lil_builder_return_value(builder, value, &error);
xs_lil_builder_destroy(builder);
```

The producer-owned text path does not expose a C `FILE *` across an FFI boundary:

```c
#include <xs/lil.h>

XsLilError error = {0};
XsLilModule *module = nullptr;
XsLilText text = {0};

if(xs_lil_module_create("Example", &module, &error) != XS_LIL_OK)
  return 1;

// Add types, signatures, blocks, instructions, and terminators here.

if(xs_lil_module_emit_text(module, &text, &error) != XS_LIL_OK)
{
  xs_lil_module_destroy(module);
  return 1;
}

// text.data is NUL-terminated; text.length excludes the terminator.
xs_lil_text_destroy(&text);
xs_lil_module_destroy(module);
```

`xs_lil_module_emit_text` verifies the complete module before allocating canonical v0 text. The returned bytes belong to
the caller until `xs_lil_text_destroy`. `XS_LIL_TEXT_VERSION` names the current format version, and explicit invalid
value/block/slot ID constants are available to bindings that cannot conveniently use C designated initializers.

`lil-c` is also the stable FFI boundary for official language bindings. Modules, functions, blocks, and builders are
opaque handles. Their addresses remain stable while their owning module lives, even when the registry grows. Strings
returned by inspection functions are borrowed and remain valid for the same lifetime. `XsLilText` is the explicit
owned-buffer exception. Every public symbol uses `XS_LIL_API`; `xs_lil_c_api_version()` returns the packed
`XS_LIL_C_API_VERSION` value so a binding can reject an incompatible library before constructing a module. The installed
runtime library is `libxs_lil` on Unix-family targets and the corresponding `xs_lil` DLL on Windows. Bindings that do not
want to reproduce the error and text structure layouts can allocate those objects with `xs_lil_error_create` and
`xs_lil_text_create`, inspect them through accessors, and release them with the matching destroy/delete function.

`<xs/lil.h>` is the umbrella include for C23 producers. Code that wants a smaller dependency surface may include
the standalone headers under `<xs/lil-c/>`: `api.h`, `model.h`, `module.h`, `function.h`, `instruction.h`, `builder.h`,
and `text.h`.
The Rust producer API is exposed directly under `xslang::xlil::*`; its public model, parser, verifier, and writer can build
and round-trip the same XLIL v0 registry without going through the C ABI.

The public [integer support header](../include/xs/int128.h) defines `XsUInt128` and `XsInt128` as explicit high/low
64-bit words. XLIL uses those project-owned C23 types for u128/i128 constants; compiler-specific native 128-bit
extensions are neither required nor used.
The public model exposes one typed integer-operation constructor and read-only operation/type accessors, so third-party
XLIL producers do not need a separate C entry point for every width.

## Direct native entry point

Direct native builds require exactly one defined XLIL entry function:

```text
.func main : () -> i32
bb0.entry:
  %r0:i32 = const.i32 0
  ret %r0
.end
```

The direct driver uses this function as the operating-system process entry point without generating an X# runtime wrapper.
It runs the configured LLVM verification/optimization pipeline, then writes `.ll`, `.o`, and `.xse` executable artifacts next
to the input registry. The current direct XLIL path uses the backend O0 pipeline. Linking is limited to the local Linux ELF
host and does not provide runtime or external-library resolution. PE `.xse` output is planned after ELF support.
