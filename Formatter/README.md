<!-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com> -->
<!-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1 -->

# Visual Formatter

Visual Formatter provides the `vfmt` command and validates every input with the canonical Visual X# lexer and parser before
changing it. It normalizes line endings, trailing horizontal whitespace, the final newline, and brace-based block
indentation. The compiler's lossless source-fragment model distinguishes code from comments, character literals, normal
strings, and raw strings, so structural characters inside protected text never affect indentation.

```text
vfmt Program.vxs
vfmt -In-Place Program.vxs Library.vxs
vfmt -Dry-Run Program.vxs Library.vxs
vfmt -Help
```

Standard mode writes one formatted source to standard output. `-In-Place` overwrites every requested file, while
`-Dry-Run` produces no output and returns a failing exit status when any requested source would change. Standard mode
accepts one source because concatenating independent programs on standard output would be ambiguous; the other modes
accept one or more sources and evaluate project configuration only once.

The default engine uses four spaces per block level. Its public Haskell options also support tabs, an independent tab width,
explicit LF or CRLF output, disabling reindentation, and preserving a missing final newline. Lines crossed by a multi-line
raw string or long comment retain their payload indentation and trailing whitespace exactly; only their physical line
ending changes when an explicit output ending is requested.

The Kotlin module under `sources/main/kotlin` owns the typed `Visual.Formatter.kts` configuration surface, canonical
defaults, validation, immutable snapshots, and real scripting evaluator. `vfmt` invokes the installed `vfmt-config` helper
once, then applies the selected input encoding, output encoding, and BOM policy to every requested source. Encoding is not
a formatter CLI option: both direct `vfmt` use and project-wide `vxs format` obey `Visual.Formatter.kts`. UTF-16 and UTF-32
output uses deterministic little-endian payloads, with BOM emission controlled independently.
