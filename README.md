<!--
SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
SPDX-License-Identifier: MPL-2.0
-->

# xs-project

`xs-project` is the LLVM-project-style monorepo root for the X# language and compiler family. The current focus is the
single `xs` tool, which owns both X# compilation and programmable Kotlin project resolution.

This repository is experimental, but it is treated as serious compiler infrastructure: every step must remain buildable and
testable, the HIR/MIR layers must not depend on LLVM, and the documented compilation flow must be preserved.

## Quick start

Required core tools:

- CMake 3.31 or newer
- Ninja
- Clang / LLVM tools
- LLD
- Rustup and Cargo; `xslang/rust-toolchain.toml` selects the pinned nightly compiler core toolchain
- JRE 25 or newer plus the `kotlin` scripting command
- Optional helper tools such as `fd`, `rg`, `bat -p`, `sd`, and `busybox wc` are useful for development

Default debug build:

```text
cmake --preset clang-debug
cmake --build --preset clang-debug
ctest --preset clang-debug --output-on-failure
```

On machines with prior OOM issues, use a 2GB virtual memory limit for build/test runs:

```text
cmake --build --preset clang-debug --target xs_project_runtime
ulimit -v 2097152
cmake --build --preset clang-debug
ctest --preset clang-debug --output-on-failure -LE jvm -FA kotlin_project_resolver
```

Run the JVM-labelled project tests separately without a virtual-memory limit.

Check a source file:

```text
./build/clang-debug/xs check -file tests/fixtures/example_project/source/Main.xs
```

Validate the test registry of a modern Kotlin project from its project directory:

```text
xs test
```

Supported top-level `#[Test] fn name()` cases are compiled through the normal native pipeline and executed in isolated
temporary `.xse` harnesses. `#[Ignore]` and `#[ShouldPanic]` are honored.

## Monorepo directories

| Path | Status | Purpose |
| --- | --- | --- |
| `include/` | active | Shared public C headers across projects |
| `xs/` | active | X# compiler, CLI, lexer/parser, AST, macro, HIR, MIR, XLIL, LLVM backend infrastructure |
| `xs_kts/` | active | Mandatory Kotlin/JVM 25 project runtime bundled with `xs` and its programmable project DSL |
| `xsrt/` | active | C23 runtime ABI boundary and boxed optional UTF-16 string support |
| `xslang/` | active | Rust semantic compiler core linked into the C23 driver through a versioned bulk AST boundary |
| `xlil-java/` | active | Official Java 25 FFM reader/writer binding for the stable XLIL C ABI |
| `Spec/` | active source documentation | X# syntax and language behavior examples/spec files |
| `docs/` | active documentation | Architecture, build, CLI, backend, roadmap, and implementation status |
| `tests/` | active | C-based unit and integration tests |
| `xsfmt/` | future | Rust nightly + Serde formatter |
| `xstidy/` | future | Rust nightly + Serde linter |
| `xs-analyzer/` | future | Rust language server + TypeScript VS Code extension |

## Compiler pipeline

Documented order:

```text
.xs sources
    → lexing and parsing
    → structural AST
    → macro expansion
    → HIR and dependency graph
    → type checking
    → MIR
    → borrow checker
    → MIR optimization
    → monomorphization
    → codegen units
    → XLIL
    → LLVM IR lowering
    → optimization
    → object code
    → linking
```

This order is not just documentation; it is an implementation boundary. Missing frontend behavior must not be invented in
the backend. HIR and MIR do not depend on the LLVM API; the backend entry language is the target-independent XLIL layer.

## Current working features

- C23-based, Clang/LLVM-focused build system
- Shared public C23 feature headers under `<xs/c23/*.h>`, with `<xs/c23_features.h>` as the umbrella and an initial
  object-safe trait/implementation binding facility
- Clang/LLVM-oriented CMake/toolchain checks
- X# lexer and structural AST parser
- Synthetic reparse and expanded-view infrastructure for declaration/statement macro expansion
- HIR symbol table plus import/name/type resolution bootstrap
- Primitive type metadata and nominal user-defined type resolution
- Literal initializer/assignment/return checks
- MIR model API, text writer, structural verifier, borrow-check skeleton, and a few MIR optimization passes
- XHIR/XMIR structured text writer and header parser bootstrap in Rust `xslang`
- XLIL model, assembly-like text writer/parser/verifier, and limited MIR → XLIL lowering core
- LLVM context/module/target/object/link infrastructure

For the detailed current status, see [docs/IMPLEMENTATION.md](docs/IMPLEMENTATION.md).

## Specification examples

The public language examples live under [Spec/](Spec). Smaller files document syntax and semantic intent for declarations,
types, macros, standard modules, control flow, and project manifests. Larger one-file completed-language sketches live under
[Spec/Programs/](Spec/Programs); they are design fixtures, not promises that the current partial compiler can compile every
example today.

## Release policy

The project is now in the `0.2.3` development period. Supported source functions cross the C23 structural-AST boundary into
Rust HIR (coordinated THIR and XHIR sides), verified and optimized MIR, and XLIL before the public C23 XLIL model drives
LLVM native `.xse` emission.
Rust compiler-core control flow now includes conditional loops, post-test `do`/`while` sugar, loop jumps, and statement
and value-producing `match`; mutable locals and match results cross CFG edges through explicit target-independent MIR
storage operations. Value-producing `if` expressions use the same target-independent merge-storage model in local
initializers and call arguments. `Long` division, remainder, bitwise operations, and shifts now remain in that Rust
HIR (THIR/XHIR), MIR, and XLIL route. Prefix and postfix
increment/decrement preserve their distinct result values through the current native local-storage slice. It does not imply
that the complete X# language is executable yet. Earlier releases are compiler infrastructure snapshots.

Explicit top-level generic calls are monomorphized through a transitive specialization worklist. The current slice checks
interface constraints, supports recursive calls to the same concrete instance, and rejects type-expanding polymorphic
recursion before native lowering.

The integer widths are fixed from `Byte`/`SByte` through `UInteger`/`Integer`. Context-typed literals for every width,
including signed minimum values and full u128 values, now use the same Rust compiler-core route through XHIR, XMIR, XLIL,
and LLVM native emission. The public C23 boundary carries 128-bit values with project-owned two-word types instead of a
compiler extension. Arithmetic, bitwise, shifts, equality, and ordering now preserve every fixed integer width through
native `.xse` emission, including signed/unsigned LLVM selection. Unary `+`/`-` for supported signed literals and logical
`!` for `Bool` use this route.
Same-module `Int` helper functions also remain i64 through this route, including arithmetic, bitwise, signed-shift, and
ordered-comparison operations. Native process entry remains `fn main() -> Long`.
Supported project helpers may now live in separate source files: the Rust compiler core merges their expanded ASTs into a
single program-wide signature, HIR, MIR, XLIL, and LLVM module before native `.xse` linking.
Direct and mutually recursive same-module calls use that program-wide signature registry. Unit-returning helpers, including
explicit `-> ()` functions, lower to result-free MIR/XLIL/LLVM calls; a value-returning call followed by `;` is evaluated
and its typed result is discarded.
Nominal `data` values can now be projected directly from call results, so chained expressions such as
`make_point().position.x` remain typed in XHIR and lower through aggregate extraction in MIR, XLIL, and LLVM without a
source-level temporary.

## CLI summary

Compilation and project resolution share one command:

```text
xs check
xs build
xs run
xs resolve
xs build --output hir -file Main.xs
xs build --output mir -file Main.xs
xs build --output xlil -file Main.xs
xs build --hir -file Main.xs
xs build --mir -file Main.xs
xs build --xlil -file Main.xs
```

The argument-free `xs` project forms use the bundled project runtime to discover and evaluate `xs.project.kts` or the
`xs.settings.kts` + `xs.build.kts` pair, then compile the returned `.xs` source registry. The `xs -file`
forms are direct single-file/intermediate input flows. `xs resolve` refreshes the binary SQLite dependency lock without
compiling sources. Checked `.xs` input can emit real `.xhir`, `.xmir`, or `.xlil`
program text. Supported `.xhir`, `.xmir`, `.xlil`, and source programs can continue through verified XLIL and LLVM to
native `.xse` output.
Intermediate output extensions:

- `.xhir`: human-readable XHIR text, intended for direct semantic inspection and review
- `.xmir`: human-readable MIR text, intended for direct control-flow/borrow inspection and review
- `.xlil`: XLIL text registry

`.xhir`, `.xmir`, and `.xlil` are intentionally text formats, not opaque serialized compiler state. `.xhir` and `.xmir`
must remain human-readable even when their official record grammar becomes stricter. They are not assembly-like: XHIR is a
structured semantic tree/record dump, and XMIR is a structured control-flow/analysis dump. `.xlil` will never be a binary
format. Current XLIL text is assembly-like and starts with `.xlil version 0`, then uses directive/label/value records such
as `.xlil module`, `.extern`, `.func`, `bb0.entry:`, `%r0:i64 = const.i64 42`, `br bb1`, `ret %r0`, and `.end`.

## Development rules

- The primary implementation language is C23.
- Do not use `#include <stdbool.h>` in new/touched C code; use C23 `bool`.
- Prefer `nullptr` over `NULL` in new/touched C code.
- Use CMake; do not use Meson.
- The supported build path is Clang, Ninja, LLVM tools, and LLD.
- Do not add persistent shell scripts; use Java source-file tools or D for automation.
- Keep files under 1000 lines; prefer smaller modules when a component starts to sprawl.
- Use `busybox wc` or another compact line-counting tool when checking file length.
- Use `ulimit -v 2097152` during test/build runs to reduce OOM risk.

For broader contribution and workflow rules, see [docs/CONTRIBUTING.md](docs/CONTRIBUTING.md)

## Documentation map

- [docs/README.md](docs/README.md): documentation entry point
- [docs/BUILDING.md](docs/BUILDING.md): build, test, toolchain, and OOM notes
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md): compiler architecture and stage boundaries
- [docs/CLI.md](docs/CLI.md): CLI contract and current status
- [docs/PACKAGES.md](docs/PACKAGES.md): package registry commands and availability
- [docs/IMPLEMENTATION.md](docs/IMPLEMENTATION.md): detailed implementation status
- [docs/SPEC.md](docs/SPEC.md): guide to the `Spec/` language examples
- [docs/TODO.md](docs/TODO.md): public roadmap
- [docs/MONOREPO.md](docs/MONOREPO.md): monorepo selection model
- [docs/LLVM_BACKEND.md](docs/LLVM_BACKEND.md): LLVM backend infrastructure
- [docs/BACKENDS.md](docs/BACKENDS.md): implemented and planned backend architecture
- [docs/XLIL.md](docs/XLIL.md): XLIL text registry and public API direction

## License

For license and notice information, see the root `LICENSE.txt` and `NOTICE.txt` files.
