<!--
SPDX-FileCopyrightText: 2026 Leitwolf <xs-lang.chess031@slmails.com>
SPDX-License-Identifier: MPL-2.0
-->

# xs-project monorepo

This repository uses an LLVM-project-style single-checkout monorepo model for X#. The X# equivalents of LLVM’s
`LLVM_ENABLE_PROJECTS` / `LLVM_ENABLE_RUNTIMES` split are `XS_ENABLE_PROJECTS` and `XS_ENABLE_RUNTIMES`.

The checkout/root concept is `xs-project`. The top-level CMake project name is `xs_project`.

## Active projects

Top-level CMake selection:

```text
cmake --preset clang-debug -DXS_ENABLE_PROJECTS=xs
```

The default value is `xs`. The value `all` selects all stable projects.

## Stable projects

- `xs`: the X# compiler, public C23 API headers, unified CLI, and current test targets.
- `xs_kts`: the mandatory Kotlin/JVM 25 project runtime bundled in the `xs` package.
- `xslang`: the Rust semantic compiler core. It is built as an internal static library of `xs`, not selected as a separate
  top-level CMake project.

Shared public C headers live under root `include/`. `xs` C sources live under `xs/sources/`, `xs`-specific headers under
`xs/include/`.

## Runtime selection

Top-level runtime selection:

```text
cmake --preset clang-debug -DXS_ENABLE_RUNTIMES=xsrt
```

`xsrt` is the first buildable runtime project. It currently defines runtime ABI version 0 and the ownership boundary for
boxed `Optional<Str>` values; broader allocation, I/O, panic, and XGC integration remain incremental work.

## Future projects

- `xsfmt`: future Rust nightly + Serde formatter project.
- `xstidy`: future Rust nightly + Serde linter project.
- `xs-analyzer`: future Rust language server + TypeScript VS Code extension project.

Future project directories live at their own roots, but are not included in the CMake build yet. Once implementation starts,
they can become selectable through `XS_ENABLE_PROJECTS`.

## Tool configuration standard

`xsfmt`, `xstidy`, and future developer tool projects use TOML for user configuration. Project build configuration uses
Kotlin scripts.

## CMake contract

- `XS_ENABLE_PROJECTS=xs`: builds the current compiler.
- `XS_ENABLE_PROJECTS=all`: builds all stable projects.
- `XS_ENABLE_RUNTIMES=`: builds no runtime.
- `XS_ENABLE_RUNTIMES=xsrt` or `all`: builds the current runtime.
- Selecting a future project intentionally fails in CMake; this prevents unfinished tools from appearing to build silently.
- Selecting a future runtime intentionally fails in the same way.
