<!--
SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
SPDX-License-Identifier: MPL-2.0
-->

# xs

Buildable Visual X# compiler project.

This project owns the compiler CLI, `.vxs` lexer/parser, AST, HIR, MIR, XLIL planning,
LLVM backend bridge and compiler tests. New C23 compiler implementation files live under
`xs/sources/`.
