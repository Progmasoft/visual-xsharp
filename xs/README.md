<!--
SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
SPDX-License-Identifier: MPL-2.0
-->

# xs

Buildable Visual X# compiler project.

This project owns the C++20 compiler CLI and Core-to-LLVM driver, the Haskell lexer-through-CorePrep frontend, project-runtime
integration, compatibility libraries, and compiler tests. The public driver has no DIMCLI dependency. The retired C
lexer/parser and their duplicate semantic pipeline are not part of this project anymore.
