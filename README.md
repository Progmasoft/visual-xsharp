<!--
SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
SPDX-License-Identifier: MPL-2.0
-->

# xs-project

`xs-project` is the LLVM-project-style monorepo root for the Visual X# language and compiler family. The current focus is the
single `xs` tool, which owns both Visual X# compilation and programmable Kotlin project resolution.

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
cmake --preset clangcl-debug
cmake --build --preset clangcl-debug
ctest --preset clangcl-debug --output-on-failure
```

Run these commands from a Visual Studio 2026 developer terminal. JVM-labelled project tests require JRE 25 and the Kotlin
script runner on `PATH`.

Check a source file:

```text
./build/clangcl-debug/vxs check -File tests/fixtures/example_project/source/Main.vxs
```

Validate the test registry of a modern Kotlin project from its project directory:

```text
vxs test
```

Supported top-level `#[Test] fn name()` cases are compiled through the normal native pipeline and executed in isolated
temporary `.vxse` harnesses. `#[Ignore]` and `#[ShouldPanic]` are honored.

## Compiler pipeline

Visual X# uses Haskell through CorePrep, then C++20 for the target-independent native pipeline:

```text
.vxs → Lexer → Parser → Parsed AST → Renamer → Name Resolution
     → Type Checker → Typed AST → Desugarer → Core → CorePrep
     → Xpp → Xmm → LLVM bitcode or VPI
```

Core, Xpp, and Xmm remain target independent. Intermediate representations stay in memory unless requested explicitly.
The Rust semantic implementation remains in the repository while the renewed pipeline is connected incrementally.

## Project and CLI

Each project has exactly one `Visual.XSharp.kts`. The DSL has no module scripts, split state, compatibility setters, or
implicit dependency aliases. `sources.main.entry` identifies the namespace and class containing `public static void Main()`.

```text
vxs check
vxs build
vxs run
vxs test
vxs resolve
vxs build -File Main.vxs
vxs build -Emit xmm
```

See [docs/CLI.md](docs/CLI.md) and [docs/PROJECT_FILES.md](docs/PROJECT_FILES.md) for the current public contract.

## Development rules

- The renewed frontend through CorePrep is Haskell. Xpp and later native stages use C++20.
- Existing C23 layers migrate subsystem by subsystem behind passing tests; the Rust implementation remains available.
- Do not use `#include <stdbool.h>` in new/touched C code; use C23 `bool`.
- Prefer `nullptr` over `NULL` in new/touched C code.
- Use CMake; do not use Meson.
- C++20 files use `.cpp` and `.hpp`; C-only headers use `.h`, and shared C/C++ headers use `.hh`.
- Use `fmt` instead of iostreams. Do not add vcpkg or Conan.
- Initialize Git submodules before configuring. The `vxs` command schema uses the pinned DIMCLI source under
  `third_party/dimcli`; compiler execution remains behind the existing driver ABI.
- The supported build path is Clang, Ninja, LLVM tools, and LLD.
- Do not add persistent shell scripts; use Java source-file tools or D for automation.
- Keep implementation, test, build, configuration, and internal files at or below 1500 lines. Public `Spec/` examples are
  exempt from this implementation limit.

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

## License

For license and notice information, see the root `LICENSE.txt` and `NOTICE.txt` files.
