<!--
SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
SPDX-License-Identifier: MPL-2.0
-->

# XMIR text format

`.xmir` is the human-readable text form of X# MIR. It is not assembly-like, not binary, and not an opaque compiler
serialization format.

XMIR exists for ownership, borrow-checking, drop planning, async lowering, and optimization inspection. MIR
is lower than HIR, so XMIR may expose blocks, places, temporaries, and terminators, but it should still read as a structured
control-flow document rather than an assembly listing.

## Design rules

- The format is deterministic, newline-normalized, UTF-8 text.
- Function bodies are grouped by function, then by named control-flow sections.
- Blocks are records with explicit fields: parameters, statements, terminator, successors, and optional analysis notes.
- Statements use descriptive names and named fields rather than XLIL-style typed SSA assignment syntax.
- Indentation is for readability only; parsers must use explicit records such as `.end` and `.program end`, not leading
  spaces, to determine structure.
- Places, projections, moves, borrows, drops, and regions are printed in stable symbolic form.
- Borrow-checker and optimizer annotations are allowed when they are explicitly marked as analysis output.
- XMIR must remain suitable for code review and text fixtures.
- XMIR must not use XLIL-style directives such as `.func`, `.extern`, or `%r0:type = op`.

## Shape

The exact grammar will be tightened as MIR stabilizes, but the intended shape is CFG/document oriented:

```text
.xmir version 0
function Main
returns void

parameters
  parameter value
    local 0
    type i64
.end

locals
  local 0
    name value
    type i64
    mutability immutable
.end

control_flow
  block 0
    statements
      statement use
        local 0
    terminator return
.end

analysis verify
  diagnostic missing_terminator
    span 1:0..1
    message MIR block is missing a terminator
.program end
```

XMIR can expose control-flow because that is MIR's job, but it should do so with named records and analysis sections. XLIL is
the later assembly-like backend input; XMIR is the compiler's readable mid-level reasoning dump.

## Current parser subset

Rust `xslang` currently parses the function/control-flow subset emitted by the first XMIR writer:

- `.xmir version 0`
- `program <name>` multi-function documents with repeated `function` records, explicit `.function end` boundaries, and one
  final `.program end`
- a structured program-level `types` section containing `aggregate <id> <name>` records with `field <type>` entries and
  `array <id>` records with explicit `element <type>` and either `length <count>` or `length dynamic` entries
- `function <name>`
- `returns <xlil-type>`
- `parameters` with `parameter <name>`, `local <id>`, and `type <xlil-type>` records. A parameter local is immutable and
  identifies the value used by MIR statements and terminators.
- `locals`, including optional `type <xlil-type>` records for values that can lower into XLIL
- `control_flow`
- explicit `.end` section markers and `.program end` document marker
- `block`
- local-use, move, borrow, end-borrow, `const.i64`, `const.i32`, `const.bool`, `store.local`, `load.local`, arithmetic,
  compare, `call`, and drop statements
- `return`, `goto`, `branch_if`, `unreachable`, and `missing` terminators
- optional local return values, goto targets, and named `branch_if` condition/then/else fields
- `analysis optimizer` records for optimization pass reports and removed item counts
- `analysis verify` records for structural verifier diagnostics, spans, and messages

The version header is part of the `xs build` input contract. It tells the compiler which XMIR text grammar version it is
being asked to parse. Version `0` is the only supported XMIR version today; future versions can be accepted explicitly
without changing the structured, human-readable format direction.

Parsed XMIR can be passed to the Rust MIR structural verifier. The current verifier checks duplicate local/block ids,
missing terminators, unknown local references, and unknown block targets before borrow checking.
Typed `store.local`/`load.local` records preserve source-local storage across CFG edges and lower to XLIL stack slots.
The verified optimizer API uses the same structural verifier before and after MIR optimization.

`xs build --output mir` emits the program form from borrow-checked, verified, optimized MIR. The Rust program reader
round-trips every function while the existing single-function reader remains available for focused tools and fixtures.
`xs build --mir -file <program.xmir>` runs the program through MIR verification, borrow checking, optimization, XLIL
lowering/verification, and the existing LLVM native backend. Invalid local/block references fail before backend entry.

For example, a tuple stored in a three-element fixed array carries the registry without using XLIL directives:

```text
types
  aggregate 0 tuple.0
    field i32
    field i32
  .end
  array 0
    element %t0
    length 3
  .end
.end
```

Place projections, drop trees, borrow regions, and optimizer annotations will be added as MIR grows.

Runtime-sized arrays use `length dynamic` in the registry. MIR reads their current count with `statement array.length`;
the statement names its result local, array local, and registry type before XLIL converts it to `len.array`.

## Non-goals

- XMIR is not the backend input language.
- XMIR is not bytecode.
- XMIR is not required to be accepted by `xs build --xlil`.
- XMIR is not a replacement for the public `xs/mir/jit.h` API.
