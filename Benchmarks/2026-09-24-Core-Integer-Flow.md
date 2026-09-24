# Core Integer Flow Benchmark

## Scope

This run measures the Core optimizer after adding path-sensitive integer facts
for guarded division, contradictory integer paths, variable comparisons, and
bounded arithmetic. It is a new workload, so these numbers do not claim a
speedup against an older baseline.

The Criterion fixtures are created before measurement. The timed work is the
production `optimizeCoreWith` path on a Core module whose size key is the number
of repeated expression statements. One benchmark executes a safe integer divide
under a nonzero guard; the other places divides on an impossible interval edge.
The latter must be removed because the conjunction `value > 11 && value < 12`
has no integer solution.

## Environment and command

- Date: 2026-09-24
- Host: Windows x86-64
- GHC: 9.10.3
- Optimization: Cabal `-O2`
- Criterion sample limit: 0.1 seconds per case
- Benchmark sizes: 8, 32, 128, 512, and 1024 statements

The two groups were run separately:

```powershell
cabal bench core-benches --enable-benchmarks --benchmark-options="--match prefix Core/GuardedIntegerEffects --time-limit 0.1"
cabal bench core-benches --enable-benchmarks --benchmark-options="--match prefix Core/ContradictoryIntegerPaths --time-limit 0.1"
```

## Measurements

The table uses Criterion's reported estimate range. Timing units are per full
optimizer invocation for the generated module.

| Statements | Guarded integer effects | Contradictory integer paths |
| ---: | ---: | ---: |
| 8 | 92–94 μs | 53–62 μs |
| 32 | 225–245 μs | 101–105 μs |
| 128 | 720–747 μs | 287–301 μs |
| 512 | 2.83–2.98 ms | 0.98–1.09 ms |
| 1024 | 6.24–6.56 ms | 1.96–2.09 ms |

The 32-statement guarded case and the 8- and 512-statement contradictory-path
cases had substantial outliers. Treat those observations as variance, not as
evidence that those sizes intrinsically cost more. This final run includes
left-to-right call/guard fact refinement and short-circuit effect handling;
benchmark fixtures themselves do not contain calls.

## Reading the result

Both workloads grow with the number of statements and remain practical at
1024 repeated sites on this host. The contradictory-path group is cheaper
because its impossible branch is eliminated without retaining repeated
failure-capable expressions. The guarded group carries and queries a nonzero
fact while visiting each live divide.

The size ratio from 8 to 1024 is 128x. Measured latency increased by about 69x
for guarded effects and 36x for contradictory paths. This is encouraging for
the current fixtures but is not a formal complexity bound: module shape,
branching, map width, closure count, inlining, and optimizer fixed-point count
all affect runtime. The optimizer's broader Criterion groups should continue to
be checked for interactions.

No previous benchmark used these exact fixtures. A future comparison should
run both revisions with the same GHC, optimization flags, host, Criterion
settings, and source tree outside the timed region. CI timing remains a
regression signal, not a release-quality latency promise.

## Follow-up

- Keep the generated comparison oracle and source-to-Core tests enabled when
  changing fact-map representation.
- Add deeper-branch and wider-live-environment workloads before adopting a more
  expensive relational domain.
- Revisit the 32-statement guarded noise on repeated runs before diagnosing a
  specific nonlinear threshold.
- Preserve the separate Core verify, wire encode/decode, and CorePrep groups;
  integer-flow cost should not be inferred from artifact or verifier timing.
