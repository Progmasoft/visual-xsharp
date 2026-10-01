<!-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com> -->
<!-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1 -->

# Compiler fuzzing and sanitizer campaigns

Fuzzing complements specification examples, component tests and IR verifiers.
A passing campaign is evidence about its inputs and duration, not a proof of
memory safety or complete language coverage.

## Independent targets and oracles

| Target | Input and checks | Coverage ownership |
| --- | --- | --- |
| `wire_fuzzer` | Core, private CorePrep transport, Xpp and Xmm; bounded decoding, semantic verification and equal encode/decode round trips | First-party C++ codecs and verifiers |
| `lexer_fuzzer` | Arbitrary bytes through the frontend lexer ABI; complete token/diagnostic evaluation | Native ABI bridge, not GHC-generated lexer branches |
| `parser_fuzzer` | Arbitrary bytes through syntax analysis; complete AST/diagnostic evaluation | Native ABI bridge, not GHC-generated parser branches |
| `source_llvm_fuzzer` | Arbitrary source through Core/CorePrep, Xpp/Xmm verification and LLVM lowering | First-party C++ pipeline |
| `differential_fuzzer` | Generated arithmetic and control flow compiled with native optimizers disabled/enabled and compared with an independent evaluator | First-party C++ pipeline and JIT bridge |
| `cli_fuzzer` | NUL-separated arguments through the typed command-line parser; two parses must produce equal typed models, leave `argv` unchanged, attach a diagnostic to every rejection and select a command on every acceptance | First-party C++ CLI parser |
| `project_fuzzer` | Evaluator registry records, both raw and as one mutated field of a valid document; decoding must be deterministic and a source-requiring decode may only accept what the permissive decode accepts | First-party C++ registry decoder; no evaluator process or filesystem access |
| `repl_fuzzer` | Up to eight operations on one persistent session: arithmetic cells checked against an independent accumulator, type queries that must not change history, failed-cell rollback and reset | First-party C++ session and JIT bridge |
| `ownership_fuzzer` | One to four threads racing weak locks, unowned loads and weak copies against the final strong release; the payload must be destroyed exactly once, never resurrected and never read after destruction | AARC runtime |
| `frontend-fuzz` | Lexer, parser and source-to-CorePrep stages mutated in-process with GHC HPC tick feedback | GHC-compiled frontend modules; no native sanitizer |

The differential oracle independently evaluates one generated program per
input: a bounded arithmetic expression, a classic `for` loop with `continue`
and `break`, a `do`/`while` loop, an `if`/`else` over that expression, a
`while` loop preceded by its own initializers, or a recursion that terminates
only because `||` and `&&` skip their right operands. The expected value is computed
by ordinary host code in the harness, never by a second compiler path. It
compiles the source once, lowers the same verified Core with Xpp/Xmm
optimizations both disabled and enabled, executes both verified
artifacts through ORC, and compares all three results. Agreement between the
two optimizer settings is not accepted by itself: both consume the same
Core-to-CorePrep adapter, so a defect there makes them agree on the same wrong
or non-terminating program. This detects miscompiles within that generated
subset; it is not an oracle for arbitrary Visual X#
programs. Invalid source is a normal rejection, whereas internal failures and
verified-model inconsistencies fail the campaign.

Arbitrary source and generated arithmetic use separate corpora and equal
per-target time budgets. This lets source mutations reach native lowering
without repeatedly creating two ORC sessions for unrelated generated code.
The differential generator consumes at most 33 bytes: 31 expression selectors,
one shape byte and one trip-count byte. Its 64-byte input limit keeps
mutations near the bytes that influence the program.

GHC frontend code and prebuilt LLVM dependencies do not receive Clang native
coverage instrumentation. Running `lexer_fuzzer` or `parser_fuzzer` must not be
presented as coverage-guided exploration of the Haskell implementation.

`frontend-fuzz` is the separate feedback engine for that code. It is a Cabal
executable built with `--enable-coverage` in its own `dist-fuzz-coverage`
build directory, so it never shares package state with the ordinary frontend
build. It resets the HPC counters before each input, keeps an input when it
reaches a tick of a `Visual.XSharp.*` module that no earlier input reached,
and refuses to run when the production modules carry no ticks. Mutations are
deterministic for a recorded seed, inputs are limited to 8192 bytes and five
seconds, and the runtime heap is limited to 512 MiB. A Haskell exception or a
timeout writes the exact input as `failure.seed` and fails the campaign;
`frontend-fuzz STAGE --replay FILE` replays it. This engine has no sanitizer
and reports tick coverage, which is not comparable with libFuzzer edge
coverage.

## Run a campaign

From the repository root:

```powershell
go run ./helpers/cmd/develop fuzz
go run ./helpers/cmd/develop fuzz-stress
```

Both commands use combined ASan/UBSan on owned native code and verify that the
runtime can start a clean process and diagnose intentional use-after-free and
signed-overflow violations. Merely linking a sanitizer is insufficient.
Dependencies retain their own compile flags. `fuzz` defaults to 30 seconds per
target; `fuzz-stress` defaults to 900. Set `VXS_FUZZ_SECONDS` to an integer from
1 through 3600 to override either duration. CI uses 90 seconds per target for
bounded campaigns and 900 for scheduled stress campaigns.

Each campaign has a 30-second per-input timeout and a finite input length:

| Target | Maximum input bytes | RSS limit (MiB) |
| --- | ---: | ---: |
| Wire | 16384 | 768 |
| Lexer | 65536 | 1024 |
| Parser | 65536 | 1536 |
| Source/LLVM | 65536 | 4096 |
| Differential | 64 | 4096 |
| CLI | 16384 | 768 |
| Project registry | 65536 | 768 |
| REPL | 64 | 4096 |
| Ownership | 64 | 768 |

ASan intentionally retains freed allocations in quarantine. Fuzz-only settings
bound this cache to 64 MiB, with a 256 KiB thread-local cache; the nonzero
use-after-free detection window remains active. These settings do not disable
the RSS limit or make an OOM successful. Ordinary sanitizer suites keep their
normal quarantine settings.

## Corpus synchronization and reports

Wire seeds come from production writers, so format-version changes do not leave
handwritten supposedly valid documents behind. Every other target has
versioned seeds under `Compiler/Fuzzing/Corpus/<target>/`; a target without
that directory fails the campaign instead of starting from nothing. Set `VXS_FUZZ_CORPUS` to retain mutation
corpora across local runs. Updated versioned seeds are added without overwriting
older discovered inputs; conflicting contents under a stable hash fail closed.

GitHub Actions restores a per-platform corpus cache and saves a unique cache
version for each run. It also uploads campaign artifacts on success or failure.
Reports contain the target, duration, RSS limit, selected sanitizer, native
coverage ownership and result. Structured reports also include executed inputs,
average executions per second, new corpus entries, slowest input time and peak
RSS from libFuzzer's final counters. A successful process without a complete
final report or with zero executed inputs fails the campaign gate. Failed
processes retain their original logs even if final counters are unavailable.
Failed work directories are preserved for diagnosis; successful CI reports
are retained for artifact upload.

Keep the exact failing input, compiler version, command and report. Replay a
single input with the same instrumented executable, for example:

```powershell
.\bazel-bin\Compiler\Fuzzing\wire_fuzzer.exe -runs=1000 PATH_TO_FAILURE
```

Use the matching Clang runtime environment, as the developer helper does. A
fixed-input replay is not a new fuzz campaign. Minimize a reproducible defect,
then add a focused regression at the owning component before changing the
implementation. A campaign-level RSS failure may require investigating the
whole corpus and sanitizer cache rather than just the final input.

## Deterministic smoke and merge gates

`wire_fuzz_smoke` exercises 1024 deterministic mutations of each of four valid
documents. It runs independently of libFuzzer and does not claim guided
coverage. `source_fuzz_smoke` checks valid-source lowering and then runs the
differential oracle on every generated program shape at trip counts 0 through
11 before mutation campaigns begin. Set `VXS_FUZZ_TRACE=1` to print each
generated source with its reference and optimized LLVM IR.

Neither smoke program nor the HPC engine has libFuzzer's per-input timeout, and
a miscompiled generated loop does not return. The developer helper therefore
bounds every smoke, libFuzzer and HPC process as a whole: 90 seconds for a
smoke program, the campaign duration plus 90 seconds for a libFuzzer target and
plus 300 seconds for the HPC engine. On expiry it terminates the complete
process tree and reports a watchdog failure. Build tools are not bounded by
this watchdog. Do not run a smoke binary directly without an external time
limit when investigating a suspected hang.

## ThreadSanitizer campaign

AddressSanitizer and ThreadSanitizer cannot share one executable.
`go run ./helpers/cmd/develop fuzz-thread` builds only the targets that start
their own threads, currently `ownership_fuzzer`, with libFuzzer and
ThreadSanitizer, proves that the runtime reports an intentional data race, and
then runs a bounded campaign with the same corpus, limits and report checks.
The command fails on Windows, where Clang has no ThreadSanitizer runtime; it
never reports a skipped run as success. The fuzzing workflow runs it on macOS
and native Ubuntu. The Fedora container job does not run it for the
shadow-memory reason given below.

Compiler Tier 1/2/3 run complete native suites with ASan/UBSan. Tier 1 macOS
and Tier 2 native Ubuntu additionally run separate TSan suites. Fedora Tier 3
runs inside a GitHub Actions container; the current TSan runtime cannot reserve
its shadow memory under that host/container ASLR layout, so Fedora does not
claim TSan coverage. The native Linux TSan gate remains required in Tier 2.
Windows does not have a supported Clang TSan runtime; it is not represented by
a fabricated passing TSan job.
Stable aggregate required checks reject failed, cancelled or skipped host jobs.
The scheduled long campaign supplements rather than replaces the bounded PR
gate. Workflow declarations alone do not enforce merging: repository branch
protection must require these actual checks.

## Remaining coverage boundaries

The harnesses above are evidence about the inputs they ran, not proofs:

- the CLI, project and REPL oracles check determinism, rollback and a small
  arithmetic model. They do not model every option interaction, lockfile
  refresh, project evaluation or REPL declaration form;
- the differential generator covers integer arithmetic, three loop forms,
  one conditional and one guarded recursion. Closures, other scalar types,
  ownership and templates have no generated-program oracle here; their
  executable checks live in the component test suites;
- `ownership_fuzzer` varies thread count and iteration count, not arbitrary
  interleavings, and its ThreadSanitizer campaign does not run on Windows or in
  the Fedora container;
- HPC feedback is expression-tick coverage of the frontend, without
  memory-safety instrumentation, and its mutator is byte- and token-level, not
  grammar-aware;
- prebuilt LLVM and the GHC runtime are not instrumented by any campaign.

Expand these deliberately instead of equating a green workflow with completion
of the entire security program.
