<!--
SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1
-->

# Compiler benchmarks

Visual X# keeps microbenchmarks next to the implementation they measure and keeps recorded results in this directory.
The first benchmark surface covers the target-independent Core-through-Xmm pipeline, including the boundary shared by
the Haskell frontend and C++20 native pipeline:

- `Compiler/Core/Benches` measures native Core verification, VXCR encoding, VXCR decoding, and CorePrep preparation;
- `Compiler/Core/CorePrep/Benches` measures native CorePrep verification and VXCP encoding/decoding;
- `Compiler/Codegen/Xpp/Benches` measures Xpp verification, optimization, and artifact encoding/decoding;
- `Compiler/Codegen/Xmm/Benches` measures Xpp-to-Xmm lowering plus Xmm verification, optimization, and artifact codecs;
- `Compiler/Haskell/Core/Benches` measures the equivalent Haskell Core and CorePrep operations with Criterion.

Run every benchmark from the repository root:

```powershell
go run scripts/develop.go benchmark
```

The command builds native benchmarks with Bazel's optimized configuration. Criterion uses the package's `-O2` benchmark
stanza. Bazel options may be appended after `--`, but platform configuration remains owned by the developer command.
Individual binaries and Criterion selectors remain available for focused investigations:

```powershell
bazelisk run -c opt //Compiler/Core/Benches:core_benches -- --benchmark_filter=CoreEncode
bazelisk run -c opt //Compiler/Core/CorePrep/Benches:coreprep_benches -- --benchmark_filter=Decode
bazelisk run -c opt //Compiler/Codegen/Xpp/Benches:xpp_benches -- --benchmark_filter=Optimize
bazelisk run -c opt //Compiler/Codegen/Xmm/Benches:xmm_benches -- --benchmark_filter=Lower
Set-Location Compiler
cabal bench visual-xsharp-core:core-benches --enable-benchmarks --benchmark-options="--match Core/Encode"
```

## Recording policy

A committed result is a baseline, not a universal performance promise. Each result must identify the commit, host,
processor, operating system, compiler/runtime versions, build mode, and exact command. Compare results only on the same
machine with similar power and thermal conditions. Close background work, use an optimized build, and retain the raw
per-size observations rather than publishing only the fastest case.

Continuous integration compiles both harnesses and performs short smoke runs. It deliberately does not fail a change on
elapsed-time thresholds: shared runners vary too much for a stable regression gate. A future dedicated runner may add
statistical comparisons once it can provide fixed CPU frequency, warm-up policy, and historical storage.

## Recorded results

- `2026-09-13-Windows-x86_64.md` is the first complete Core-through-Xmm baseline.
- `2026-09-13-Native-Liveness.md` records the Xpp/Xmm optimizer follow-up after native liveness integration.
- `2026-09-14-Native-Dataflow.md` records the verifier/encoder follow-up after worklist scheduling and packed lattices.
