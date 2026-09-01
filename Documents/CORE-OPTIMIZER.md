<!-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com> -->
<!-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0 -->

# Core optimization

## Purpose

The Core optimizer is the target-independent simplification boundary between
desugaring and CorePrep. It improves the semantic Core tree while names still
carry stable `SymbolId` identities and types still use the frontend type model.
It does not construct a control-flow graph, select a target instruction, infer
an ABI, or perform LLVM work.

The pipeline is designed around three rules:

1. accept only Core that passes the Core verifier;
2. preserve observable evaluation while simplifying values and control flow;
3. verify the final Core again before returning it to the compiler driver.

These checks are part of the optimizer API rather than assumptions made by its
callers. A tool loading a `.core` artifact receives the same safety boundary as
the normal source compiler.

## Position in the compiler

```text
Typed AST
   |
   v
Desugarer
   |
   v
Core verifier ---- rejects malformed symbols, types, calls, and returns
   |
   v
Core optimizer ---- constant propagation, branch cleanup, liveness
   |
   v
Core verifier ---- rejects an invalid optimizer result
   |
   v
CorePrep ---------- atomization and explicit basic-block control flow
```

CorePrep is intentionally after optimization. A constant branch removed in
Core does not need blocks, temporaries, or jumps in CorePrep. Conversely, the
Core optimizer does not imitate CorePrep by introducing basic blocks early.

## Public entry points

The default compiler uses `defaultCoreOptimizer`. Embedding code that needs a
report can call `optimizeCoreWith defaultOptimizerOptions` and receive an
`OptimizationResult`.

The result contains:

- the verified optimized module;
- structural metrics before optimization;
- structural metrics after optimization;
- the number of fixed-point iterations;
- whether the pipeline converged within the configured limit; and
- a typed report for every enabled pass invocation.

The existing `CoreOptimizer` facade remains small. Its function returns only
the optimized module, which keeps ordinary compiler orchestration independent
of reporting details.

## Pass order

One iteration runs enabled passes in this order:

1. constant propagation and expression folding;
2. control-flow simplification; and
3. effect-aware dead-code elimination.

The order is deliberate. Constant propagation can make a condition known.
Control-flow simplification can then remove a branch. Liveness can finally
remove bindings that became unused after branch selection.

The complete sequence repeats until an iteration makes no structural change.
The default maximum is twelve iterations. A finite bound protects compiler
availability if a future rewrite accidentally oscillates. Reaching the bound
returns the last verified result and marks the report as not converged; it does
not hide the condition.

## Constant environment

Constant propagation uses a sequential environment keyed by `SymbolId`.
Spelling is never used as identity. This matters when local names shadow one
another or when two source scopes happen to use the same text.

Only immutable bindings whose value is already a literal enter the environment.
This restriction is conservative and intentional:

- duplicating a call could execute user code more than once;
- duplicating a closure could change allocation and ownership behavior;
- duplicating a large expression could grow the program;
- copying an expression across a write could observe different state; and
- a literal has no evaluation effect to duplicate.

A mutable binding never becomes a propagated constant. Assignment removes the
target from the current environment before later statements are simplified.
Branches receive the incoming environment independently, and no branch-local
fact leaks to statements after the branch.

## Integer folding

Integer operations are evaluated with unbounded host `Integer` arithmetic and
materialized only when the result fits the declared Visual X# scalar type.
This separates computation from representation:

- `byte` is signed 8-bit;
- `short` is signed 16-bit;
- `long` is signed 32-bit;
- `int` is signed 64-bit;
- `longint` is signed 128-bit;
- `ubyte`, `ushort`, `ulong`, `uint`, and `ulongint` use matching unsigned
  widths; and
- `char` is an unsigned 32-bit scalar at the Core boundary.

An overflowing result remains an explicit primitive expression. The optimizer
does not wrap, saturate, or invent a diagnostic. The language's later checked
or unchecked arithmetic policy can therefore be implemented without first
undoing an incorrect fold.

The integer operations currently folded are:

| Core primitive | Compile-time rule |
| --- | --- |
| add | exact addition followed by range validation |
| subtract | exact subtraction followed by range validation |
| multiply | exact multiplication followed by range validation |
| divide | truncation toward zero when the divisor is nonzero |
| floor divide | rounding toward negative infinity when nonzero |
| remainder | remainder paired with truncating division |
| negate | exact unary negation followed by range validation |
| comparisons | a boolean literal result |

Division, floor division, and remainder by zero remain explicit. Removing them
would erase the later stage's required failure behavior.

## Floating constants

The shared scalar module validates floating spellings, including exponents,
infinity, and NaN, but the optimizer does not yet fold floating arithmetic.
Host floating arithmetic is not a substitute for a specified target semantic:
NaN payloads, signed zero, rounding modes, and precision must remain stable.

Floating spellings use ASCII digits. Unicode decimal categories are not
accepted as wire-level numeric text even when a host character library labels
them as digits. This matches source token rules and keeps artifact validation
independent of locale.

## Algebraic identities

The optimizer recognizes identities that preserve type and evaluation:

```text
x + 0  -> x
0 + x  -> x
x - 0  -> x
x * 1  -> x
1 * x  -> x
x / 1  -> x
x // 1 -> x
```

`x % 1` becomes zero only when `x` is a variable or literal. Replacing an
arbitrary call or closure expression with zero would discard its effect.

Double arithmetic negation collapses when the nested and outer result types
agree. Double logical negation collapses only for a boolean operand. Visual X#
allows numeric values in boolean context, so `!!numericValue` performs a real
numeric-to-boolean conversion and must not become the original numeric value.

## Boolean folding

Logical primitives fold boolean literals. Numeric literals are also interpreted
using the language condition rule: zero is false and every nonzero value is
true. Comparison primitives produce `bool`, never the operand's numeric type.

These rules are checked by the Core verifier before optimization. The optimizer
therefore never needs to guess whether a string, closure, or unresolved value
is condition-compatible.

## Control-flow simplification

Known boolean and numeric conditions select a branch. Statements after an
unconditional return are removed. If both sides of an `if` always return, code
after that `if` is unreachable and is removed as well.

When two branches are structurally identical, the branch can be replaced by
its shared body. The condition is discarded only if it is pure. An effectful
condition is emitted as `CoreEvaluate` before the shared body.

The same preservation rule applies when both branches are empty:

```text
if effectfulCall() { } else { }
```

becomes:

```text
evaluate effectfulCall()
```

It does not disappear.

## Effect model

The optimizer uses a conservative ordered effect model:

| Effect | Current producers | Discardable when unused |
| --- | --- | --- |
| pure | literals, variables, pure primitives | yes |
| allocation | closure construction | no |
| call | direct or indirect Core application | no |
| write | reserved for explicitly effectful expressions | no |

Primitive expressions inherit the strongest effect of their operands. Calls
are effectful even if every operand is pure. Closure construction is effectful
because it can allocate, establish AARC ownership edges, and later participate
in destruction or cycle handling.

This model intentionally lacks speculative purity inference. A future effect
system can prove more calls discardable, but the baseline optimizer must remain
correct before that metadata exists.

## Backward liveness

Dead-code elimination walks each statement list backward while carrying the set
of symbols needed by later statements.

For a binding:

- if the bound symbol is live, retain the binding and add symbols read by its
  initializer;
- if the symbol is dead and the initializer is pure, remove the binding; or
- if the symbol is dead and the initializer is effectful, replace the binding
  with `CoreEvaluate`.

Assignments follow the same principle. A write whose value is never read can
be removed when its right-hand side is pure. An effectful right-hand side is
preserved as an evaluation.

Returns reset liveness to the symbols read by the returned expression because
statements after a return are unreachable. Each branch is analyzed using the
live set required after the join; the incoming set is the union of the
condition and both branch requirements.

## Closure regions

A closure body is a separate liveness region. Values required after closure
construction in the enclosing function do not make similarly named closure
locals live. Capture initializers, however, execute in the enclosing region and
remain part of the closure expression's effect and symbol set.

Literal capture initializers may propagate into the closure body. Parameters
remove matching capture entries from that body environment, so a parameter
always shadows a captured constant with the same `SymbolId`.

Calls and nested closure allocations in a dead closure binding are retained.
The outer binding becomes `CoreEvaluate`; the closure body itself is still
optimized independently.

## Shared scalar facts

The verifier and optimizer import one canonical scalar-facts module. It owns:

- Core scalar spelling extraction;
- integer, floating, and numeric type catalogs;
- signed and unsigned integer range validation; and
- stable floating literal spelling validation.

This prevents a dangerous split where the optimizer creates a literal that the
verifier rejects, or the verifier accepts a type the optimizer accidentally
treats with a different width.

The scalar module is public within the Haskell package because codecs, artifact
tools, and focused tests also need the same contract. It is not a language-level
standard-library API.

## Metrics

Metrics count structural nodes, not estimated runtime cost:

- functions;
- statements;
- expressions;
- bindings;
- assignments;
- branches;
- calls; and
- closures.

Every enabled pass report stores metrics immediately before and after that pass.
Adjacent reports therefore form a continuous chain, including across fixed-point
iterations. Disabled passes produce no report entry.

Metrics are suitable for tests, diagnostics, and regression dashboards. They
are not a promise that fewer nodes always means faster code. A later cost model
can add target-independent estimates without changing existing counters.

## Reporting contract

Pass identity is represented by `OptimizationPass`, not by parsing a display
string. A `PassReport` includes:

- the one-based iteration number;
- the typed pass identity;
- metrics before the pass;
- metrics after the pass; and
- whether the Core module changed structurally.

Reports include stable compiler data only. They do not include timestamps,
machine paths, pointer values, or randomized identifiers, so equal inputs and
options produce equal reports.

## Options

`OptimizerOptions` currently controls:

- maximum fixed-point iterations;
- constant propagation;
- control-flow simplification; and
- dead-code elimination.

All three passes are enabled by default. Options exist for compiler testing,
diagnostics, and controlled development. They are not currently public Visual
X# project DSL keys or CLI flags.

Setting the maximum below one still permits one iteration. This guarantees that
the optimizer has a deterministic result and report without introducing a
special zero-pass meaning. To preserve a module exactly, disable all passes.

## Determinism

The optimizer is deterministic:

- functions retain source order;
- statements retain order unless a proven rewrite removes them;
- maps are queried by stable `SymbolId`;
- liveness sets affect membership, not emitted ordering;
- no rewrite uses hash iteration to construct output; and
- reports contain no environment-dependent values.

Running the default optimizer on its own converged output produces the same
Core module. Idempotence is covered by focused tests.

## Failure behavior

Invalid input returns the Core verifier's diagnostics and no optimization is
attempted. A malformed result returns verifier diagnostics instead of reaching
CorePrep. The optimizer does not catch a diagnostic and continue with a partial
tree.

An iteration limit is different from invalid Core. The result remains verified
and is returned with `optimizationConverged = False`. Compiler policy may later
choose whether that report is informational, diagnostic, or fatal.

## Test strategy

Focused tests cover:

- every scalar family and representative range boundary;
- ASCII-only floating spelling validation;
- arithmetic, comparison, and logical constant folding;
- truncating division versus floor division;
- zero divisors and overflow preservation;
- immutable propagation and mutable invalidation;
- algebraic identities and numeric boolean conversion;
- known, identical, empty, and effectful branches;
- unreachable statement removal;
- pure and effectful dead bindings, evaluations, and assignments;
- closure body regions, literal captures, and parameter shadowing;
- individually disabled passes;
- fixed-point convergence and iteration limits;
- metric and pass-report continuity;
- optimizer idempotence;
- input rejection; and
- final verifier acceptance.

Integration tests additionally compile Visual X# source through optimized Core
and CorePrep. They inspect unoptimized Core when the source-value contract is
the subject of the test, and optimized Core when dead-value removal is the
subject. This distinction prevents optimization progress from weakening lexer
or desugarer coverage.

## Deliberate non-goals

The current optimizer does not perform:

- floating arithmetic folding;
- interprocedural call evaluation;
- call purity inference;
- function inlining;
- common-subexpression elimination;
- loop optimization;
- escape analysis;
- ownership insertion or deletion;
- closure environment layout;
- cross-module symbol resolution;
- CorePrep block optimization;
- Xpp or Xmm optimization; or
- target-specific lowering.

These are separate compiler decisions. Adding one requires an explicit semantic
contract, verifier coverage, and tests at the representation that owns it.

## Extension checklist

Before adding a Core rewrite:

1. state which source-level semantics justify it;
2. identify every evaluation that may be removed, duplicated, or reordered;
3. define behavior for overflow, division failure, NaN, and signed zero where
   relevant;
4. prove that `SymbolId` scope is preserved;
5. preserve closure capture and parameter shadowing rules;
6. add valid, invalid, boundary, and effectful tests;
7. ensure the output passes `verifyCore`;
8. ensure a second optimizer run is structurally identical;
9. document whether the pass changes metrics or reporting; and
10. confirm that CorePrep receives a simpler tree without taking ownership of
    the optimization itself.

Following this checklist keeps Core optimization substantial without turning it
into a second type checker, a premature backend, or an unsafe source rewriter.
