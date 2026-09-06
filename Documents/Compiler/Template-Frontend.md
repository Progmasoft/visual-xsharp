<!--
SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
-->

# Template Frontend Architecture

This document describes the compiler implementation boundary for Visual X#
template declarations. The language contract remains in `Spec/`; this document
explains how the Haskell frontend preserves that contract for later compiler
stages.

## Scope

The current frontend recognizes template class declarations and carries their
semantic information through:

1. lexical analysis;
2. parsing;
3. renaming;
4. name resolution;
5. type checking;
6. application binding;
7. typed declaration instantiation.

The frontend does not emit an open template declaration as Core. A template is
lowered only after a concrete application has selected a declaration and all
semantic type and value variables have been replaced. This rule prevents Core,
CorePrep, Xpp, and Xmm from acquiring unresolved source-language variables.

Constraint parsing and ordering, explicit instantiation declarations, template
function declarations, template aliases, template extensions, deduction, and
native symbol mangling remain separate work. Their absence must not be hidden
by manufacturing a generic runtime function.

## Parsed representation

`TemplateTypeDeclaration` is distinct from `TypeDeclaration`. It records:

- the class declaration name and source span;
- the ordered template parameter list;
- the class members;
- the annotation assigned by each semantic pass.

Each parameter records one of three categories:

- a type parameter introduced by `typename`;
- a compile-time value parameter introduced by a type and name;
- a template-template parameter introduced by a nested template signature and
  `class`.

Pack status is stored independently from category. This permits a type pack, a
value pack, or a template-template pack without encoding three more unrelated
constructors. A default remains either type syntax or restricted compile-time
value syntax.

Nested template-template parameters use shape records rather than source names.
The source signature `template<typename> class Container` describes what
`Container` accepts; the unnamed inner `typename` does not introduce a binding
in the enclosing class.

The parser never stores a template prefix as text. Doing so would force later
passes to parse the declaration again and could make analyzer and compiler
behavior diverge.

## Lexical rules

`template` and `typename` are keywords. `...` is emitted as one maximal symbol
token. Treating an ellipsis as three period tokens would make pack syntax
dependent on whitespace and complicate diagnostics.

Template value defaults use the same restricted scalar expression tree as
value template arguments. Runtime calls, callable literals, and other
side-effecting expressions cannot enter a specialization identity.

## Renaming

The declaration name is introduced in the surrounding declaration environment.
Template parameter names receive fresh, positive semantic identities in source
order.

All template parameter names enter the template environment before member
bodies are renamed. This is necessary because a default may refer to a later
parameter, as in:

```vxs
template<typename T = U, typename U = int>
class Pair {
}
```

The same environment is visible to member bodies. A value parameter referenced
by a method expression therefore resolves to the parameter's semantic identity,
not to a global with the same spelling.

Parameter environments do not leak between template declarations. Two classes
may both declare `T`, and the resulting symbols remain distinct.

## Name resolution

Name resolution converts every positive renamer identity into a `SymbolId`.
Template declarations and their parameters follow the same zero-sentinel rule
as functions, locals, and captures: zero is never a valid source declaration
identity.

Type syntax intentionally retains source spellings. The typed pass maintains a
template context that maps those spellings to resolved parameter identities.
This avoids putting partially resolved names into `TypeSyntax` while still
ensuring that semantic `Type` values use `ResolvedName`.

## Type checking

A type parameter occurrence becomes:

```text
TypeVariable ResolvedName
```

A value parameter occurrence in a template value position becomes:

```text
TemplateValueParameter ResolvedName
```

For example, the parameter type `[T; N]` produces a `System.Array` semantic
type with an ordered type argument for `T` and value argument for `N`. The
ordered sum is important: type and value arguments cannot be sorted into
separate lists without changing specialization identity.

Template-template parameters behave as type variables while a body is open.
Their parameter shapes are retained on the declaration so application binding
can distinguish them from ordinary type parameters.

Member signatures, local bindings, callable parameters, capture annotations,
and every expression annotation use the same template context. Substitution is
therefore not limited to method boundaries.

An open declaration's own semantic type contains one argument per parameter.
Type and template-template parameters appear as type variables. Value
parameters appear as template value parameters.

## Structural verification

The template verifier checks invariants expected by application binding:

- declaration, parameter, and member symbols are positive;
- parameter symbols and spellings are unique within one declaration;
- a typed parameter does not carry `ErrorType`;
- a parameter pack does not carry a default;
- a default's type/value category matches its parameter category;
- a template-template signature is not empty, including nested signatures.

Multiple declarations with the same qualified name are not a structural error.
Constraint-based specialization intentionally permits that public surface.
Until constraint ordering is implemented, catalog lookup reports such an
application as ambiguous instead of picking a declaration by source order.

## Template catalog

The catalog is built from `TypedAST`. Each descriptor contains:

- the namespace-qualified declaration name;
- the declaration symbol;
- ordered parameter descriptors;
- semantic member signatures.

Ordinary declarations are excluded. Catalog lookup requires an exact qualified
name and never falls back to suffix matching. This keeps namespace behavior
case-sensitive and deterministic.

The minimum arity counts non-pack parameters without defaults. The maximum
arity is exact when no pack exists and unbounded when a pack exists.

## Application binding

Application binding accepts an ordered list of semantic `TemplateArgument`
values. It checks each argument against the corresponding parameter category.

A type parameter accepts a type argument. A value parameter accepts a value
argument. A template-template parameter currently accepts a zero-argument
named type as the semantic reference to a declaration. First-class template
references will replace this temporary representation when constraint and
declaration lookup are connected.

A pack receives all arguments not required by parameters following it. This
allows the binder to preserve a required suffix rather than greedily consuming
the entire application.

Bindings preserve whether an argument was explicit or supplied by a default.
That distinction is useful for diagnostics and reproducible specialization
keys even when both paths produce the same semantic argument.

## Default arguments

Defaults are resolved as a dependency graph. They are not evaluated by a simple
left-to-right fold because the language permits a default to reference a later
parameter.

The resolver follows parameter references until it reaches an explicit binding
or a concrete default. A visited-symbol set rejects cycles such as `T = U` and
`U = T` deterministically.

Concrete integer, Boolean, and character defaults become canonical template
values. Type defaults preserve qualified and nested template argument
structure. Substitution-aware arithmetic folding remains owned by the future
specialization evaluator; the application binder does not silently guess it.

## Typed instantiation

Instantiation accepts a binding and the typed template declaration that
created its descriptor. A mismatched declaration symbol is rejected.

The traversal substitutes annotations in:

- the declaration type;
- member signatures;
- parameters;
- local bindings and assignments;
- returns and branches;
- calls, unary expressions, and binary expressions;
- callable signatures and bodies;
- explicit and implicit capture records.

Source spans, source type syntax, semantic value names, access modifiers,
static status, and statement termination are preserved. The result is an
ordinary `TypeDeclaration`, which makes the existing Core lowering boundary
usable without teaching Core about source templates.

Nested template declarations retain their own parameter ownership and are not
substituted blindly by an outer binding. They must be selected and instantiated
through their own descriptor.

## Core boundary

Desugaring returns no functions for an open `TemplateTypeDeclaration`. This is
intentional, not an empty implementation. Emitting its members would create
invalid Core containing unresolved type variables and would make every open
member appear eagerly instantiated.

After a closed declaration has been produced, ordinary member lowering can be
used. Selection, lazy member reachability, constraint ordering, stable native
name mangling, and insertion into the specialization demand graph are the next
integration responsibilities.

## Diagnostic principles

Template diagnostics must identify the qualified declaration, parameter name,
argument position, expected category, and received category where applicable.
They must not print host-language constructor names as the only explanation.

Unknown and ambiguous declarations are distinct. Too few and too many
arguments are distinct. An unresolved default is distinct from an explicit
argument category mismatch.

Errors are accumulated when multiple supplied arguments have category problems.
Arity errors stop binding because no stable parameter alignment exists.

## Testing strategy

The frontend suite covers lexical maximal munch, all three parameter kinds,
packs, defaults, nested template-template shapes, malformed delimiters,
renaming scope, stable positive identities, typed variables, arrays,
dictionaries, callable types, catalog lookup, arity, category mismatch,
forward defaults, cycles, packs, substitution, and typed-tree instantiation.

Tests inspect semantic constructors and SymbolIds. Merely accepting source text
is insufficient because a parser could accept a template while discarding the
information needed by later stages.

Non-template declarations remain in the same suite as a regression boundary.
Adding template support must not change ordinary class parsing, member
resolution, or Core lowering.

## Next integration slice

The next template compiler slice should connect application discovery to the
catalog and perform declaration selection. It should then:

1. evaluate and order constraints;
2. produce a stable specialization key;
3. instantiate only demanded members;
4. assign a collision-resistant native symbol spelling;
5. insert the closed functions into the existing demand graph;
6. verify that no type or value parameter reaches Core;
7. cache one result per canonical application key.

That work should reuse the binder and instantiation traversal rather than
reconstructing argument matching in Core or the C++ backend.

The integration must also retain deterministic source-order diagnostics while
allowing independent specializations to be prepared concurrently. Concurrency
must never make catalog selection or emitted symbol identity nondeterministic.
