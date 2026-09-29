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
| `source_llvm_fuzzer` | Source through Core/CorePrep, Xpp/Xmm verification and LLVM lowering; generated arithmetic differential oracle | First-party C++ pipeline and JIT bridge |

The source oracle independently evaluates bounded generated arithmetic, compiles
it with Xpp/Xmm optimizations both disabled and enabled, executes both verified
artifacts through ORC, and compares all three results. This detects miscompiles
within that generated subset; it is not an oracle for arbitrary Visual X#
programs. Invalid source is a normal rejection, whereas internal failures and
verified-model inconsistencies fail the campaign.

GHC frontend code and prebuilt LLVM dependencies do not receive Clang native
coverage instrumentation. Lexer/parser execution must not be presented as
coverage-guided exploration of their Haskell implementation. Haskell tests and
HPC coverage remain complementary gates, not substitutes for a frontend-specific
feedback-guided engine.

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

ASan intentionally retains freed allocations in quarantine. Fuzz-only settings
bound this cache to 64 MiB, with a 256 KiB thread-local cache; the nonzero
use-after-free detection window remains active. These settings do not disable
the RSS limit or make an OOM successful. Ordinary sanitizer suites keep their
normal quarantine settings.

## Corpus synchronization and reports

Wire seeds come from production writers, so format-version changes do not leave
handwritten supposedly valid documents behind. Lexer, parser and source seeds
come from `Compiler/Fuzzing/Corpus/`. Set `VXS_FUZZ_CORPUS` to retain mutation
corpora across local runs. Updated versioned seeds are added without overwriting
older discovered inputs; conflicting contents under a stable hash fail closed.

GitHub Actions restores a per-platform corpus cache and saves a unique cache
version for each run. It also uploads campaign artifacts on success or failure.
Reports contain the target, duration, RSS limit, selected sanitizer, native
coverage ownership and result. Logs include execution counts and peak RSS.
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
coverage. `source_fuzz_smoke` checks valid-source lowering and the differential
oracle before mutation campaigns begin.

Compiler Tier 1/2/3 run complete native suites with ASan/UBSan, plus separate
TSan runs on supported macOS/Linux hosts. Windows does not have a supported
Clang TSan runtime; it is not represented by a fabricated passing TSan job.
Stable aggregate required checks reject failed, cancelled or skipped host jobs.
The scheduled long campaign supplements rather than replaces the bounded PR
gate. Workflow declarations alone do not enforce merging: repository branch
protection must require these actual checks.

## Remaining coverage boundaries

CLI argument generation, project/lockfile inputs, persistent REPL sessions and
broader generated language programs need independent oracles. Deep semantic
cases, ownership concurrency and frontend feedback-guided coverage are not
established by the four existing targets. Expand these deliberately instead of
equating a green workflow with completion of the entire security program.
