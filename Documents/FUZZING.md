<!-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com> -->
<!-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1 -->

# Fuzzing the compiler

Fuzzing complements specification examples and component-owned tests. The first connected campaign targets four
untrusted binary input boundaries: Core (`.core`), private CorePrep transport, Xpp (`.xpp`), and Xmm (`.xmm`). These
decoders must reject malformed input without crashing, excessive allocation, or silently changing a successfully decoded
model. A structural decode is not proof that a module passes its semantic verifier.

## What runs in CI

`//Compiler/Fuzzing:wire_fuzz_smoke` generates one valid, version-current seed for each format, then runs 1,024
deterministic mutations per seed. Mutations include bit flips, byte replacement, truncation, insertion, deletion, and
all-one count/scalar bytes. Each case has explicit input, depth, count, and text limits. A successful decode is encoded
and decoded again when the encoder accepts that model; the resulting model must equal the first one. This catches
reader/writer asymmetry as well as crashes. An encoder's semantic rejection is not treated as a decoder failure.

From the repository root:

```powershell
bazelisk build //Compiler/Fuzzing:wire_fuzz_smoke
.\bazel-bin\Compiler\Fuzzing\wire_fuzz_smoke.exe
```

On macOS and Linux, use `./bazel-bin/Compiler/Fuzzing/wire_fuzz_smoke` for the second command. All three compiler tiers
run this target. The program prints the seed number, iteration, and hex input before reporting a caught invariant
failure. Preserve that input in a focused regression test at the owning codec; do not make a random seed the only proof of
a fixed bug.

This smoke is **not** coverage-guided; its 4,096 cases are fast enough for every PR, but cannot establish a coverage
percentage or prove the absence of decoder vulnerabilities.

## Coverage-guided campaign

The same four-stage `LLVMFuzzerTestOneInput` harness is also linked into a real libFuzzer executable. The host-specific
Bazel profile instruments transitive project code with SanitizerCoverage and links libFuzzer's `main`. A fresh temporary
corpus starts with four valid serialized documents, one per stage. During the 30-second CI run, libFuzzer retains inputs
that discover new coverage; comparison value profiling helps it cross binary fields. This follows the LLVM
[libFuzzer corpus and instrumentation model](https://llvm.org/docs/LibFuzzer.html).

```powershell
go run scripts/develop.go fuzz
```

`develop.go` selects the Windows ClangCL, macOS Clang, or Linux Clang profile, builds the corpus generator and instrumented binary,
runs the campaign, and removes **only its own generated temporary directory** after success. On a crash, timeout, or
invariant exception, it preserves that directory and prints its path; GitHub Actions uploads it as a failure artifact.
Keep a minimized failure in the owning component's regression suite before fixing the decoder. Compiler Tier 1 runs
this coverage-guided step on Windows, macOS Sequoia, and macOS Tahoe in addition to the deterministic smoke.

The 30-second duration is a continuous regression signal, not a security proof or complete coverage claim. For a longer
local campaign, export a fresh corpus with `wire_fuzz_smoke -Write-Corpus EMPTY_DIRECTORY`, build
`//Compiler/Fuzzing:wire_fuzzer` with the relevant private Bazel fuzz profile, then invoke the binary with that corpus
and a larger `-max_total_time`. Never report a bare instrumented build as a completed fuzz campaign.

## Next campaigns

The current seed corpus starts from small valid documents, so it reaches framing and basic body fields but does not deeply
exercise nested expressions, CFGs, ownership operations, or large numeric literals. Expand with independently generated
function-bearing seeds and known-version golden artifacts before increasing random iteration counts. Keep malformed
wire regression cases in the owning Core, CorePrep, Xpp, or Xmm test package.

The next distinct inputs are lexer/parser source text, project file/lockfile decoding, CLI argument sequences, REPL
session commands, and artifact/LLVM ingestion. They should have separate harnesses and oracles: a source parser crash is
not equivalent to a wire codec mismatch, and neither should be hidden behind one catch-all success counter. Run memory
sanitizers for campaigns that handle pointers or ownership and keep minimized crash inputs as permanent regressions.
