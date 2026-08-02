<!--
SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
SPDX-License-Identifier: MPL-2.0
-->

# Rust XLIL API

The xslang crate exposes the target-independent XLIL v1 model to Rust
producers, readers, verifiers, optimizers, and compiler frontends. These APIs
do not require LLVM. LLVM consumes verified XLIL later in the pipeline.

## Choosing an API level

Three producer levels share the same canonical model:

1. xslang::xlil::Builder is the complete insertion-point builder. It accepts
   runtime XLIL type values and suits compilers with their own type system.
2. xslang::xlil::typed::TypedBuilder adds Rust marker types. It catches common
   register, slot, signature, and call mismatches before module verification.
3. the xlil_create attribute translates a deliberately limited Rust function
   subset into companion XLIL module and text producers.

All levels produce xslang::xlil::Module. They do not define separate dialects
or bypass the canonical verifier and writer.

## Dependency

Use the main crate for the model and builders:

    [dependencies]
    xslang = "0.2.8"

Enable the attribute macro only when needed:

    [dependencies]
    xslang = { version = "0.2.8", features = ["proc-macros"] }

The macro implementation is published as xslang-proc-macros, but applications
should normally enable the feature on xslang instead of depending on the
implementation crate directly.

## Raw builder

The raw builder accepts explicit XLIL types and register identifiers:

    use xslang::xlil::{
        Builder, IntegerBinaryOperation, Type, module_to_string,
    };

    let mut builder = Builder::new("calculator");
    builder.begin_function("add", Type::I64, vec![Type::I64, Type::I64])?;
    let entry = builder.append_block("entry")?;
    builder.position_at_end(entry)?;

    let left = builder.parameter(0)?;
    let right = builder.parameter(1)?;
    let sum = builder.binary_integer(
        IntegerBinaryOperation::Add,
        Type::I64,
        left,
        right,
    )?;
    builder.return_value(Some(sum))?;

    let module = builder.finish()?;
    println!("{}", module_to_string(&module));

Builder::finish verifies the completed module. Invalid control flow, missing
terminators, unknown values, mismatched calls, and structural errors become
BuildError values instead of entering a supposedly valid module.

### Insertion points

Instructions are appended to the selected block. append_block creates and
selects a block. position_at_end selects an existing block. A producer should
not assume that creating a value changes the selected block.

The insertion_block query is useful for structured lowering that creates
branch blocks while translating an expression.

### Values and slots

XLIL values are immutable virtual registers. Stack slots are separate mutable
storage:

    let slot = builder.add_slot(Type::I32)?;
    let initial = builder.const_i32(7)?;
    builder.store(slot, initial)?;
    let loaded = builder.load(slot)?;
    builder.return_value(Some(loaded))?;

Use slots for source constructs that require a merge point or mutable local.
Value identifiers and slot identifiers are never interchangeable.

### Control flow

Blocks end in exactly one terminator:

    let condition = builder.parameter(0)?;
    let then_block = builder.append_block("then")?;
    let else_block = builder.append_block("else")?;

    builder.position_at_end(entry)?;
    builder.branch_if(condition, then_block, else_block)?;

The condition must have XLIL type bool. Both targets must exist in the current
function. A terminator closes its block; later instructions require another
selected block.

## Typed builder

The typed facade stores an XLIL identifier and its Rust marker together:

    use xslang::xlil::IntegerBinaryOperation;
    use xslang::xlil::typed::{Signature, TypedBuilder};

    let mut builder = TypedBuilder::new("calculator");
    builder.begin(
        Signature::returning::<i64>("add")
            .parameter::<i64>()
            .parameter::<i64>(),
    )?;
    builder.append_block("entry")?;

    let left = builder.parameter::<i64>(0)?;
    let right = builder.parameter::<i64>(1)?;
    let sum = builder.integer(IntegerBinaryOperation::Add, left, right)?;
    builder.return_value(sum)?;
    let module = builder.finish()?;

Value<T> and Slot<T> are lightweight typed handles. Marker types do not alter
the XLIL text format.

### Type markers

Native Rust primitives select matching XLIL types where representations exist:

- bool
- i8, i16, i32, i64, and i128
- f32 and f64

The xslang::xlil::types module also exports explicit names:

- I8, I16, I32, I64, and I128
- F16, F32, F64, and F128

F16 and F128 preserve exact bits. They can appear in signatures and
pass-through values. Arithmetic remains deferred until XLIL operations and
backend lowering define that behavior.

### Erased values

AnyValue carries a register and its runtime XLIL type. It is intended for
heterogeneous lists such as call arguments:

    let arguments = vec![left.erase(), right.erase()];
    let result = builder.call_value::<i64>("add", arguments)?;

Use downcast to recover a typed value when the expected marker is known. A
failed downcast returns TypeMismatch.

### Typed slots

Slots preserve their element marker:

    let slot = builder.add_slot::<i64>()?;
    builder.store(slot, left)?;
    let value = builder.load(slot)?;

Storing an i32 value in an i64 slot therefore becomes a Rust error at the
producer call site rather than a later verifier error.

### Raw escape hatch

TypedBuilder::raw gives temporary access to the underlying builder.
TypedBuilder::into_raw transfers ownership. These methods serve records that
do not yet have a typed facade. They do not create a second representation or
disable final verification.

## UTF-32 construction

Utf32Builder converts Rust UTF-8 strings into Unicode scalar values:

    use xslang::xlil::types::Utf32Builder;

    let value = Utf32Builder::from_text("Leitwolf");
    assert_eq!(value.units()[0], 0x004c);

The builder retains code points, not the original source text. Passing its
units to the XLIL string-constant builder emits numeric UTF-32 registry data.

## Attribute producer

The attribute keeps the original Rust function and generates two companions:

- name_xlil returns a verified Module.
- name_xlil_text returns canonical XLIL text.

    #[xslang::xlil_create]
    fn score(value: i64) -> i64
    {
        let doubled = value * 2;
        doubled + 3
    }

    assert_eq!(score(4), 11);
    let module = score_xlil()?;
    let text = score_xlil_text()?;

The source function retains ordinary Rust behavior. Generated companions have
the same visibility as the attributed function.

### Attribute options

Names can be configured without changing the XLIL function name:

    #[xslang::xlil_create(
        module = "example.math",
        producer = "build_sum",
        text = "write_sum",
    )]
    pub fn sum(left: i64, right: i64) -> i64
    {
        left + right
    }

module sets the registry module name. Without it, the macro derives a stable
name from the Rust module path and function name. producer and text must be
valid Rust identifiers. Duplicate, unknown, non-string, and empty options are
compile errors.

### Supported signatures

The 0.2.6 subset accepts safe, synchronous, non-generic free functions.
Parameters must be immutable identifier bindings. Supported types are bool,
signed integers, f32, f64, and explicit XLIL numeric markers.

An omitted Rust return type becomes XLIL void. A value-returning function must
end in a tail expression or an explicit return statement with a value.

The following are intentionally rejected:

- methods and receivers;
- generic or variadic functions;
- async, unsafe, extern, and const functions;
- reference, tuple, aggregate, and application-defined value types;
- mutable, reference, destructuring, or subpattern parameters.

### Supported expressions

The current subset includes:

- parameter and immutable local references;
- boolean, integer, f32, and f64 literals;
- signed integer arithmetic, bitwise operations, shifts, and comparisons;
- f32 and f64 arithmetic and ordered comparisons;
- unary numeric negation and boolean negation;
- value-producing if/else;
- boolean and/or with short-circuit control flow;
- nested value blocks containing immutable local bindings.

Integer literals use the expected type supplied by the signature, local
annotation, or surrounding expression. A context-free integer literal
defaults to i64. Out-of-range literals are rejected during macro expansion.

Float literals default to f64. An f32 suffix or expected f32 type selects XLIL
f32. F16 and F128 literals and arithmetic are not silently rounded through a
host type.

### Local bindings

Only initialized, immutable identifier bindings are accepted:

    #[xslang::xlil_create]
    fn adjusted(value: i32) -> i32
    {
        let doubled = value * 2;
        let offset: i32 = 1;
        doubled + offset
    }

Explicit type annotations are checked against the initializer. Bindings obey
lexical block scope. Mutable, destructuring, uninitialized, and let-else
bindings are rejected until their XLIL lowering is defined.

### Value conditionals

A conditional used as a value requires an else branch. Both branches must
produce the same type:

    #[xslang::xlil_create]
    fn select(condition: bool, left: i32, right: i32) -> i32
    {
        if condition { left } else { right }
    }

The macro emits explicit then, else, and merge blocks plus a typed slot. This
is ordinary XLIL control flow; there is no hidden select instruction.

### Short circuit

For boolean and, the right side is evaluated only on the true edge. For
boolean or, the right side is evaluated only on the false edge. Generated
XLIL uses br_if, a boolean slot, and a merge block, preserving evaluation
order.

### Rejected constructs

Unsupported expressions produce a compile error at the attributed function.
The macro does not leave a partial module or guess semantics for calls,
closures, loops, aggregates, references, casts, indexing, field access,
assignment, or early control-flow exits.

Full language frontends should use the explicit builders after resolving those
constructs into their own typed IR.

## Reading and verification

Use parse_verified or read_verified for untrusted or external text when the
caller needs a proof-carrying verified wrapper. The plain parser remains
available for diagnostics and repair tools that must inspect invalid input.

The verifier checks module registries, signatures, blocks, values, slots,
calls, instructions, and terminators. Parsing and verification remain distinct
operations even when a convenience function performs both.

## Writing

module_to_string produces canonical, human-readable XLIL v1. Use
write_module_io for an std::io::Write destination. The writer never emits a
binary encoding.

Text produced by an attribute companion, raw builder, or typed builder has the
same format. It can be consumed by the C API, Java FFM binding, compiler
driver, and future conforming producers.

## Error boundaries

Prefer the structured error belonging to each operation:

- BuildError for builder failures;
- ParseError for text input;
- verifier diagnostics for invalid models;
- TypedBuildError for typed-facade mismatches;
- std::io::Error for stream operations.

Applications that want one conventional boundary may use
xslang::rust::XSResult<T>. This alias is optional and does not replace the
specific XLIL error contracts.

## Stability

The Rust API and XLIL format are pre-1.0. Format version 1 is the only emitted
version. Readers still accept version 0 for migration, but writers do not
downgrade output. Future format support remains explicitly versioned.
