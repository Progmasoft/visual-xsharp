<!-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com> -->
<!-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0 -->

# Core intermediate representation

## Scope

Core is the last tree-shaped, target-independent representation in the Visual
X# frontend. It is produced by the Haskell Desugarer, optimized in Haskell, and
then adapted to CorePrep. Core is also the first public compiler artifact in
the pipeline: a verified module can be encoded as a bounded `.core` document.

Core is not source syntax. It contains resolved identities and checked types,
not unresolved identifiers, overload candidates, parser recovery nodes, or
source-level shorthand. It is also not a backend IR. It has no LLVM types,
target registers, calling conventions, object layout, or linker directives.

## Module model

A `CoreModule` contains:

- one qualified namespace name; and
- an ordered list of Core functions.

The current source driver emits one module per compiled namespace and selects
the configured entry namespace for the native boundary. Cross-namespace imports
and a multi-module link unit are later semantic work. The optimizer does not
merge namespaces or resolve an external name by spelling.

An empty module name or an empty name segment is invalid. Module order is
deterministic and follows the frontend's stable declaration order.

## Symbol identity

Every function, parameter, binding, capture, assignment target, and variable
reference uses `ResolvedName`:

```text
ResolvedName
  SymbolId          semantic identity
  Identifier        diagnostic spelling
```

`SymbolId` zero is reserved as the native/wire “no symbol” sentinel. Real
semantic identities are positive. Negative values can occur only in an
incomplete resolution path and are rejected before Core becomes valid.

Spelling is not identity. Two scopes can use the same spelling with different
symbols, and a renamed source spelling can retain the same symbol. Optimizers,
codecs, and native adapters must compare `SymbolId` when binding or looking up
a value.

## Type model

Core reuses the resolved frontend `Type` model:

- named types with qualified names and type arguments;
- function types with ordered parameters and one result;
- resolved type variables; and
- `ErrorType`, which is forbidden in verified Core.

Named scalar types keep their Visual X# spelling. Core does not replace `int`
with an LLVM integer width or encode `String` as host bytes. Type lowering is a
later stage decision.

Source `void` is checked before Core. At the current boundary, resultless
functions use the historically named `unit` marker plus `CoreUnit` as the
explicit return marker. Visual X# has no source-language `unit` type. Source-
facing tools must spell the result `void` and keep this representation private.

## Functions

A `CoreFunction` contains:

- a positive resolved function name;
- ordered resolved parameters and their types;
- one resolved return type; and
- an ordered statement body.

Function symbols are placed in the verifier environment before any function is
checked. Direct calls can therefore refer to a function declared later in the
module. Duplicate function symbols are invalid even if spellings differ.

Parameters are immutable Core storage. Their symbols must be unique within the
function and their types must be fully resolved.

A value-returning function must return on every reachable path represented by
the tree. A source `void` function still carries an explicit
`CoreReturn CoreUnit` after lowering; both `CoreUnit` and its type are private
no-result markers.

## Statements

Core has five statement forms.

### Bind

`CoreBind` introduces typed local storage with:

- a positive resolved name;
- a declared type;
- a mutability bit; and
- an initializer expression.

The initializer is evaluated before the name becomes available to following
statements. Its expression type must equal the declared type. A local symbol
cannot redefine a parameter, function, or earlier binding visible in the same
Core environment.

### Assign

`CoreAssign` writes an existing mutable binding. The target must be defined,
positive, and mutable. Its source expression must have exactly the declared
storage type.

Assignment is a statement, not a value expression. An optimizer may remove a
dead write only when it preserves evaluation effects of the right-hand side and
retains the storage declaration required by any surviving write.

### Return

`CoreReturn` terminates the current function or closure body with one explicit
expression. The expression type must equal the owning return type.

Statements physically present after a return are verified but are unreachable
and may be removed by control-flow simplification. Verification of unreachable
input prevents malformed hidden subtrees from crossing the public artifact
boundary.

### If

`CoreIf` contains one condition and independent ordered true and false statement
lists. Conditions accept `bool` or a numeric scalar. Numeric zero is false and
nonzero is true.

Bindings introduced inside a branch do not escape into the environment after
the `CoreIf`. CorePrep later turns this tree into explicit branch, jump, and join
blocks.

### Evaluate

`CoreEvaluate` evaluates an expression and discards its result. It represents
source call statements and preserves effects when an optimizer removes a dead
binding or branch wrapper.

A pure `CoreEvaluate` can be deleted. Calls and closure construction are
conservatively effectful and remain explicit.

## Expressions

Every Core expression has a statically queryable type.

### Variable

`CoreVariable` refers to a resolved symbol and repeats the expected type. The
verifier checks both existence and exact agreement with the declaration. The
repeated type keeps consumers local and deterministic; they need not rerun type
inference to inspect an expression.

### Literal

`CoreLiteral` pairs a payload with its scalar type. Payload forms are:

- arbitrary-precision host `Integer`, range-checked against the Visual X# type;
- exact floating source spelling, validated without host rounding;
- Unicode scalar `String` content;
- boolean; and
- the internal `CoreUnit` no-result marker.

The payload and declared type must match. For example, an integer payload is
not valid merely because its number could later convert to a float.

### Apply

`CoreApply` contains a callee expression, ordered argument expressions, and a
result type. The callee must have a `FunctionType`. Argument count, argument
types, and result type must exactly match that function type.

Core does not encode overload selection or default arguments. Those decisions
must already be complete.

### Primitive

`CorePrimitive` represents the target-independent operator set:

- add, subtract, multiply, divide, floor divide, and remainder;
- less-than, less-equal, greater-than, greater-equal, equal, and not-equal;
- logical and/or; and
- arithmetic negate and logical not.

Arithmetic operands must be numeric and use the same type. Comparisons return
`bool`. Logical operands accept bool or numeric context and return `bool`.
Unary primitives take one operand; other primitives take two.

### Closure

`CoreClosure` contains:

- ordered captures;
- ordered callable parameters;
- a return type;
- a nested Core statement body; and
- a callable expression type.

Its expression type must be a `FunctionType` whose parameter and return types
match the closure declaration. A value-returning closure body must return on every
path.

## Captures

A Core capture records:

- strong, weak, or unowned capture mode;
- the resolved name visible inside the closure;
- its declared type; and
- the initializer evaluated in the enclosing environment.

Capture and parameter symbols must each be unique in their own lists. Capture
initializers cannot read the capture they are defining; they are verified in
the enclosing environment. The initializer type must equal the capture type.

Closure conversion in CorePrep lifts the nested body into a function and places
hidden capture parameters before explicit callable parameters. Core itself
retains the structured closure because it is the better boundary for capture
analysis and target-independent optimization.

## Scalar widths

Core integer validation uses these exact ranges:

| Type | Range |
| --- | --- |
| `char` | 0 through 2^32 - 1 |
| `byte` | -2^7 through 2^7 - 1 |
| `short` | -2^15 through 2^15 - 1 |
| `long` | -2^31 through 2^31 - 1 |
| `int` | -2^63 through 2^63 - 1 |
| `longint` | -2^127 through 2^127 - 1 |
| `ubyte` | 0 through 2^8 - 1 |
| `ushort` | 0 through 2^16 - 1 |
| `ulong` | 0 through 2^32 - 1 |
| `uint` | 0 through 2^64 - 1 |
| `ulongint` | 0 through 2^128 - 1 |

Floating types are `sfloat`, `lfloat`, `float`, and `double`. Their payload is
still exact normalized spelling in Core; Xpp/Xmm and LLVM own binary semantic
lowering.

## Verification boundary

`verifyCore` checks the complete module and accumulates diagnostics. It does not
stop after the first malformed statement. Diagnostic groups cover:

- module and function identity;
- resolved types;
- duplicate parameters, bindings, captures, and closure parameters;
- definite returns;
- binding and assignment storage rules;
- condition compatibility;
- variable lookup and type agreement;
- call signatures;
- primitive arity, operand, and result types;
- literal payload/range validity; and
- closure callable/capture contracts.

The verifier returns the original module on success. This makes it convenient
to compose at boundaries without introducing a second “verified Core” tree that
could drift from the wire and optimizer models.

## Optimization boundary

The Core optimizer runs only after verification and verifies its result again.
Its passes may:

- propagate immutable literal bindings;
- fold exact integer and boolean primitives;
- select known branches;
- remove unreachable statements;
- delete unused pure bindings, writes, and evaluations; and
- optimize nested closure bodies.

It may not change symbol identity, infer a missing type, ignore a malformed
call, lower a closure layout, or introduce backend concepts. See
[Core optimization](CORE-OPTIMIZER.md) for pass behavior and effect rules.

## CorePrep adaptation

CorePrep converts nested expression evaluation to atoms and operations. It:

- introduces deterministic temporary symbols;
- creates explicit basic blocks;
- translates `CoreIf` to branch/jump structure;
- lifts closure bodies to functions;
- materializes closure creation operations; and
- verifies targets, definitions, operation types, and terminators.

CorePrep is internal and has no public file extension or `-Emit` option. It
exists to adapt tree-shaped Core to Xpp without forcing the Core optimizer or
native Xpp lowering to reconstruct evaluation order.

## Wire representation

Public `.core` files use the bounded `VXCR` contract documented in
[Artifact wire](ARTIFACT-WIRE.md). The wire preserves:

- qualified names and identifier spellings;
- positive `SymbolId` values;
- recursive types within a depth budget;
- exact scalar payloads;
- statement and expression order;
- capture modes and callable types; and
- all function and closure bodies.

Readers enforce document, collection, string, numeric payload, and recursion
limits before constructing an accepted module. Decoding is followed by semantic
verification. A magic or version for another representation is not guessed or
accepted as Core.

## Determinism

Equal verified modules encode to equal bytes and optimize to equal trees under
equal options. Determinism relies on ordered module lists, ordered statements,
stable symbols, exact literal payloads, and bounded codecs.

Human-readable spellings are retained for diagnostics but never regenerated
from map iteration. Optimizer maps and sets affect lookup or membership only;
they do not reorder emitted declarations.

## Ownership rules for contributors

Change Core only when a source semantic needs a target-independent typed form.
Before extending it:

1. update the Core data model;
2. define expression typing and symbol ownership;
3. add verifier acceptance and rejection rules;
4. update the bounded wire format with an explicit version decision;
5. update the optimizer's effect, symbol, metric, and nested-expression walks;
6. update CorePrep adaptation and verification;
7. update the native Core reader/verifier if the form crosses `.core`;
8. add round-trip and malformed-document tests;
9. add frontend-to-Core and Core-to-CorePrep integration tests; and
10. document current native support without claiming a later stage is connected
    before its implementation exists.

Do not place source parser recovery nodes, project DSL settings, LLVM objects,
or target ABI state in Core. Those belong to their owning layers.
