<!-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com> -->
<!-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0 -->

# Repository layout

## Buildable components

```text
Compiler/     Native vxs driver, public headers, modular C++20 implementation, Haskell packages, and build support
xslang/       Retiring Rust reference implementation; not a production build dependency
ProjectSystem/ Kotlin project evaluator, VXDC, and Visual.XSharp.kts DSL
Spec/         Public language-design example suites
tests/        Native compiler and integration tests
include/      Shared non-compiler ABI headers
third_party/  Pinned source dependencies
```

Compiler-owned public headers live physically under `Compiler/Headers/Visual/XSharp/`. Bazel removes the physical
`Compiler/Headers` prefix from consumers, so the stable include spelling remains `Visual/XSharp/...`.

The ecosystem tools have independent ownership and build systems:

```text
Analyzer/         Haskell LSP and Kotlin configuration DSL
Formatter/        Haskell vfmt executable and Kotlin configuration DSL
Linter/           Haskell vlint executable and Kotlin configuration DSL
```

These canonical projects replace the retired `xs-analyzer`, `xsfmt`, and `xstidy` prototypes. Their Haskell components use
Cabal, their Kotlin configuration layers use Gradle with an external JRE 25 and
Kotlin Script Runner, and the Visual Analyzer editor integrations additionally use their platform-specific build systems.

Their first compiler-connected layer is intentionally narrow: Visual Analyzer exposes syntax, semantic, and full frontend
analysis without a standalone executable; `vfmt` performs parser-gated physical source normalization; and `vlint` combines
semantic compiler diagnostics with independently actionable source-hygiene checks. Kotlin configuration and editor hosts
remain separate ownership layers and are not represented by placeholder implementations.

The analyzer, formatter, and linter Kotlin modules currently provide typed scopes, defaults, validation, and immutable
snapshots. Their future script evaluators remain a separate integration layer.

Visual Formatter and Visual Linter use independent release lines rather than inheriting the compiler version. Analyzer,
Formatter, and Linter also have separate CI workflows so each project can evolve without coupling its validation gates to
the others.

## Build ownership

Bazel owns top-level native orchestration. Language-specific CI jobs call Cabal and Gradle directly; the repository carries
neither CMake configuration nor a second wrapper such as `just`.

## Nested repositories

The website and IDE are maintained as separate repositories beneath the local working directory. They are not part of the
root Git index or root Bazel graph.

## Generated directories

Build products, Cargo targets, Cabal `dist-newstyle`, Gradle output, website distribution files, and local service state are
generated or local-only data and are not source ownership boundaries.
