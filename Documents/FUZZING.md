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

On macOS, use `./bazel-bin/Compiler/Fuzzing/wire_fuzz_smoke` for the second command. Both hosts run this target in the
Native workflow. The program prints the seed number, iteration, and hex input before reporting a caught invariant
failure. Preserve that input in a focused regression test at the owning codec; do not make a random seed the only proof of
a fixed bug.

This is a **bounded deterministic mutation smoke test**, not coverage-guided fuzzing. Its 4,096 cases are fast enough for
every PR, but they cannot establish a coverage percentage or prove the absence of decoder vulnerabilities. The harness
also exposes `LLVMFuzzerTestOneInput` in `//Compiler/Fuzzing:wire_libfuzzer_entry` for a future instrumented libFuzzer
driver. Building that library alone does not run libFuzzer or enable coverage instrumentation. Do not report it as a
coverage-guided run until the driver, sanitizer runtime, corpus, duration, and crash artifacts are actually connected.

## Next campaigns

The current corpus starts from small valid documents, so it reaches framing and basic body fields but does not deeply
exercise nested expressions, CFGs, ownership operations, or large numeric literals. Expand with independently generated
function-bearing seeds and known-version golden artifacts before increasing random iteration counts. Keep malformed
wire regression cases in the owning Core, CorePrep, Xpp, or Xmm test package.

The next distinct inputs are lexer/parser source text, project file/lockfile decoding, CLI argument sequences, REPL
session commands, and artifact/LLVM ingestion. They should have separate harnesses and oracles: a source parser crash is
not equivalent to a wire codec mismatch, and neither should be hidden behind one catch-all success counter. Run memory
sanitizers for campaigns that handle pointers or ownership and keep minimized crash inputs as permanent regressions.
