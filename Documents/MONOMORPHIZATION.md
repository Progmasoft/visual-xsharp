<!-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com> -->
<!-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0 -->

# Template monomorphization

Visual X# templates use compile-time monomorphization. They do not use JVM-style
type erasure, a universal boxed representation, or a runtime generic dictionary
as the default execution model. Each required concrete template argument set
produces a concrete declaration with concrete storage and callable signatures.

This is the intended compiler contract. The current frontend transports ordered
type and compile-time value arguments and the native layer can validate,
substitute, identify, and intern concrete type specializations. It does not yet
clone template declarations or execute the complete instantiation engine.
Until that engine lands, documents and diagnostics must not claim that all
template programs are executable.

## Current implementation boundary

The implemented slice includes:

- parsed `[T; expression]` fixed-array sugar;
- unambiguous literal value arguments such as `Buffer<32>`;
- exact integer, character, Boolean, unary, and binary constant evaluation;
- an ordered `TemplateArgument` sum in typed AST, Core, CorePrep, Xpp, and Xmm;
- strict Core/CorePrep v4 and Xpp/Xmm v3 codecs;
- recursive structural validation and parameter collection;
- independent type-parameter and value-parameter substitution;
- deterministic structural identity rendering;
- immutable Haskell worklist planning with atomic batch failure;
- thread-safe native interning of valid concrete types; and
- explicit classification of `[]T`, `System.Array<T>`, and
  `System.Array<T, N>`.

The slice deliberately does not include:

- parsing template declarations into the current small declaration AST;
- declaration-aware disambiguation of a bare identifier in `Example<T>`;
- lookup of source constants used as fixed-array sizes;
- template-template arguments, packs, defaults, or wildcard deduction;
- constraint ordering and specialization selection;
- demand discovery or declaration/body cloning; or
- generated symbol mangling and incremental cache persistence.

A bare identifier in an ordinary angle-bracket argument therefore remains a
type in the current parser. The semicolon in `[T; N]` is unambiguous, but an
unresolved `N` is diagnosed by TypeChecker until constant and template-
parameter declarations enter the semantic environment. Numeric, Boolean, and
character arguments do not have that ambiguity.

## Source contract

The normative syntax and behavior remain in `Spec/Language/Decls.vxs`. In
particular:

- template parameters may represent types, compile-time values, templates, or
  parameter packs;
- constrained declarations may specialize the same template surface;
- members are instantiated lazily when required;
- explicit instantiation requests a concrete class or function; and
- recursive instantiation must be diagnosed when it does not terminate.

Different concrete argument lists denote different concrete types. `Box<int>`
and `Box<String>` do not share nominal identity merely because they came from
the same template declaration.

## Compiler placement

Monomorphization belongs after Renamer, Name Resolution, and Type Checker have
established declaration identity, argument meaning, constraints, and overload
selection. It belongs before final Core is treated as closed, target-independent
input for optimization and CorePrep.

```text
Parsed AST
  -> Renamer
  -> Name Resolution
  -> Type Checker
  -> template demand discovery
  -> constraint/specialization selection
  -> monomorphization
  -> Desugarer / closed Core
  -> Core optimizer
  -> CorePrep
```

LLVM is too late for language template selection. Performing it there would
duplicate source semantics in the backend and would make diagnostics depend on
target lowering.

## Specialization identity

A specialization key must include semantic identity rather than printed text:

- the resolved template declaration `SymbolId`;
- ordered, fully resolved type arguments;
- canonical compile-time value arguments including their declared types;
- resolved template-template arguments;
- expanded pack boundaries; and
- the selected constrained declaration.

Aliases are resolved before key construction. Two source spellings that denote
the same type therefore share one specialization. Conversely, equal short names
from different namespaces remain different.

The key is deterministic and independent of discovery order. It is used for the
in-memory specialization cache and stable symbol derivation, but its serialized
form is a compiler implementation detail rather than a source mangling promise.

### Structural identity now

The implemented identity renderer is intentionally not a linker mangling. It
kind-tags every node, length-prefixes qualified-name components, records ordered
argument boundaries, keeps value kinds distinct, and includes both `SymbolId`
and spelling for unresolved parameters. This prevents collisions between:

- `System.Array<int>` and `System.Array<int, 0>`;
- `Example<int, 4>` and `Example<4, int>`;
- integer `65` and character `'A'`;
- one qualified component named `A.B` and two components `A` plus `B`; and
- two parameter identities that happen to have the same spelling.

The renderer is deterministic for an already resolved `Type`; it does not make
aliases equivalent by itself. Alias resolution and selected declaration
identity must be added before the final monomorphization key is considered
complete.

### Validation and metrics

Validation walks the same structure before interning. It rejects empty names,
missing recursive type payloads, invalid parameter symbols, noncanonical
integer payloads, invalid Unicode character values, negative fixed-array sizes,
and malformed array-family arity or argument kinds. A caller-supplied nesting
limit bounds recursive work.

Metrics report type nodes, type arguments, value arguments, parameter
references, and maximum depth. They are not optimization estimates. Their
purpose is deterministic diagnostics, tracing, and future resource limits.

### Substitution

Type and value binding maps are separate. Substitution descends through named
arguments and callable signatures, replaces only matching positive semantic
identities, and leaves missing bindings intact. That makes partial
specialization representable without manufacturing an `ErrorType`.

The current substitution function transforms types, not declarations. Capture-
avoiding body cloning, ownership reclassification, constraint checking, and
new generated symbols remain work for the monomorphization pass.

### Native specialization table

The native table accepts only structurally valid concrete types. It derives the
internal identity, checks for an existing entry under a shared lock, then
rechecks under an exclusive lock before allocating a positive identifier. Two
parallel compilation jobs racing to intern the same type therefore receive the
same entry.

Identifiers reflect deterministic insertion order within one table; they are
not stable across processes and must not be serialized as an ABI promise.
Snapshots preserve insertion order for tracing. Lookup supports either the
table-local identifier or a concrete structural type.

### Haskell specialization planner

The Haskell Core package owns the target-independent planning boundary. It
accepts separate type and value binding lists, rejects duplicate keys and keys
used for both parameter kinds, performs recursive substitution, validates the
result, and refuses to intern a type while any semantic parameter remains open.

The planner's catalog is immutable. Interning returns an updated catalog and a
flag that distinguishes a new demand from a cache hit. Batch interning threads
that catalog through a request list; if any request is invalid, the operation
returns no catalog at all. Callers therefore cannot accidentally retain a
partially accepted worklist after a later request fails.

Catalog identifiers are positive and follow first-insertion order. Repeated
requests in one batch retain their corresponding result positions but share
the same entry. Lookup is available by table-local identifier or by concrete
structural type, and snapshots are ordered by identifier for deterministic
tracing. As with the native table, these identifiers are not serialized ABI.

## Demand discovery

The compiler starts from reachable non-template declarations, explicit
instantiations, exported ABI requirements, and selected project entry points.
Whenever a checked body requires a concrete template, it places that
specialization key on a work queue.

Processing a specialization may discover further demands. The queue reaches a
fixed point when no unseen key remains. A key has explicit states such as
`Queued`, `Instantiating`, `Complete`, and `Failed`; encountering an
`Instantiating` key through an expanding argument pattern reports recursive
instantiation instead of recursing until host stack exhaustion.

Lazy member instantiation means creating a concrete class does not eagerly
compile every method body. Layout-required fields and bases are instantiated;
method bodies enter the queue when called, explicitly instantiated, or required
by an exported interface.

## Constraint and overload order

Constraint evaluation precedes body cloning. Candidates whose `requires`
conditions are false are removed. The remaining constrained declarations are
ordered by the language's specialization rules. Ambiguity is a source error;
the compiler must not select whichever candidate happened to be discovered
first.

Overload resolution records the selected template and concrete arguments. The
monomorphizer consumes that decision rather than performing a second, potentially
different overload search.

## Produced declarations

A concrete specialization has no unresolved template parameter in its external
type, field layout, callable signature, ownership classification, or Core body.
Substitution is capture-avoiding and preserves the `SymbolId` distinction
between source declarations and generated concrete declarations.

Generated symbols carry provenance back to the template declaration and
argument list for diagnostics. User-facing messages display source names and
arguments; raw generated symbol spellings stay internal.

Every produced declaration passes the same Core verifier as an ordinary
declaration. Monomorphized output receives no verifier bypass merely because its
source template was checked earlier.

## Ownership and callable interaction

Monomorphization occurs before ownership classification becomes final for a
concrete nominal type. A field of template parameter `T` may become a trivial
scalar, CoW value, or AARC reference in different specializations. The concrete
result determines layout, destructor work, and Xpp ownership operations.

Callable templates receive concrete public signatures before closure conversion
is finalized. Lifted capture prefixes and invoke thunks then use those concrete
types; no runtime generic argument is appended to the callable ABI by default.

## Code size and deduplication

Distinct semantic specializations remain distinct even if their generated LLVM
instructions happen to be identical. Later optimization and linker identical-
code folding may merge machine code where ABI and address-identity rules permit,
but that is not type erasure and does not change language identity.

The compiler should report specialization counts and depth in diagnostics or
build traces. It may cache verified concrete Core by specialization key plus
compiler/version/input fingerprint. A cache hit must reproduce the same symbols
and dependencies as fresh instantiation.

## Required diagnostics

The completed engine must diagnose at least:

- missing or excessive template arguments;
- a wrong parameter kind;
- a failed or ambiguous constraint set;
- invalid pack expansion;
- an unavailable explicit-instantiation target;
- recursive specialization without a finite fixed point;
- an instantiated body that is invalid for the concrete arguments; and
- implementation resource limits with an instantiation trace.

The trace should show the source demand chain without exposing internal Core or
LLVM names.

## Testing boundary

Tests must cover identity canonicalization, alias equivalence, namespace
separation, value arguments, packs, constrained selection, lazy members,
explicit instantiation, recursive failure, deterministic discovery order,
ownership-class changes, closure signatures, serialization, and cache
reproducibility.

Until those behavior tests and the instantiation pass exist, monomorphization is
a documented architectural decision and roadmap item—not a completed compiler
feature.
