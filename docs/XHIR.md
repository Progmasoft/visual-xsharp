<!--
SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
SPDX-License-Identifier: MPL-2.0
-->

# XHIR text format

`.xhir` is the human-readable text form of XHIR, the normalized operational side of X# HIR. It is not assembly-like, not
binary, and not an opaque compiler serialization format.

HIR is split into coordinated THIR and XHIR sides. THIR carries the typed, source-oriented semantic structure; XHIR carries
the normalized high-level operations used by MIR lowering. These are parts of one HIR layer, not unrelated pipeline IRs.
A `.xhir` document represents XHIR only and is not a textual THIR dump.

XHIR exists for MIR input and semantic inspection: names, modules, import, declarations, resolved types, generic
parameters, interfaces, macro expansion results, and expression/type-check information. It should read like a structured
semantic tree, not like an instruction stream.

## Design rules

- The format is deterministic, newline-normalized, UTF-8 text.
- Source-order is preserved unless a section explicitly says it is sorted.
- The top level is a named document with nested records.
- Records use stable field names instead of positional operands.
- Indentation is for readability only; parsers must use explicit records such as `.end` and `.program end`, not leading
  spaces, to determine structure.
- Identifiers, paths, and types are printed in their X# spelling where that spelling is still meaningful.
- Compiler-generated ids are allowed only when they help diagnostics or cross-reference repeated records.
- XHIR must remain suitable for code review and text fixtures.
- XHIR must not use XLIL-style directives such as `.func`, `.extern`, or `%r0:type = op`.

## Shape

The exact grammar will be tightened as HIR stabilizes, but the intended shape is record/tree oriented:

```text
.xhir version 1
module App

import
  import std::io
.end

declarations
  function Main
    visibility public
    signature
      parameters []
      returns ()
    body
      block
        let message
          type Str
          value string "Hello from X#"
        call std::io.println
          arguments [message]
.end
.program end
```

This is intentionally closer to a semantic document than to assembly. A formatter should be able to align indentation, and a
reviewer should be able to understand which declaration or expression changed without reconstructing a low-level CFG.

## Current parser subset

Rust `xslang` currently parses the module-symbol and checked-function subsets emitted by the first XHIR writer:

- `.xhir version 1`
- `program <name>` multi-function documents with repeated `function` records, explicit `.function end` boundaries, and one
  final `.program end`
- an optional leading `declarations` section with `data` records, ordered base records, typed fields, explicit field
  mutability, and `.end` delimiters; direct native lowering currently accepts `data` declarations while retaining other
  nominal kinds for later semantic support
- `module <name>` with `import`, `declarations`, and `symbol` records
- `function <name>` with `signature`, `locals`, and `body`
- `parameters` records before `locals`; each `parameter <name>: <type> <mutability>` preserves a leading function ABI
  entry separately from ordinary storage locals
- explicit `.end` section markers and `.program end` document marker
- primitive and named type records
- literal, local, assignment, prefix/postfix update, typed direct-call, conditional block, loop, classic-for, fixed-array
  for-each,
  statement-match, short-circuit logical binary, `propagate`, let, expression, and return records
- `analysis typecheck` records for type-check diagnostics, spans, and messages

The version header is part of the `xs build` input contract. Canonical writers emit XHIR version `1`. Readers accept
versions `0` and `1`, preserving existing artifacts without silently accepting an unknown future grammar.

Class, interface, and data declarations retain their nominal category, base relations, and available field layouts.
Generic parameter declarations/constraints, macro-expansion records, and richer expression forms will be added as the
checked HIR model stabilizes.

`xs build --output hir` emits the program form. The Rust program reader splits functions only at `.function end`; indentation
does not participate in parsing. A single-function XHIR document remains supported by the function reader.
`xs build --hir -file <program.xhir>` reads the program form, type-checks it, lowers it through MIR and verified XLIL, and
uses the native backend when every represented operation is supported.
Tuple and fixed-array registry ids are not serialized as low-level XHIR directives. The direct compiler reconstructs their
deterministic layouts from the higher-level tuple/array types carried by function signatures, locals, and statements.
Nominal data layouts are similarly kept source-oriented and explicit:

```text
declarations
  data Named
    field label: Str immutable
  .end
  data Point
    base Named visibility internal
    field x: Long mutable
    field y: Long immutable
  .end
.end
```

Each base record preserves source order, access, and the optional trailing `virtual` marker. Native `data` layout is
base-first and recursively expands bases in base-list order before the declaration's own fields. Duplicate nominal names,
unknown or cyclic bases, cross-category bases, and ambiguous inherited field names are rejected before lowering. Virtual
inheritance is retained by XHIR but does not yet have a value layout and is rejected by direct native lowering.

Payload-free normal enums use nominal declaration records with ordered variants:

```text
enum Color
  variant Red
  variant Green
.end
```

Variant values remain semantic XHIR literals such as `literal enum Color::Green tag 1`. The tag is checked against the
declaration and carried toward MIR; X# does not expose enum-to-integer conversion.

Enum-data declarations retain their source-level payload overloads:

```text
enum data Value
  variant Number: Int
  variant Number: Long
  variant Text: String
.end
```

The declaration-local order remains visible in XHIR. Semantic analysis additionally builds a target-independent resolved
variant registry. Base declarations are traversed in source order before the derived declaration's own variants. A shared
ancestor reached through multiple base paths contributes its variants once. The resolved registry assigns a flattened
`u32` tag sequence beginning at zero for each concrete enum-data type; these runtime tags do not replace the
declaration-local tags stored in XHIR.

A typed variant constructor is selected by exact payload type. Nominal payloads compare by nominal identity, so two data
types with identical fields do not select each other's overload. Primitive, optional, result, reference, collection, and
tuple payloads preserve their full checked type structure during selection. No numeric widening or structural conversion
is performed merely to choose an enum-data variant.

Selected values are explicit records rather than ordinary calls:

```text
enum_data Value::Number owner Value tag 1 payload Long
  value
    literal integer 7
.end
```

The `owner` is the declaration that introduced the selected overload. `tag` is the flattened hierarchy tag, so inherited
constructors preserve their declaration identity while receiving the tag used by the target enum-data type. A
payload-free variant uses `payload ()` and omits the `value` record. The XHIR parser and type checker reject a record whose
owner, tag, payload presence, or payload type does not select the same registry entry.

The hierarchy is rejected before MIR lowering when it contains an unknown or cross-category base, an inheritance cycle,
two distinct variants with the same name and payload type, or an overload set containing a payload-free variant. A
diamond-shaped hierarchy is valid when the repeated variant identity comes from the same original ancestor.

A typed direct call is represented as a semantic record rather than an instruction:

```text
call add : (Long, Long) -> Long
  argument
    literal integer 3
  argument
    literal integer 4
```

Parameter types are part of checked HIR. Commas inside nested generic types do not split the outer parameter list.
Unit is written as `()` in call signatures. A call used as a semicolon-terminated statement remains a call record; MIR
decides whether it has no result register (`-> ()`) or evaluates and discards a typed value result.

Increment and decrement preserve whether the source expression returns the old or updated binding value:

```text
update postfix increment value
update prefix decrement value
```

MIR lowering turns these semantic records into explicit load, integer operation, store, and selected result-value steps.

`logical_and` and `logical_or` remain typed semantic expressions in XHIR. MIR lowering expands them into branch, short,
right-operand, and merge blocks so `&&` and `||` never become eager binary instructions.

Statement match records retain the checked selector type and semantic arms. A final `else` arm is required:

```text
match Long
  selector
    local value
  arm literal integer 2
    body
      return
        literal integer 7
    .end
  arm else
    body
      return
        literal integer 3
    .end
.end
```

Conditional expressions retain semantic blocks and their checked result type:

```text
if_expression Long
  condition
    local enabled
  then
    tail
      literal integer 1
  .end
  else
    tail
      literal integer 2
  .end
```

The `.end` records, rather than indentation, delimit each branch.

Classic `for` records retain the semantic header positions instead of encoding them through indentation:

```text
for
  initializer
    let index
      type Long
      mutability mutable
      initializer
        literal integer 0
  condition
    binary lt
      left
        local index
      right
        literal integer 3
  update
    assign index
      binary add
        left
          local index
        right
          literal integer 1
  body
    expression
      local index
  .end
.end
```

Any omitted initializer, condition, or update section is absent from the record. The body and final `.end` remain required.

Fixed-array for-each preserves its checked collection and element types before MIR expands it into index-based control
flow:

```text
for_each value
  type Long
  iterable_type [Long; 3]
  iterable
    local values
  body
    expression
      call consume : (Long) -> ()
        argument
          local value
  .end
```

The element binding is immutable and scoped to the body. This record is semantic XHIR; it is not an iterator instruction.

## Non-goals

- XHIR is not executable input for a backend.
- XHIR is not THIR, and `.xhir` is not a THIR serialization.
- XHIR is not a lossless source-code pretty-printer.
- XHIR is not the public XLIL API surface.
- XHIR is not required to preserve trivia such as comments or exact whitespace.
