<!--
SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
-->

# Visual X# compiler

Buildable Visual X# compiler project.

This project owns the C++20 compiler CLI and Core-to-LLVM driver, the Haskell source-set loader and lexer-through-CorePrep
frontend, Kotlin project evaluation, compatibility libraries, and compiler tests. Project roots are discovered and merged
by namespace in Haskell rather than mapped from entry names to file paths. The public driver has no DIMCLI dependency. The retired C
lexer/parser and their duplicate semantic pipeline are not part of this project anymore.

See the [compiler pipeline](../Documents/COMPILER-PIPELINE.md), [architecture](../Documents/ARCHITECTURE.md), and
[building guide](../Documents/BUILDING.md) for component ownership and supported workflows.
