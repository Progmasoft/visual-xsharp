# Core interprocedural effect analysis

This document defines the optimizer contract implemented by the Haskell Core pipeline. It is a compiler engineering reference, not a language specification and not a user-selectable emission format.

## Safety objective

The optimizer may erase an expression only after proving that evaluating it cannot allocate, fail, invoke unknown code, or diverge. The proof is intentionally conservative. A missed optimization is acceptable; deleting observable evaluation is not.

Effects form the following retention-oriented order:

- `PureEffect`: evaluation may be discarded when its result is dead.
- `FailureEffect`: evaluation may report a language/runtime failure.
- `AllocationEffect`: evaluation creates runtime-managed state.
- `CallEffect`: evaluation invokes code whose complete behavior is unknown.
- `DivergenceEffect`: evaluation reaches a recursive strongly connected component and may not terminate.

Direct calls use a module-local symbol catalogue. Indirect calls remain unknown. Recursive strongly connected components are classified as divergent until a future termination proof exists. Closure construction is allocation; the closure body is analyzed only in its execution region.

## Pipeline placement

Each fixed-point optimizer iteration performs constant propagation, recomputes effects, simplifies control flow, recomputes effects again, and performs liveness-based elimination. Recalculation prevents a removed call edge from leaving stale summaries behind. The final result carries deterministic typed reports for diagnostics and tests.

## Arithmetic failures

Integer divide, floor-divide, and remainder operations are discardable only when the divisor is a known nonzero integer literal. A variable or zero divisor is classified as possible failure. Floating-point division follows target floating semantics and remains pure when its operands are pure.

## Conformance matrix

The cases below are reviewable obligations. Each one states the local construct, observation context, conservative classification, transformation rule, and regression oracle.
### E001 — literal in discarded expression

- Input: a verified Core literal appears as a discarded expression.
- Required summary: `PureEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E002 — literal in dead binding initializer

- Input: a verified Core literal appears as a dead binding initializer.
- Required summary: `PureEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E003 — literal in return value

- Input: a verified Core literal appears as a return value.
- Required summary: `PureEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E004 — literal in branch condition

- Input: a verified Core literal appears as a branch condition.
- Required summary: `PureEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E005 — literal in closure body

- Input: a verified Core literal appears as a closure body.
- Required summary: `PureEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E006 — literal in capture value

- Input: a verified Core literal appears as a capture value.
- Required summary: `PureEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E007 — literal in argument evaluation

- Input: a verified Core literal appears as a argument evaluation.
- Required summary: `PureEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E008 — literal in transitive callee

- Input: a verified Core literal appears as a transitive callee.
- Required summary: `PureEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E009 — literal in recursive component

- Input: a verified Core literal appears as a recursive component.
- Required summary: `PureEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E010 — literal in unreachable suffix

- Input: a verified Core literal appears as a unreachable suffix.
- Required summary: `PureEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E011 — literal in identical branches

- Input: a verified Core literal appears as a identical branches.
- Required summary: `PureEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E012 — literal in empty branch

- Input: a verified Core literal appears as a empty branch.
- Required summary: `PureEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E013 — local read in discarded expression

- Input: a verified Core local read appears as a discarded expression.
- Required summary: `PureEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E014 — local read in dead binding initializer

- Input: a verified Core local read appears as a dead binding initializer.
- Required summary: `PureEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E015 — local read in return value

- Input: a verified Core local read appears as a return value.
- Required summary: `PureEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E016 — local read in branch condition

- Input: a verified Core local read appears as a branch condition.
- Required summary: `PureEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E017 — local read in closure body

- Input: a verified Core local read appears as a closure body.
- Required summary: `PureEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E018 — local read in capture value

- Input: a verified Core local read appears as a capture value.
- Required summary: `PureEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E019 — local read in argument evaluation

- Input: a verified Core local read appears as a argument evaluation.
- Required summary: `PureEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E020 — local read in transitive callee

- Input: a verified Core local read appears as a transitive callee.
- Required summary: `PureEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E021 — local read in recursive component

- Input: a verified Core local read appears as a recursive component.
- Required summary: `PureEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E022 — local read in unreachable suffix

- Input: a verified Core local read appears as a unreachable suffix.
- Required summary: `PureEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E023 — local read in identical branches

- Input: a verified Core local read appears as a identical branches.
- Required summary: `PureEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E024 — local read in empty branch

- Input: a verified Core local read appears as a empty branch.
- Required summary: `PureEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E025 — pure primitive in discarded expression

- Input: a verified Core pure primitive appears as a discarded expression.
- Required summary: `PureEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E026 — pure primitive in dead binding initializer

- Input: a verified Core pure primitive appears as a dead binding initializer.
- Required summary: `PureEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E027 — pure primitive in return value

- Input: a verified Core pure primitive appears as a return value.
- Required summary: `PureEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E028 — pure primitive in branch condition

- Input: a verified Core pure primitive appears as a branch condition.
- Required summary: `PureEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E029 — pure primitive in closure body

- Input: a verified Core pure primitive appears as a closure body.
- Required summary: `PureEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E030 — pure primitive in capture value

- Input: a verified Core pure primitive appears as a capture value.
- Required summary: `PureEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E031 — pure primitive in argument evaluation

- Input: a verified Core pure primitive appears as a argument evaluation.
- Required summary: `PureEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E032 — pure primitive in transitive callee

- Input: a verified Core pure primitive appears as a transitive callee.
- Required summary: `PureEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E033 — pure primitive in recursive component

- Input: a verified Core pure primitive appears as a recursive component.
- Required summary: `PureEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E034 — pure primitive in unreachable suffix

- Input: a verified Core pure primitive appears as a unreachable suffix.
- Required summary: `PureEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E035 — pure primitive in identical branches

- Input: a verified Core pure primitive appears as a identical branches.
- Required summary: `PureEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E036 — pure primitive in empty branch

- Input: a verified Core pure primitive appears as a empty branch.
- Required summary: `PureEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E037 — checked integer division in discarded expression

- Input: a verified Core checked integer division appears as a discarded expression.
- Required summary: `FailureEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E038 — checked integer division in dead binding initializer

- Input: a verified Core checked integer division appears as a dead binding initializer.
- Required summary: `FailureEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E039 — checked integer division in return value

- Input: a verified Core checked integer division appears as a return value.
- Required summary: `FailureEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E040 — checked integer division in branch condition

- Input: a verified Core checked integer division appears as a branch condition.
- Required summary: `FailureEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E041 — checked integer division in closure body

- Input: a verified Core checked integer division appears as a closure body.
- Required summary: `FailureEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E042 — checked integer division in capture value

- Input: a verified Core checked integer division appears as a capture value.
- Required summary: `FailureEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E043 — checked integer division in argument evaluation

- Input: a verified Core checked integer division appears as a argument evaluation.
- Required summary: `FailureEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E044 — checked integer division in transitive callee

- Input: a verified Core checked integer division appears as a transitive callee.
- Required summary: `FailureEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E045 — checked integer division in recursive component

- Input: a verified Core checked integer division appears as a recursive component.
- Required summary: `FailureEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E046 — checked integer division in unreachable suffix

- Input: a verified Core checked integer division appears as a unreachable suffix.
- Required summary: `FailureEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E047 — checked integer division in identical branches

- Input: a verified Core checked integer division appears as a identical branches.
- Required summary: `FailureEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E048 — checked integer division in empty branch

- Input: a verified Core checked integer division appears as a empty branch.
- Required summary: `FailureEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E049 — floating-point division in discarded expression

- Input: a verified Core floating-point division appears as a discarded expression.
- Required summary: `PureEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E050 — floating-point division in dead binding initializer

- Input: a verified Core floating-point division appears as a dead binding initializer.
- Required summary: `PureEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E051 — floating-point division in return value

- Input: a verified Core floating-point division appears as a return value.
- Required summary: `PureEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E052 — floating-point division in branch condition

- Input: a verified Core floating-point division appears as a branch condition.
- Required summary: `PureEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E053 — floating-point division in closure body

- Input: a verified Core floating-point division appears as a closure body.
- Required summary: `PureEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E054 — floating-point division in capture value

- Input: a verified Core floating-point division appears as a capture value.
- Required summary: `PureEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E055 — floating-point division in argument evaluation

- Input: a verified Core floating-point division appears as a argument evaluation.
- Required summary: `PureEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E056 — floating-point division in transitive callee

- Input: a verified Core floating-point division appears as a transitive callee.
- Required summary: `PureEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E057 — floating-point division in recursive component

- Input: a verified Core floating-point division appears as a recursive component.
- Required summary: `PureEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E058 — floating-point division in unreachable suffix

- Input: a verified Core floating-point division appears as a unreachable suffix.
- Required summary: `PureEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E059 — floating-point division in identical branches

- Input: a verified Core floating-point division appears as a identical branches.
- Required summary: `PureEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E060 — floating-point division in empty branch

- Input: a verified Core floating-point division appears as a empty branch.
- Required summary: `PureEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E061 — direct call in discarded expression

- Input: a verified Core direct call appears as a discarded expression.
- Required summary: `callee summary`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E062 — direct call in dead binding initializer

- Input: a verified Core direct call appears as a dead binding initializer.
- Required summary: `callee summary`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E063 — direct call in return value

- Input: a verified Core direct call appears as a return value.
- Required summary: `callee summary`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E064 — direct call in branch condition

- Input: a verified Core direct call appears as a branch condition.
- Required summary: `callee summary`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E065 — direct call in closure body

- Input: a verified Core direct call appears as a closure body.
- Required summary: `callee summary`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E066 — direct call in capture value

- Input: a verified Core direct call appears as a capture value.
- Required summary: `callee summary`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E067 — direct call in argument evaluation

- Input: a verified Core direct call appears as a argument evaluation.
- Required summary: `callee summary`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E068 — direct call in transitive callee

- Input: a verified Core direct call appears as a transitive callee.
- Required summary: `callee summary`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E069 — direct call in recursive component

- Input: a verified Core direct call appears as a recursive component.
- Required summary: `callee summary`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E070 — direct call in unreachable suffix

- Input: a verified Core direct call appears as a unreachable suffix.
- Required summary: `callee summary`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E071 — direct call in identical branches

- Input: a verified Core direct call appears as a identical branches.
- Required summary: `callee summary`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E072 — direct call in empty branch

- Input: a verified Core direct call appears as a empty branch.
- Required summary: `callee summary`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E073 — indirect call in discarded expression

- Input: a verified Core indirect call appears as a discarded expression.
- Required summary: `CallEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E074 — indirect call in dead binding initializer

- Input: a verified Core indirect call appears as a dead binding initializer.
- Required summary: `CallEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E075 — indirect call in return value

- Input: a verified Core indirect call appears as a return value.
- Required summary: `CallEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E076 — indirect call in branch condition

- Input: a verified Core indirect call appears as a branch condition.
- Required summary: `CallEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E077 — indirect call in closure body

- Input: a verified Core indirect call appears as a closure body.
- Required summary: `CallEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E078 — indirect call in capture value

- Input: a verified Core indirect call appears as a capture value.
- Required summary: `CallEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E079 — indirect call in argument evaluation

- Input: a verified Core indirect call appears as a argument evaluation.
- Required summary: `CallEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E080 — indirect call in transitive callee

- Input: a verified Core indirect call appears as a transitive callee.
- Required summary: `CallEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E081 — indirect call in recursive component

- Input: a verified Core indirect call appears as a recursive component.
- Required summary: `CallEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E082 — indirect call in unreachable suffix

- Input: a verified Core indirect call appears as a unreachable suffix.
- Required summary: `CallEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E083 — indirect call in identical branches

- Input: a verified Core indirect call appears as a identical branches.
- Required summary: `CallEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E084 — indirect call in empty branch

- Input: a verified Core indirect call appears as a empty branch.
- Required summary: `CallEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E085 — closure construction in discarded expression

- Input: a verified Core closure construction appears as a discarded expression.
- Required summary: `AllocationEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E086 — closure construction in dead binding initializer

- Input: a verified Core closure construction appears as a dead binding initializer.
- Required summary: `AllocationEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E087 — closure construction in return value

- Input: a verified Core closure construction appears as a return value.
- Required summary: `AllocationEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E088 — closure construction in branch condition

- Input: a verified Core closure construction appears as a branch condition.
- Required summary: `AllocationEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E089 — closure construction in closure body

- Input: a verified Core closure construction appears as a closure body.
- Required summary: `AllocationEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E090 — closure construction in capture value

- Input: a verified Core closure construction appears as a capture value.
- Required summary: `AllocationEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E091 — closure construction in argument evaluation

- Input: a verified Core closure construction appears as a argument evaluation.
- Required summary: `AllocationEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E092 — closure construction in transitive callee

- Input: a verified Core closure construction appears as a transitive callee.
- Required summary: `AllocationEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E093 — closure construction in recursive component

- Input: a verified Core closure construction appears as a recursive component.
- Required summary: `AllocationEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E094 — closure construction in unreachable suffix

- Input: a verified Core closure construction appears as a unreachable suffix.
- Required summary: `AllocationEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E095 — closure construction in identical branches

- Input: a verified Core closure construction appears as a identical branches.
- Required summary: `AllocationEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E096 — closure construction in empty branch

- Input: a verified Core closure construction appears as a empty branch.
- Required summary: `AllocationEffect`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E097 — capture initializer in discarded expression

- Input: a verified Core capture initializer appears as a discarded expression.
- Required summary: `nested summary`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E098 — capture initializer in dead binding initializer

- Input: a verified Core capture initializer appears as a dead binding initializer.
- Required summary: `nested summary`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E099 — capture initializer in return value

- Input: a verified Core capture initializer appears as a return value.
- Required summary: `nested summary`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E100 — capture initializer in branch condition

- Input: a verified Core capture initializer appears as a branch condition.
- Required summary: `nested summary`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E101 — capture initializer in closure body

- Input: a verified Core capture initializer appears as a closure body.
- Required summary: `nested summary`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E102 — capture initializer in capture value

- Input: a verified Core capture initializer appears as a capture value.
- Required summary: `nested summary`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E103 — capture initializer in argument evaluation

- Input: a verified Core capture initializer appears as a argument evaluation.
- Required summary: `nested summary`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E104 — capture initializer in transitive callee

- Input: a verified Core capture initializer appears as a transitive callee.
- Required summary: `nested summary`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E105 — capture initializer in recursive component

- Input: a verified Core capture initializer appears as a recursive component.
- Required summary: `nested summary`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E106 — capture initializer in unreachable suffix

- Input: a verified Core capture initializer appears as a unreachable suffix.
- Required summary: `nested summary`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E107 — capture initializer in identical branches

- Input: a verified Core capture initializer appears as a identical branches.
- Required summary: `nested summary`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E108 — capture initializer in empty branch

- Input: a verified Core capture initializer appears as a empty branch.
- Required summary: `nested summary`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E109 — mutable assignment in discarded expression

- Input: a verified Core mutable assignment appears as a discarded expression.
- Required summary: `nested expression summary`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E110 — mutable assignment in dead binding initializer

- Input: a verified Core mutable assignment appears as a dead binding initializer.
- Required summary: `nested expression summary`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E111 — mutable assignment in return value

- Input: a verified Core mutable assignment appears as a return value.
- Required summary: `nested expression summary`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E112 — mutable assignment in branch condition

- Input: a verified Core mutable assignment appears as a branch condition.
- Required summary: `nested expression summary`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E113 — mutable assignment in closure body

- Input: a verified Core mutable assignment appears as a closure body.
- Required summary: `nested expression summary`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E114 — mutable assignment in capture value

- Input: a verified Core mutable assignment appears as a capture value.
- Required summary: `nested expression summary`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E115 — mutable assignment in argument evaluation

- Input: a verified Core mutable assignment appears as a argument evaluation.
- Required summary: `nested expression summary`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E116 — mutable assignment in transitive callee

- Input: a verified Core mutable assignment appears as a transitive callee.
- Required summary: `nested expression summary`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E117 — mutable assignment in recursive component

- Input: a verified Core mutable assignment appears as a recursive component.
- Required summary: `nested expression summary`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E118 — mutable assignment in unreachable suffix

- Input: a verified Core mutable assignment appears as a unreachable suffix.
- Required summary: `nested expression summary`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E119 — mutable assignment in identical branches

- Input: a verified Core mutable assignment appears as a identical branches.
- Required summary: `nested expression summary`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

### E120 — mutable assignment in empty branch

- Input: a verified Core mutable assignment appears as a empty branch.
- Required summary: `nested expression summary`, combined with every eagerly evaluated child.
- Rewrite rule: erase it only when the combined summary is exactly `PureEffect` and the produced value is dead.
- Preservation rule: retain source order for every non-pure child and never speculate across an unknown call.
- Regression oracle: optimized Core verifies, a second optimization is identical, and the typed effect report is deterministic.

## Determinism

Reports follow module function order. Direct callee identifiers are sorted and deduplicated. Fixed-point convergence depends only on Core structure and symbol identifiers, never map insertion order or display names.

## Future extensions

Inlining, escape analysis, allocation sinking, termination proofs, and ownership-sensitive effects may refine `PureEffect`. They must not weaken the conservative boundary without verifier-backed tests. CorePrep remains an internal adapter and is not exposed as a user emission target.

