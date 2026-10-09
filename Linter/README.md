<!-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com> -->
<!-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1 -->

# Visual Linter

Visual Linter provides the `vlint` command. Its semantic diagnostics come directly from the canonical Visual X# compiler
frontend. The first independent checks cover trailing whitespace, mixed line endings, and a missing final newline.

```text
vlint Program.vxs
vlint -Fix Program.vxs
vlint -List-Checks
vlint -Help
```

The linter reports what the compiler it is built with reports. Version 0.1.1 is built with the compiler of
Visual X# 0.5.0: a source may use method references (`Type::Method`), names written through their namespace,
conditionals over strings and console output, and their diagnostics arrive under the `compiler` check, for example
`compiler.VXT0080` for a method reference that does not select one overload and `compiler.VXT0076` for a format the
compiler rejects.

`-Fix` applies only safe physical-source fixes and never rewrites compiler or semantic diagnostics. `-List-Checks` prints
the stable rule identifiers currently implemented by the binary.

The Kotlin module under `sources/main/kotlin` owns the typed `Visual.Linter.kts` configuration surface. It exposes the
complete rule catalog as typed scope properties and produces immutable snapshots without discovering or evaluating
scripts.
