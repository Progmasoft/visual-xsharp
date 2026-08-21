# Visual Linter

Visual Linter provides the `vlint` command. Its semantic diagnostics come directly from the canonical Visual X# compiler
frontend. The first independent checks cover trailing whitespace, mixed line endings, and a missing final newline.

```text
vlint Program.vxs
vlint -Fix Program.vxs
vlint -List-Checks
vlint -Help
```

`-Fix` applies only safe physical-source fixes and never rewrites compiler or semantic diagnostics. `-List-Checks` prints
the stable rule identifiers currently implemented by the binary.

The Kotlin module under `sources/main/kotlin` owns the typed `Visual.Linter.kts` configuration surface. It exposes the
complete rule catalog as typed scope properties and produces immutable snapshots without discovering or evaluating
scripts.
