<!--
SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
SPDX-License-Identifier: MPL-2.0
-->

# Visual X# project files

A Visual X# project has one `Visual.XSharp.kts` file. The CLI discovers it from the working directory or its parents; there
is no `-Project` option. Split settings/build scripts and separate module scripts are not project formats.

The project runtime requires Java 25 and evaluates the Kotlin DSL before invoking the native compiler. Source files use the
`.vxs` extension. The configured `sources.main.entry` identifies the program entry point; source discovery does not infer an
entry point from a special filename.

## Main sections

- `project` declares identity, version, and stability.
- `compiler` selects the compiler/standard, backend, build mode, emission kind, diagnostics, and LLVM settings.
- `outdirs` selects release and debug output directories.
- `targets` declares supported target triples.
- `authors`, `dependencies`, `workspaces`, and `pml` describe publication and dependency metadata.
- `sources.main` declares the source directory and required entry point; `sources.test` declares tests.
- `sources.viget` controls ViGet publication exclusions.

Conditional configuration continues to use `cfg(...)`.

## Outputs

The compiler's default output is a `.vxse` native executable. Explicit intermediate emission uses `.core`, `.xpp`, or `.xmm`.
LLVM-facing emission may produce object, assembly, LLVM IR, or LLVM bitcode according to the selected `Emit` value.
Intermediate representations remain in memory unless emission is explicitly requested.

## Lockfile

Successful dependency resolution writes binary SQLite data to `Visual.XSharp.Lockfile.sqlite3`. Normal evaluation does not
write a text dump. The separate `vxdc` tool is responsible for producing a requested diagnostic dump from a project file.

ViGet packages use the `.vipkg` extension and tar+zstd container format.
