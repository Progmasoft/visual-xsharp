<!-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com> -->
<!-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1 -->

# Match, if expressions, guard and nested blocks

This page describes what the compiler implements today for `match`, for `if`
used as an expression, for `guard` and for a block written as a statement:
what is accepted, in what order things are evaluated, and where the
implemented subset ends. The language design is in `Spec/Language/Decls.vxs`,
sections 31 and 32; the diagnostics are listed in
[Diagnostics](DIAGNOSTICS.md); the lowering is described in
[Core IR](CORE-IR.md).

## Match

```vxs
int kind = match (code), (strict) {
    (0), (_) -> 10,
    (1), (true) -> 20,
    (int other), (_) if other < 0 -> 0 - other,
    (_), (_) -> { int rest = code - 1; rest + 30 }
};
```

- Every subject has its own parentheses, and every arm has one pattern for
  each subject. Parentheses around a pattern are optional.
- The subjects are evaluated once, left to right, before any arm is tested.
- The arms are tested in source order. The first arm whose patterns accept
  the subjects and whose guard holds is selected; exactly one body runs.
- A guard, `pattern if condition ->`, is evaluated only when the patterns of
  its arm accept. A guard that is false passes the subjects on to the arms
  after it. A guard is `bool` or numeric; a numeric guard holds when it is
  not zero.
- The body is an expression or a block. The comma after a block body is
  optional; after an expression body it is required unless the arm is the
  last one.

### Patterns

| Pattern | Accepts | Notes |
| --- | --- | --- |
| a literal, `1`, `true`, `'a'` | the value equal to it | typed from its subject; has no sign |
| `_` | every value | |
| `Type name` | every value of the subject's type | binds the subject as an immutable local |
| `Type _` | every value of the subject's type | binds nothing |

A binding is in scope in the guard and the body of its own arm only. Two arms
may bind the same name; one arm may not bind a name twice, and a binding may
not reuse a name that is already in scope. A bare name is not a pattern: a
binding always states its type.

`null`, enum case patterns such as `.Ready`, and type patterns that name
another type than their subject's are parsed and rejected, because reference
subjects, enum declarations and class hierarchies are not implemented.

### Statement and expression

A `match` at the start of a statement is the statement form. It needs no
terminator. Its block bodies are statement blocks of the enclosing body: they
may `return`, and inside a loop they may `break` and `continue`. An expression
body is evaluated for its effect and must have one. When no arm accepts,
nothing happens.

Anywhere else a `match` is an expression:

- every arm yields a value, and all arms have one type;
- an arm made only of untyped numeric literals takes its type from the place
  that receives the match, and without one from the first arm that has a
  type;
- some arm must accept whatever the subjects are. That is established by an
  arm without a guard whose patterns are all `_` or type patterns, or, when
  every subject is a `bool`, by arms without guards that accept every
  combination of `true` and `false` between them. No other set of literals
  counts as complete.

In both forms an arm that can never be selected is an error: an earlier arm
without a guard accepts everything it accepts.

## If as an expression

```vxs
int larger = if (first > second) { first } else { second };
```

An `if` in operand position is an expression. Both blocks are required, the
`else` branch is a block and not another `if`, and each block ends with an
expression that has no semicolon; that expression is the value of the block.
Only the selected block runs. The two blocks have one type. An `if` at the
start of a statement is the `if` statement, as before.

## Blocks used as values

The blocks of an `if` expression and the block bodies of the arms of a match
expression are blocks used as values. Statements before the final expression
run in order, and names declared in the block end with it. `return`, `break`
and `continue` cannot leave such a block; a loop inside the block may still be
left with `break`.

## Guard

```vxs
guard (count > 0) else {
    return 0;
}
```

The block runs when the condition is false. It must not complete normally:
its last statement is `return`, `break` or `continue`, an `if` whose two
branches both end that way, or a nested block that does. The statements after
the guard therefore run only when the condition held.

## Nested blocks

A `{` at the start of a statement opens a nested block. Its statements run in
order, and the names it declares are in scope only inside it, so two blocks
side by side may declare the same name. A nested block may not redeclare a
name that is in scope around it.

## Limits of the implemented subset

- Subjects and results of `match`, and results of `if` expressions, are
  `bool` or numeric. Other types need storage rules for the result slot that
  the backend does not have yet. A value of a template type parameter is
  rejected for the same reason.
- A binding in the condition of an `if`, a `guard` or a `while`, such as
  `if (auto user = Find())`, is recognized and rejected: it requires optional
  values.
- `match` and `guard` are reserved words.
- A match may have any number of arms: the lowering groups them, so its
  nesting does not grow with the number of arms. Statements nested by the
  programmer, such as an `if` inside the first branch of an `if`, are still
  limited by the stack of the native stages after Core; see the known
  limitations in the changelog.

## Where it is implemented and tested

| Concern | Location |
| --- | --- |
| grammar | `Visual.XSharp.Parser.Match` and the statement and primary-expression dispatch of `Visual.XSharp.Parser` |
| scoping of pattern bindings and nested blocks | `Visual.XSharp.Resolver.Renamer` |
| typing rules | `Visual.XSharp.TypeChecker.Branching` |
| lowering to Core | `Visual.XSharp.Desugarer.Branching` |
| grammar, typing, lowered shapes, evaluation | `BranchingTests.hs` |
| match against the `if` chain it stands for | `BranchingOracleTests.hs` |
| diagnostic positions, damaged input, templates | `BranchingDiagnosticTests.hs` |
| native execution in both pipeline modes | `Compiler/Fuzzing/BranchingExecutionCases.cpp` |
| generated programs with an independent host model | shape 13 of `Compiler/Fuzzing/SourceFuzz.cpp` |
