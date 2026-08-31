<!--
SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
-->

# xslang

`xslang` contains retained Rust compiler experiments and reference algorithms from an earlier architecture. It is not linked
into `vxs`, is not part of the Bazel production graph, and is not a supported alternative frontend, middle end, or backend.

The production compiler is:

```text
Haskell Lexer through CorePrep
-> C++20 Xpp and Xmm
-> C++20 LLVM backend and LLD driver
```

No new Visual X# language behavior should be implemented in this crate. Useful algorithms or tests may be adapted into the
Haskell or C++20 stage that owns the behavior, with that stage's types, verifier, diagnostics, and tests. Retained Rust APIs
and internal formats are legacy implementation details rather than the current public intermediate-artifact catalog.

This tree may be reduced in reviewed slices after equivalent maintained behavior is verified. Do not delete retained code
merely because it is outside the production graph, and do not reconnect it as a compatibility fallback.

See the [implementation status](../Documents/IMPLEMENTATION.md), [compiler pipeline](../Documents/COMPILER-PIPELINE.md), and
[repository layout](../Documents/MONOREPO.md) for the maintained architecture.
