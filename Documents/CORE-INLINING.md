<!-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com> -->
<!-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1 -->

# Core safe-expression inlining

## Scope

The Core inliner is a target-independent simplification pass between verified
Core and CorePrep. It replaces a deliberately small class of direct calls with
the callee's return expression. The implementation favors a proof that
evaluation is preserved over an optimistic estimate that a rewrite is likely
safe.

This pass is not a general source-level macro expander. It does not copy
statement bodies, move ownership operations, infer visibility, remove function
declarations, or choose a target ABI. It operates on resolved `SymbolId`
identity after type checking and desugaring.

The inliner builds on the optimizer's interprocedural effect analysis. There is
one purity decision, shared by dead-code elimination, branch cleanup, and
inlining. A new call effect must therefore be modeled in effect inference
before it can become an inlining candidate.

## Pipeline position

One fixed-point iteration has the following relevant order:

```text
verified Core
    |
    v
effect inference
    |
    v
safe-expression inlining
    |
    v
constant propagation and folding
    |
    v
effect recomputation
    |
    v
control-flow simplification
    |
    v
effect recomputation
    |
    v
dead-code elimination
```

Inlining precedes constant folding so a call such as a pure `AddOne(41)` can
expose `41 + 1` and become `42` in the same iteration. Effect recomputation
after structural rewrites ensures later passes see the current call graph,
not the graph from the start of the iteration.

The full sequence repeats until Core is unchanged or the configured iteration
limit is reached. This permits a pure call chain to collapse without recursive
rewriting inside one candidate expansion.

## Candidate contract

A Core function is eligible only when all of these conditions hold:

1. its `SymbolId` has a module-local effect report;
2. the solved effect is exactly `PureEffect`;
3. the function is not in a recursive strongly connected component;
4. the complete body is exactly one `CoreReturn` statement;
5. parameter and result types already satisfy the Core verifier.

The one-return requirement is structural. A function with a pure local binding
followed by a return is not directly eligible. Earlier optimizer iterations may
remove the local or propagate its literal, after which a later iteration can
discover the simpler candidate.

The pass does not maintain an eligibility annotation on `CoreFunction`.
Candidates are rediscovered from the current verified module on each iteration.
This avoids stale annotations after propagation, branch removal, or future
Core transformations.

### Why purity is required

Copying a result expression changes where evaluation occurs. If the callee can
allocate, fail, invoke unknown code, or diverge, a naive replacement can alter
observable behavior or ownership. Requiring `PureEffect` makes the candidate
independent of whether its result is later consumed or discarded.

### Why recursion is separate

Recursive functions are currently classified as divergent, but the explicit
recursive check remains part of candidate discovery. It documents the
termination boundary and keeps inlining safe if the effect ordering changes.
A future termination proof may refine selected recursive functions; such a
proof must also define an expansion strategy and a finite budget.

### Why one return expression

Statement-body inlining requires fresh local identity, control-flow splicing,
return-join construction, diagnostic provenance, and ownership transfer. Core
currently has structured statements while CorePrep owns explicit blocks.
Pretending that a statement sequence is an expression would blur that stage
boundary. The current pass therefore handles only an expression already owned
by a return.

## Argument safety

Every call argument must be either:

- a `CoreLiteral`; or
- a `CoreVariable` read.

No other expression is substituted, even when its current effect summary is
pure. This rule handles the three parameter-use cases uniformly.

### Parameter used once

A variable or literal replaces the parameter directly. Type equality has
already been checked by the Core verifier, and the final replacement is checked
again against the call's result type.

### Parameter unused

Inlining removes argument evaluation when the return expression does not refer
to the parameter. Dropping a literal or variable read is safe because neither
has an observable effect. Dropping a primitive tree or call would need a
separate proof that evaluation is irrelevant, so those arguments are rejected.

### Parameter used multiple times

Substitution may duplicate the argument. Duplicating a literal value or stable
variable read is safe at Core. Duplicating a primitive may duplicate failure;
duplicating a call may repeat user code; duplicating a closure may allocate
multiple environments. Those forms remain rejected regardless of present
purity inference.

### Bottom-up rewriting

Children are rewritten before their parent call is considered. A nested call
can become a literal or variable and then satisfy the outer argument rule. This
does not bypass safety: the inner call has already passed the complete candidate
contract and its replacement is the value seen by the outer rewrite.

For example:

```text
Identity(Answer())
```

may become:

```text
Identity(42)
```

and then:

```text
42
```

in one bottom-up traversal when both functions are proven candidates.

## Symbol substitution

The substitution environment is keyed only by `SymbolId`. Source spelling is
retained for diagnostics and display but never selects a parameter. Two names
with identical text and distinct symbols remain distinct.

Substitution recursively visits:

- direct and indirect callee expressions;
- call arguments;
- primitive arguments;
- closure capture initializers; and
- closure bodies.

Closure parameters remove matching entries from the substitution environment
before the closure body is copied. In normal verified Core, symbol allocation
already makes identities unique; the deletion is an additional capture-safety
boundary for hand-built Core and future transformations.

The return expression's type must equal the call expression's result type after
substitution. A mismatch leaves the call unchanged. The verifier should reject
such input before optimization, but retaining the local guard prevents this
pass from manufacturing a type mismatch if candidate construction evolves.

## Node budget

Inlining can increase expression size. The option
`optimizerMaximumInlineExpressionNodes` limits both the candidate expression
and the expanded expression at each call site. Candidate discovery deliberately
retains oversized pure functions in its catalog: doing so lets the report
distinguish “not semantically eligible” from “eligible but over this call site's
configured growth budget.” No oversized expression is copied.

The count includes:

- the expression root;
- each callee expression;
- every call argument;
- every primitive argument;
- each closure capture initializer; and
- every expression contained in a closure statement body.

Statement nodes are counted when they occur inside a closure expression. A
branch contributes its condition and both statement regions. This makes the
budget describe the complete copied payload rather than only its outer
expression constructor.

Values below one normalize to one. A zero or negative configuration therefore
does not acquire a special “disable” meaning; `optimizerInlining = False` is the
explicit way to disable the pass.

Both checks matter:

1. candidate-size checking rejects a call when the function's original return
   expression is already too large;
2. post-substitution checking catches expansion caused by a parameter that
   appears multiple times.

The current default is twenty-four expression nodes. It is a conservative Core
growth guard, not a target cost model. LLVM instruction count, register
pressure, machine code size, and branch prediction do not belong in this pass.

## Effect boundaries

The following functions are never candidates under the current model.

### Allocation

A function returning or constructing a closure has `AllocationEffect`.
Inlining it could duplicate or erase AARC-managed allocation. The closure's
body may be pure; construction itself remains observable runtime work.

### Possible failure

Integer divide, floor-divide, and remainder with an unproved nonzero divisor
have `FailureEffect`. A return expression containing such an operation is not
inlined. Constant propagation may later prove a divisor and allow a future
iteration to reconsider the function.

### Unknown invocation

A function that invokes an indirect callable has `CallEffect`. The compiler
does not infer purity from a function type, name spelling, or the purity of a
particular argument. Callable effect types would require an explicit language
and Core contract.

### Recursive component

Self-recursive and mutually recursive functions receive `DivergenceEffect`.
Removing or expanding their calls could remove nontermination or cause
unbounded compiler expansion. They remain calls.

### Transitive effects

Effect inference solves the module call graph to a fixed point. A syntactically
simple function that calls an allocating, failing, unknown, or recursive
function inherits that effect and is excluded. Candidate discovery never looks
only at the local return constructor.

## Rewriting regions

The pass reaches every Core expression region while preserving statement and
function order.

### Return values

This is the direct candidate use case. The call is replaced by a substituted
return expression and later passes may fold it.

### Binding initializers

A rewritten initializer remains attached to the same binding. Constant
propagation and liveness decide whether the binding becomes a literal fact or
is removed.

### Assignment values

Only the right-hand expression changes. The assignment target and mutability
semantics remain untouched.

### Branch conditions

A pure predicate call can become a boolean or numeric expression. Constant
folding may make it literal, allowing control-flow simplification to select one
branch.

### Evaluation statements

The value is rewritten but the statement remains until liveness examines its
effect. If inlining proves the complete evaluation pure, dead-code elimination
may remove it later in the iteration.

### Closure captures

Capture initializers execute when the closure is constructed. They are visited
in the enclosing expression region and may be inlined independently. Closure
allocation still prevents the outer closure expression from being discarded.

### Closure bodies

The body is a separate runtime region, but module-local direct call summaries
remain valid there. Calls inside it are rewritten without importing enclosing
statement liveness.

## Non-transformations

The inliner intentionally does not:

- delete an unused callee declaration;
- change module or function ordering;
- rename a symbol;
- generate a fresh `SymbolId`;
- inline a function with local statements;
- inline an allocating or failing expression;
- inline an indirect call;
- speculate from function spelling;
- coerce a return type;
- expose a CorePrep block;
- insert AARC retain or release operations;
- perform escape analysis;
- select an LLVM intrinsic; or
- alter public source syntax.

Function reachability will require a visibility/export and multi-module link
unit contract. Statement-body inlining will require a representation-aware
control-flow design. Ownership-sensitive inlining will require explicit Core or
Xpp ownership operations. Those are independent compiler milestones.

## Reporting

Each enabled fixed-point iteration produces an `InlineReport` with:

- the number of discovered candidates;
- the number of rewritten calls;
- the number of calls skipped because at least one argument was unsafe; and
- the number of calls skipped because either the original candidate or the
  substituted result exceeded budget.

The ordinary pass trace also contains `InliningPass` with structural metrics
before and after the pass. These two reports answer different questions:

- `PassReport` shows whether Core changed and how its total structure changed;
- `InlineReport` explains candidate and skip decisions specific to inlining.

Disabled inlining produces neither an `InliningPass` entry nor an
`InlineReport`. Disabling interprocedural effects also disables inlining because
there is no safe candidate proof. This dependency is explicit rather than
falling back to a local syntactic purity guess.

Reports are deterministic. Candidate count follows the current module; rewrite
counts follow source expression traversal; no timestamp, pointer, path, or hash
iteration order enters the data.

## Examples

These examples use Core-like notation and are not Visual X# source grammar.

### Literal return

```text
Answer() = return 42
Entry()  = return Answer()
```

becomes:

```text
Answer() = return 42
Entry()  = return 42
```

The declaration remains because Core does not yet represent export reachability.

### Parameter substitution

```text
Identity(value) = return value
Entry(input)     = return Identity(input)
```

becomes:

```text
Identity(value) = return value
Entry(input)     = return input
```

### Folding after inlining

```text
AddOne(value) = return value + 1
Entry()       = return AddOne(41)
```

becomes `return 41 + 1` during inlining and `return 42` during constant
folding in the same optimizer iteration.

### Unsafe argument

```text
Identity(value) = return value
Entry()         = return Identity(Read())
```

The outer call is retained unless bottom-up rewriting first proves and replaces
`Read()` with a variable or literal. The inliner never duplicates or drops the
unresolved call evaluation.

### Repeated parameter

```text
Twice(value) = return value + value
```

`Twice(input)` may inline because duplicating the variable read is safe.
`Twice(Compute())` does not inline because duplicating `Compute()` could repeat
observable evaluation.

### Unused parameter

```text
Answer(ignored) = return 42
```

`Answer(0)` and `Answer(input)` may inline. `Answer(Compute())` remains a call
because inlining would erase `Compute()`.

### Closure allocation

```text
MakeCallable() = return closure { return 42 }
```

is not a candidate. Even though the closure body is pure, constructing the
callable allocates an AARC object.

### Possible arithmetic failure

```text
Quotient(divisor) = return 42 / divisor
```

is not a candidate because the divisor may be zero. The optimizer preserves
the failure point.

### Recursive call

```text
Again() = return Again()
```

is never expanded. Its effect report records recursive divergence.

## Verification strategy

The focused Core suite covers:

- nullary and parameterized literal substitution;
- multiple parameters and positional mapping;
- repeated and unused safe arguments;
- rejection of primitive, call, and closure arguments;
- allocation, possible failure, and indirect-call rejection;
- self-recursive and mutually recursive components;
- original and expanded node budgets;
- independent option disablement;
- same-iteration constant folding;
- fixed-point pure call chains;
- branch and closure expression regions;
- typed candidate, rewrite, and pass reports;
- stable function order;
- final verifier acceptance; and
- optimizer idempotence.

Every integration result passes the Core verifier after optimization. The
general optimizer suite also checks report continuity across the new pass and
updates its expected pass count to four per enabled iteration.

## Review checklist

Before broadening candidate selection, answer all of these questions:

1. Can any argument evaluation be removed?
2. Can any argument evaluation be duplicated?
3. Can source evaluation order change?
4. Can allocation identity change?
5. Can a failure or divergence disappear?
6. Can a `SymbolId` cross its valid scope?
7. Can closure capture shadowing change?
8. Can the expanded expression exceed the configured budget?
9. Does the effect solver understand every new expression form?
10. Does the output pass `verifyCore`?
11. Is a second optimizer run structurally identical?
12. Do typed reports explain the new decision?

If any answer is unknown, retain the call. A conservative call is valid Core;
an unsound inline expansion is a compiler correctness bug.

## Future work

Potential extensions remain intentionally separate:

- fresh-symbol statement-body inlining;
- call-site frequency and target-independent cost modeling;
- function reachability after a public/export contract exists;
- termination proofs for selected recursive functions;
- effect-polymorphic callable types;
- ownership-aware argument movement;
- escape-informed closure allocation removal;
- diagnostic provenance for copied statement bodies; and
- link-unit-aware cross-module candidate discovery.

None of these should weaken the current safe-expression path. They can add new
proofs and candidate classes while preserving the rule that unknown evaluation
stays explicit.

## Diagnostic and compatibility boundary

Inlining is silent because it changes neither accepted source programs nor
their required observable behavior. Candidate and skip information is exposed
as typed compiler data for tests and development tooling, not as warnings to
ordinary users. A missed candidate is therefore an optimization outcome rather
than a source diagnostic.

The pass adds no CLI option and no `Visual.XSharp.kts` key. Its options are an
internal embedding and test surface until the compiler has a stable public
optimization-profile contract. Artifact compatibility is unchanged: input and
output are the existing verified Core model and the VXCR wire version does not
change.

CorePrep consumes only the final verified tree. It is not told which calls were
inlined and does not reconstruct removed call boundaries. Debug provenance for
future statement-body inlining must be designed explicitly; this expression
pass does not invent source positions that Core does not carry.
