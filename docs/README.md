<!--
SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
SPDX-License-Identifier: MPL-2.0
-->

# xs-project documentation

This directory contains the active architecture and implementation documentation for the X# compiler. The `Spec/` directory
is the source documentation area for syntax and language examples; `docs/` explains compiler architecture, the build/test
process, the CLI contract, and implementation status.

## Reading order

Recommended order for newcomers:

1. [../README.md](../README.md): repository overview and quick start
2. [BUILDING.md](BUILDING.md): build/test and toolchain
3. [ARCHITECTURE.md](ARCHITECTURE.md): compiler pipeline and layer boundaries
4. [CLI.md](CLI.md): user commands and current status
5. [PROJECT_FILES.md](PROJECT_FILES.md): Kotlin project files and package boundaries
6. [PACKAGES.md](PACKAGES.md): package registry commands and current availability
7. [RUNTIME.md](RUNTIME.md): current runtime ABI boundary
8. [IMPLEMENTATION.md](IMPLEMENTATION.md): stage-by-stage implementation status
9. [SPEC.md](SPEC.md): guide to the `Spec/` language-example tree
10. [XHIR.md](XHIR.md): human-readable XHIR text format direction
11. [XMIR.md](XMIR.md): human-readable MIR text format direction
12. [XLIL.md](XLIL.md): backend input text registry direction
13. [XLIL_JAVA.md](XLIL_JAVA.md): official Java 25 FFM binding
14. [TODO.md](TODO.md): public roadmap
15. [RELEASES.md](RELEASES.md): pre-1.0 release policy
16. [BACKENDS.md](BACKENDS.md): current backend status and future backend contracts
17. [LLVM_BACKEND.md](LLVM_BACKEND.md): LLVM backend infrastructure
18. [MONOREPO.md](MONOREPO.md): monorepo project/runtime selection model

## Documentation authority

- For documented X# syntax, `Spec/` has priority.
- For how to read the example/spec tree, start with [SPEC.md](SPEC.md).
- For project configuration, `PROJECT_FILES.md` and the `xs_kts/` DSL API have priority.
- For XHIR/XMIR text-output behavior, read `docs/XHIR.md` and `docs/XMIR.md`.
- For XLIL behavior, read `docs/XLIL.md` and `xs/include/xs/lil.h` together.
- For implementation order, [IMPLEMENTATION.md](IMPLEMENTATION.md) is authoritative.
- Public remaining work is summarized in [TODO.md](TODO.md).

If you find a conflict, do not silently add new behavior. Update the relevant public documentation and implementation in the
same patch when the behavior is user-visible.

## Update rule

When user-visible code behavior changes, at least one public document should change too:

- CLI changes go in `CLI.md`.
- Build/toolchain changes go in `BUILDING.md`.
- Pipeline or layer-boundary changes go in `ARCHITECTURE.md` and `IMPLEMENTATION.md`.
- Public roadmap changes go in `TODO.md`.
- User-visible changes are summarized in the root `CHANGELOG.md`.
