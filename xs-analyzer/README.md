<!--
SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
SPDX-License-Identifier: MPL-2.0
-->

# xs-analyzer

Early X# analyzer project.

`xs-analyzer` contains both the Rust language server and the official Visual
Studio Code extension:

- Rust language server sources live under `xs-analyzer/sources/`.
- TypeScript VS Code extension sources live under `xs-analyzer/sources/extension/`.

The Rust server currently performs the LSP initialize/shutdown handshake and
advertises full-document UTF-16 synchronization. Semantic requests and editor
process management remain future work.

Visual Studio Code is the only officially supported IDE integration target.
JetBrains plugins are not planned as official project outputs.
