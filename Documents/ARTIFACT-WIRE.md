<!-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com> -->
<!-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0 -->

# Artifact wire contracts

Visual X# uses bounded binary documents at compiler stage boundaries. They are
compiler artifacts rather than source formats. Core, Xpp, and Xmm are public
`-Emit`/`-Build` choices; CorePrep remains a private adapter contract.

## Contract families

| Contract | Magic | Current version | Producer | Consumer |
| --- | --- | ---: | --- | --- |
| Core | `VXCR` | 3 | Haskell frontend | native Core reader |
| CorePrep | `VXCP` | 3 | CorePrep adapter | native pipeline tools |
| Xpp | `VXPP` | 1 | verified CorePrep-to-Xpp lowering | Xmm lowering or artifact tools |
| Xmm | `VXMM` | 1 | verified Xpp-to-Xmm lowering | LLVM backend or artifact tools |

The contracts have related scalar encodings but separate structural schemas.
Their magic values must never be treated as aliases.

Core and CorePrep have a Haskell producer. Xpp and Xmm are C++20-owned
contracts on both sides of their codec. This distinction does not weaken the
reader boundary: an artifact loaded from disk is untrusted and is verified
again before the next lowering stage.

## General encoding rules

- Multibyte counts and identifiers are little-endian.
- Integer literal magnitude bytes are big-endian.
- Every variable-size field is checked before allocation.
- The reader must consume the whole input.
- Unknown enum tags are errors, not extension records.
- An unsupported version fails before body decoding.
- A producer emits only its declared current version.

These rules keep decoder behavior deterministic and make corrupt artifacts
fail locally instead of reaching a later compiler pass.

## Decoder limits

A limit is part of the decode API rather than a global mutable setting. The
current contracts bound modules, functions, parameters, statements, blocks,
instructions, operands, text bytes, numeric bytes, and nesting depth where the
model is recursive.

Callers may reduce limits for a constrained context. Increasing a limit does
not relax semantic checks: an integer that passes the byte-count bound must
still fit its declared scalar width.

## Scalar type tags

Wire v3 assigns an explicit tag to unit/no-result, boolean, string, function,
named, variable, character, every signed and unsigned integer width, and every
floating width. A decoder reconstructs the exact type; it does not infer width
from the literal byte count.

That separation is required because the same magnitude can inhabit several
types and because signedness affects native instruction selection even when
the bit pattern is identical.

### Core v3 scalar tag map

The native and Haskell Core codecs use the following assignments for version
3. This table is an implementation-maintenance aid, not a user extension API.

| Tag | Type | Tag | Type |
| ---: | --- | ---: | --- |
| 0 | no-result/unit marker | 11 | `longint` |
| 1 | `bool` | 12 | `ubyte` |
| 2 | compatibility `int` slot | 13 | `ushort` |
| 3 | `String` | 14 | `ulong` |
| 4 | named type | 15 | `uint` |
| 5 | function type | 16 | `ulongint` |
| 6 | type variable | 17 | `sfloat` |
| 7 | `char` | 18 | `lfloat` |
| 8 | `byte` | 19 | `float` |
| 9 | `short` | 20 | `double` |
| 10 | `long` | | |

The compatibility entry retains an in-memory historical slot. It does not
create another source spelling, and new numeric values still carry their exact
declared scalar type.

### CorePrep v3 scalar tag map

CorePrep retains historical `int` and `long` positions before the extended
catalog. Its assignments must therefore not be copied blindly from Core:

| Tag | Type | Tag | Type |
| ---: | --- | ---: | --- |
| 0 | no-result/unit marker | 11 | `ushort` |
| 1 | `bool` | 12 | `ulong` |
| 2 | `int` | 13 | `uint` |
| 3 | `long` | 14 | `ulongint` |
| 4 | `String` | 15 | `sfloat` |
| 5 | function type | 16 | `lfloat` |
| 6 | named type | 17 | `float` |
| 7 | type variable | 18 | `double` |
| 8 | `char` | 19 | `byte` |
| 9 | `longint` | 20 | `short` |
| 10 | `ubyte` | | |

The distinct table is one reason magic and version are verified before any
type record is decoded.

## Integer payloads

The payload is sign plus normalized unsigned magnitude. Canonical rules are:

- zero has no magnitude bytes;
- zero is never negative;
- a nonzero magnitude does not start with `00`;
- sign is encoded only as `00` or `01`; and
- magnitude order is most significant byte first.

Examples:

| Value | Sign | Magnitude |
| ---: | --- | --- |
| `0` | `00` | empty |
| `1` | `00` | `01` |
| `-1` | `01` | `01` |
| `255` | `00` | `ff` |
| `256` | `00` | `01 00` |

The payload itself is unbounded in meaning and bounded in transport. The type
verifier decides whether it fits `byte`, `int`, `longint`, or another target.

### Integer rejection order

Readers reject structural defects before applying the declared type range:

1. a sign byte other than zero or one;
2. a byte count above the caller's numeric limit;
3. insufficient bytes for the declared count;
4. negative zero or a leading-zero magnitude;
5. a payload alternative that does not match the scalar family; and
6. a mathematical value outside the declared width.

This ordering keeps hostile length fields from allocating memory and keeps
semantic diagnostics separate from transport corruption.

## Floating payloads

Floating values are transported as ASCII spelling. The format permits decimal
finite values and explicit lowercase `nan`/`inf` forms. The decoder validates
ASCII and grammar before constructing the IR literal.

Preserving spelling avoids host floating conversion and retains enough input
for LLVM to round directly to half, float, double, or fp128.

The transport grammar is deliberately smaller than a future source grammar
might be. Source separators are normalized away before Core, and target suffix
policy is already resolved by TypeChecker. The wire carries a semantic number
spelling rather than the original token text.

## Text payloads

Names and string literals are distinct fields with their own size accounting.
String decoding must reject invalid encoded text rather than replacing it.
Symbol identity never depends only on spelling: resolved symbol identifiers
remain part of the structural records.

## Core document order

A Core document contains:

1. magic and version;
2. qualified module name;
3. function count;
4. each resolved function symbol;
5. parameters and their types;
6. return type; and
7. ordered Core statements and expressions.

Expression records include their result type. The verifier checks that the
recorded type agrees with constants, variables, applications, and primitives.

## CorePrep document order

A CorePrep document contains:

1. magic and version;
2. qualified module name;
3. function count;
4. resolved function metadata;
5. explicit entry block identifier;
6. blocks, instructions, and operands; and
7. one terminator per block.

CorePrep serializes the adapter model after evaluation order and control flow
are explicit. It must not be exposed as a stable package or CLI artifact.

## Xpp and Xmm common scalar records

Xpp and Xmm share a C++20 artifact support layer for types, literals, Unicode
text, qualified names, and resolved symbols. They do not share instruction or
control-flow records. Reusing only scalar records prevents a generic object
serializer from erasing which stage owns an invariant.

The v1 Xpp/Xmm type tag catalog follows the native Core type model exactly:

| Tag | Type | Tag | Type |
| ---: | --- | ---: | --- |
| 0 | no-result/unit marker | 11 | `uint` |
| 1 | `bool` | 12 | `ulongint` |
| 2 | `char` | 13 | `sfloat` |
| 3 | `byte` | 14 | `lfloat` |
| 4 | `short` | 15 | `float` |
| 5 | `int` | 16 | `double` |
| 6 | `long` | 17 | `String` |
| 7 | `longint` | 18 | function type |
| 8 | `ubyte` | 19 | named type |
| 9 | `ushort` | 20 | type variable |
| 10 | `ulong` | | |

Literal tags are independent of the type tag:

| Tag | Payload |
| ---: | --- |
| 0 | unit/no-result |
| 1 | boolean byte |
| 2 | canonical sign plus integer magnitude |
| 3 | validated ASCII floating spelling |
| 4 | Unicode-scalar string |

The reader validates the literal against its separately encoded type before
returning a stage model. Legacy host-width integer alternatives accepted by
the in-memory model are normalized to the canonical arbitrary-width payload
when written; decoding never recreates a host-width alternative.

## Xpp document order

An Xpp v1 document contains:

1. `VXPP`, version, and zero reserved flags;
2. qualified module name;
3. ordered resolved functions;
4. parameter symbols/types, return type, and entry block;
5. ordered blocks and instructions;
6. instruction effect, opcode, destination, result type, and operands;
7. closure function identity and capture modes; and
8. one typed terminator with explicit CFG targets per block.

The instruction effect preserves definition, store, and discard as distinct
operations. A destination of zero is therefore meaningful only for discard.
The codec does not derive effect from the opcode or fabricate storage.

Xpp function operands carry stable symbol identity plus their callable type.
They are not serialized as storage reads. Closure targets similarly occupy a
dedicated symbol field rather than an untyped extra operand.

## Xmm document order

An Xmm v1 document contains:

1. `VXMM`, version, and zero reserved flags;
2. qualified module name;
3. ordered functions and resolved function identity;
4. parameter virtual registers and a parallel ordered type vector;
5. return type and entry block;
6. blocks and instructions;
7. opcode, destination register, result type, explicit has-result bit, values,
   closure function, and capture modes; and
8. a typed terminator with CFG targets.

Xmm values have three disjoint tags: data register, immediate, and function.
This prevents a direct function call from consuming a virtual register or an
immediate from being interpreted as register zero. Parameter register/type
vectors remain separate because the ABI cannot be reconstructed from first
instruction use.

## Public conversion routes

The CLI connects the public stage artifacts without exposing CorePrep:

```powershell
vxs build -File Main.vxs -Emit xpp
vxs build -Build xpp -File Main.xpp -Emit xmm
vxs check -Build xmm -File Main.xmm
vxs build -Build xmm -File Main.xmm -Emit llvmll
```

An artifact can move forward or be rewritten at its current stage. It cannot
be raised back to an earlier representation: Xmm cannot emit Xpp or Core, and
Xpp cannot emit Core. `check` writes nothing and continues through the next
connected verification/lowering boundary. `build` replaces the selected
output only after the input has decoded and verified successfully.

Stage optimization flags apply at the owning boundary. A loaded Xpp/Xmm
document is verified before optimization, optimized only when enabled, and
verified again before serialization or forward lowering.

## Compatibility policy

The version field describes the entire schema. Core/CorePrep version 3 is not
a permissive extension of version 2: their scalar type and literal tag spaces
changed. Xpp/Xmm begin independently at version 1. Every current reader rejects
earlier and future versions for its own magic.

If migration is needed later, it should be implemented as an explicit reader
for the old version followed by model conversion. The current decoder must not
guess which schema produced an ambiguous tag.

Golden documents are named with their version and live beside the component
that owns the contract test. Changing the current version requires changing
both implementations and golden expectations in the same commit.

## Error behavior

Decode errors carry a stable category and a byte offset. Useful categories
distinguish at least:

- invalid magic;
- unsupported version;
- unexpected end of input;
- unknown tag;
- invalid scalar literal;
- invalid text;
- configured limit exceeded; and
- trailing bytes.

The decoder does not print. The CLI or embedding caller owns presentation and
exit behavior.

## Security properties

Artifacts are untrusted input even when generated by a local frontend. A
decoder must validate lengths before reserving memory, avoid recursive work
beyond its configured depth, detect count multiplication overflow, and never
use unchecked partial list operations.

Semantic verification runs after structural decode and before lowering. A
successfully decoded module is not automatically a valid module.

## Deterministic production

Writers preserve declaration and instruction order established by the owning
IR. They do not serialize pointer values, hash-map iteration order, host-sized
integers, native endianness, or locale-dependent number formatting.

Equivalent verified input therefore produces equivalent bytes on supported
development hosts.

## Test matrix

Every contract revision should cover:

- a minimal golden document;
- round trips for every type tag;
- minimum and maximum integer values for every width;
- negative values and zero canonicalization;
- representative finite and special floating spellings;
- non-ASCII text where permitted;
- each truncated field boundary;
- every unknown tag family;
- old and future version rejection;
- each configurable limit; and
- trailing-byte rejection.

Cross-language tests are essential for Core/CorePrep. Xpp/Xmm instead require
stage-crossing tests: lowered output, codec round trip, verifier, and the next
lowering owner must all agree. A codec-only equality test can preserve a model
bug just as easily as a single-language Core round trip.

When one side changes a tag, a golden-byte test should fail before a large
module reaches LLVM. When one side changes only a verifier rule, the same bytes
may decode, but semantic verification must reject the module consistently.

## Change checklist

Before changing a wire contract:

1. identify the owning IR change;
2. decide whether the change requires a version increment;
3. update Haskell and C++ tag tables together;
4. update writer and reader limits;
5. update structural and semantic verifiers;
6. add cross-language boundary cases;
7. replace versioned golden documents;
8. update the public implementation documents; and
9. remove generated artifacts after local verification.

The wire format follows the compiler model. It must not become a second,
independent language specification.
