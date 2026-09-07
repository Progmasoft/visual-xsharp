<!-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com> -->
<!-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1 -->

# Visual Formatter

Visual Formatter provides the `vfmt` command and validates every input with the canonical Visual X# lexer and parser before
changing it. It normalizes line endings, trailing horizontal whitespace, the final newline, and brace-based block
indentation. The compiler's lossless source-fragment model distinguishes code from comments, character literals, normal
strings, and raw strings, so structural characters inside protected text never affect indentation.

```text
vfmt Program.vxs
vfmt -In-Place Program.vxs
vfmt -Dry-Run Program.vxs
vfmt -Help
```

Standard mode writes formatted source to standard output. `-In-Place` overwrites the file, while `-Dry-Run` produces no
output and returns a failing exit status when formatting would change the file.

The default engine uses four spaces per block level. Its public Haskell options also support tabs, an independent tab width,
explicit LF or CRLF output, disabling reindentation, and preserving a missing final newline. Lines crossed by a multi-line
raw string or long comment retain their payload indentation and trailing whitespace exactly; only their physical line
ending changes when an explicit output ending is requested.

The Kotlin module under `sources/main/kotlin` owns the typed `Visual.Formatter.kts` configuration surface, canonical
defaults, validation, and immutable snapshots. Script discovery and evaluation are intentionally outside this module.
