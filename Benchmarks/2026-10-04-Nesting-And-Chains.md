<!--
SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1
-->

# Nesting depth, long chains and nested loops

## Scope

This record holds three sets of measurements taken before and after one
change set:

- the stack each native Core stage needs per level of nesting, in an ordinary
  build and in an AddressSanitizer and UndefinedBehaviorSanitizer build;
- the time of the Haskell Core operations that grew faster than their input:
  the loop fixed point of the integer analysis on nested loops, and wire
  encoding, CorePrep lowering and CorePrep verification on `else if` chains
  and on sequences of `if` statements;
- the time of `vxs check` on whole programs of those shapes, and of the
  sanitized `source_fuzz_smoke` program.

"Before" is commit `5ffdbdff` for the Criterion measurements and `main` at
`d9bd54e4` for `vxs check` and the smoke program. "After" is commit
`35d9c1ab`. The stack tables and the operator chain table name their
revisions themselves; "now" is the working tree this file is committed with.

## Environment

- Date: 2026-10-04
- OS: Windows NT 10.0.26200, x86-64
- CPU: Intel Core i5-3210M @ 2.50 GHz
- GHC 9.10.3, Cabal `-O2` for Criterion; clang-cl 22.1.8 for the native code
- Native build: the default configuration of `develop build`; sanitizer
  build: `develop sanitize address-undefined`

## Stack per level of nesting

`//Compiler/Support/Tests:stack_probe` builds a Core module of a given shape
and depth and runs one stage on a thread whose stack has a given size; the
process is ended by the operating system when the stack is too small. The
smallest size that completes was found by bisection, to within 0.5 percent or
4 KiB, at depths 64, 256, 1024 and 4096, and the table gives the slope
between the two largest depths measured. Windows rounds a stack to 64 KiB, so
small totals are coarse and the slopes are the meaningful figures.

```powershell
go -C helpers run ./cmd/develop build -- //Compiler/Support/Tests:stack_probe
bazel-bin/Compiler/Support/Tests/stack_probe.exe statements 1024 decode 708
```

The shapes are `statements`, an `if` nested in an `if`; `operands`, an
addition whose second operand is an addition; `expressions`, an addition
whose first operand is an addition; and `chain`, an `else if` chain. The
first two are nesting that every stage recurses along. The last two are
chains, which are as deep in the tree as they are long. `pipeline` is
`Pipeline::ConsumeCore`, the whole native route from Core bytes to LLVM.

Three revisions were measured: `5ffdbdff`, before any of this work;
`35d9c1ab`, after the frames of the reader, the verifier and the adapter were
reduced; and the working tree this file is committed with, in which chains
are walked in a loop by every stage, modules are released from a list, and
the reader reads every expression into its place. KiB of stack per level:

| Shape | Stage | `5ffdbdff` | `35d9c1ab` | Now | Now, sanitizers |
| --- | --- | ---: | ---: | ---: | ---: |
| statements | wire writer | 0.67 | 0.67 | 0.67 | 2.05 |
| statements | wire reader | 14.8 | 0.65 | 0.65 | 1.59 |
| statements | Core verifier | 1.70 | 0.67 | 0.67 | 1.70 |
| statements | CorePrep adapter | 3.81 | 0.31 | 0.31 | 1.17 |
| statements | pipeline | 14.9 | 0.67 | 0.67 | 1.67 |
| operands | wire writer | | 0.19 | 0.19 | 0.62 |
| operands | wire reader | | 7.13 | 0.37 | 1.06 |
| operands | Core verifier | | 0.91 | 0.91 | 2.52 |
| operands | CorePrep adapter | | 1.94 | 1.94 | 3.39 |
| operands | pipeline | | 7.13 | 1.94 | 3.39 |
| expressions | wire writer | 0.83 | 0.83 | 0 | not measured |
| expressions | wire reader | 10.6 | 0.42 | 0 | not measured |
| expressions | Core verifier | 2.34 | 0 | 0 | not measured |
| expressions | CorePrep adapter | 4.35 | 0 | 0 | not measured |
| expressions | pipeline | 10.6 | 0.42 | 0 | not measured |
| chain | wire reader | 0.38 | 0.38 | 0 | not measured |
| chain | CorePrep adapter | 0.25 | 0 | 0 | not measured |
| chain | pipeline | 0.38 | 0.38 | 0 | not measured |

A zero means the stage completed on the smallest stack at every depth
measured: up to 1024 additions and 4096 links, and, for the working tree,
20000 of each on 512 KiB in every stage. An empty cell was not measured: the
`operands` shape was added to the probe after `5ffdbdff`, and until then the
cost of nesting in a second operand had been taken, wrongly, to be that of
the `expressions` shape. The sanitizer build is AddressSanitizer with
UndefinedBehaviorSanitizer.

Totals that follow from the slopes, for the whole native pipeline:

| Input | `5ffdbdff` | Now | Now, sanitizers |
| --- | ---: | ---: | ---: |
| statements nested 256 deep, the frontend limit | 3.7 MiB | 0.2 MiB | 0.4 MiB |
| operands nested 1024 deep, the frontend limit | | 1.9 MiB | 3.4 MiB |
| statements nested 4096 deep, the wire limit, writer | 2.7 MiB | 2.7 MiB | 8.2 MiB |
| operands nested 4000 deep, near the wire limit | | 7.6 MiB | 13.3 MiB |
| sum of 20000 operands | crash | under 0.1 MiB | not measured |
| `else if` chain of 20000 links | crash | under 0.1 MiB | not measured |

The largest stack any measured stage needs on input the limits admit is
therefore 13.3 MiB, in a sanitizer build at the depth limit of the wire
codec, and 3.4 MiB at the limits of the frontend, against the 256 MiB the
compiler thread reserves. The adapter is now the stage that costs most per
level of real nesting. Copying a module still recurses once per level; the
pipeline copies only the bodies of closures.

## Haskell Core operations

```powershell
Set-Location Compiler
cabal bench visual-xsharp-core:core-benches --enable-benchmarks --benchmark-options="--match pattern NestedLoop Chain Sequence --time-limit 0.1"
```

Criterion estimates, one run of each revision. The sample limit is short and
the ranges are wide, so only the orders of magnitude and the growth per
doubling are meaningful.

`Core/NestedLoopIntegerFacts`: the optimizer on loops nested to the given
depth.

| Depth | Before | After |
| ---: | ---: | ---: |
| 2 | 251 us | 244 us |
| 4 | 876 us | 326 us |
| 8 | 14.2 ms | 650 us |
| 12 | 276 ms | 659 us |
| 16 | 4.89 s | 907 us |

Before, four more levels multiplied the time by about 18. The analysis
repeated the fixed point of an inner loop on every pass over the loop around
it. It now iterates only loops that hold at most one further level of loops
and treats what a deeper loop assigns as unknown, which is sound.

`else if` chains and sequences of `if` statements of the given length:

| Benchmark | Size | Before | After |
| --- | ---: | ---: | ---: |
| `Core/EncodeChain` | 256 | 78.9 ms | 1.72 ms |
| | 512 | 289 ms | 5.01 ms |
| | 1024 | 968 ms | 14.9 ms |
| | 2048 | 4.95 s | 60.0 ms |
| `CorePrep/PrepareChain` | 256 | 23.8 ms | 1.94 ms |
| | 512 | 101 ms | 4.22 ms |
| | 1024 | 460 ms | 9.15 ms |
| | 2048 | 2.20 s | 20.2 ms |
| `CorePrep/PrepareSequence` | 256 | 3.60 ms | 3.21 ms |
| | 512 | 7.18 ms | 6.16 ms |
| | 1024 | 31.0 ms | 14.9 ms |
| | 2048 | 46.7 ms | 32.4 ms |
| `CorePrep/VerifyChain` | 256 | 11.2 ms | 1.36 ms |
| | 512 | 46.7 ms | 3.35 ms |
| | 1024 | 160 ms | 6.34 ms |
| | 2048 | 746 ms | 15.1 ms |
| `CorePrep/VerifySequence` | 256 | 16.4 ms | 1.85 ms |
| | 512 | 58.5 ms | 4.21 ms |
| | 1024 | 209 ms | 7.95 ms |
| | 2048 | 788 ms | 17.8 ms |

Lowering and verification now double with the input. Encoding a chain still
grows by a factor of three to four per doubling, from a base about fifty
times lower than before; the cause was not investigated.

## Whole programs

`vxs check -File` on one method of the given shape, one run each, wall time
in seconds. `crash` is a stack overflow without a diagnostic.

| Program | Size | Before | After |
| --- | ---: | ---: | ---: |
| `else if` chain | 500 | 1.40 | 0.64 |
| | 1000 | 2.52 | 1.28 |
| | 2000 | crash | 3.30 |
| | 4000 | crash | 10.4 |
| sequence of `if` statements | 500 | 0.76 | 0.70 |
| | 1000 | 1.51 | 1.32 |
| | 2000 | 3.58 | 2.79 |
| | 4000 | 12.0 | 7.11 |
| nested `for` loops | 8 | 0.19 | 0.17 |
| | 14 | 1.06 | 0.13 |
| | 50 | more than 120 | 0.28 |
| | 255 | more than 120 | 3.72 |
| `match` arms | 500 | 0.74 | 0.65 |
| | 1000 | 1.65 | 1.36 |
| | 2000 | 3.95 | 3.41 |

Chains of operators, which the expression nesting limit rejected above 1024
operands until this change, on `35d9c1ab` with the limit lifted and on the
working tree:

| Program | Size | Limit lifted only | Now |
| --- | ---: | ---: | ---: |
| sum of operands | 5000 | 2.45 | 1.23 |
| | 20000 | 30.4 | 4.57 |
| | 50000 | 198 | 9.37 |
| comparisons joined by `&&` | 100 | 3.68 | not measured |
| | 200 | 47.6 | 1.13 |
| | 800 | more than 120 | 1.75 |
| | 3000 | not measured | 5.60 |

The `&&` figures of the first column are the same on `main`: 3.66 seconds
for 100 comparisons. Constant propagation asked the integer facts about
every node of an expression, and the CorePrep lowering collected symbol
identities by appending lists; both are fixed, and the facts are consulted
only for conditions of at most 256 nodes.

The time still grows faster than the program between 2000 and 4000
statements: by 3.1 for the chain and by 2.5 for the sequence. Earlier
per-stage timing placed that growth in the frontend before Core and in the
native stages after CorePrep; it is not addressed here.

## Sanitized smoke program

`source_fuzz_smoke` built with AddressSanitizer and
UndefinedBehaviorSanitizer, run by hand on the same machine:

| Revision | Wall time | Programs compiled for the execution tables |
| --- | ---: | --- |
| `main` at `d9bd54e4` | 291 s | one for every run |
| one program for every distinct body | 204 s | 134 for 237 runs |
| after: up to eight small bodies in a program | 160 s | 237 runs in fewer programs |

In the last measurement the expression table takes 23 seconds, the branching
table with its four large programs 36 seconds, and the 342 generated
differential programs, which this change does not touch, 98 seconds.

`main` exceeds the 240-second watchdog of `develop sanitize` on this machine.
No run was removed, and the large programs are run on more arguments than on
`main`: every run of a body is now a call in the one program compiled for
that body, which returns a bit for each run whose value differs from the expected
one and must return zero in both pipeline modes. Changing one expected value
by hand makes the smoke program fail and name the run.
