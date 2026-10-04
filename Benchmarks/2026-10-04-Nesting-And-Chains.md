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

"Before" is commit `5ffdbdff` for the stack and Criterion measurements and
`main` at `d9bd54e4` for `vxs check` and the smoke program. "After" is the
working tree that this file is committed with.

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

The shapes are `statements`, an `if` nested in an `if`; `expressions`, an
addition whose first operand is an addition; and `chain`, an `else if`
chain. `pipeline` is `Pipeline::ConsumeCore`, the whole native route from
Core bytes to LLVM. KiB per level:

| Shape | Stage | Before | After | After, sanitizers |
| --- | --- | ---: | ---: | ---: |
| statements | wire writer | 0.67 | 0.67 | 2.05 |
| statements | wire reader | 14.8 | 0.65 | 1.59 |
| statements | Core verifier | 1.70 | 0.67 | 1.70 |
| statements | CorePrep adapter | 3.81 | 0.31 | 1.15 |
| statements | pipeline | 14.9 | 0.67 | 1.67 |
| expressions | wire writer | 0.83 | 0.83 | 2.34 |
| expressions | wire reader | 10.6 | 0.42 | 0.67 |
| expressions | Core verifier | 2.34 | 0 | 0 |
| expressions | CorePrep adapter | 4.35 | 0 | 0 |
| expressions | pipeline | 10.6 | 0.42 | 0.67 |
| chain | wire reader | 0.38 | 0.38 | 0.62 |
| chain | CorePrep adapter | 0.25 | 0 | 0 |
| chain | pipeline | 0.38 | 0.38 | not measured |

A zero means the stage completed on the smallest stack at every depth. The
sanitizer build was not measured before the change. What remains per
expression level and per chain link in the reader and the pipeline is the
release of the module, whose implicit destructors recurse.

Totals that follow from the slopes, for the whole pipeline:

| Input | Before | After | After, sanitizers |
| --- | ---: | ---: | ---: |
| statements nested 256 deep | 3.7 MiB | 0.2 MiB | 0.4 MiB |
| expressions nested 1024 deep | 10.7 MiB | 0.4 MiB | 0.6 MiB |
| statements nested 4096 deep, reader only | 59 MiB | 2.6 MiB | 6.4 MiB |
| statements nested 4096 deep, writer only | 2.7 MiB | 2.7 MiB | 8.2 MiB |
| `else if` chain of 4096 links | 1.5 MiB | 1.5 MiB | 2.4 MiB, reader only |

The first two rows are the frontend's nesting limits and the next two the
default depth limit of the native wire codec. The pipeline row for 4096
nested statements is absent: the function body is a level itself, so that
module is one level beyond the wire limit and is rejected.

The largest stack any measured stage needs on input the limits admit is
therefore 8.2 MiB, in a sanitizer build, against the 256 MiB the compiler
thread reserves.

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
