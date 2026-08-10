<!--
SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
SPDX-License-Identifier: MPL-2.0
-->

# Visual X# command-line interface

The compiler command is `vxs`. Visual X# source files use `.vxs`; native executables use `.vxse`.

## Commands

```text
vxs check
vxs build
vxs run
vxs test
vxs resolve
vxs update
vxs install Publisher.Name
vxs install -Global Publisher.Name
vxs viget push
vxs viget update
vxs version
```

`update` refreshes system and project dependencies. `install` obtains a ViGet package for the project or, with `-Global`,
for the system. The ViGet subcommands publish a new package or a new version.

Visual Formatter and Visual Linter are distributed as `Progmasoft.VisualFormatter` and `Progmasoft.VisualLinter` packages.

## Project and file selection

With `Visual.XSharp.kts`, the working directory determines the project and `-File` is unnecessary. There is no `-Project`
flag. A direct source invocation uses `-File path/to/file.vxs`.

## Compiler options

- `-Standard 26|latest`
- `-Compiler-Version VERSION|latest`
- `-Werror true|false`
- `-Warnings all|medium|low|none`
- `-Wexperimental true|false`
- `-Wshadow true|false`
- `-Type-Safe-Format true|false`
- `-Wundef true|false`
- `-Backend llvm`
- `-Llvm-OptLevel 1|2|3|g`
- `-Llvm-Compiler aot|orc`
- `-Llvm-Lto fat|thin|none`
- `-Xpp-Optimization-Passes true|false`
- `-Xmm-Optimization-Passes true|false`
- `-Emit binary|object|core|xpp|xmm|assembly|llvmll|llvmbc`
- `-Build object|vxs|core|xpp|xmm|llvmll|llvmbc`

`-Emit` defaults to `binary`; `assembly` means LLVM-generated assembly. `-Build` defaults to `vxs` and controls the input
stage used to produce the final native executable. Core, Xpp, and Xmm artifacts use `.core`, `.xpp`, and `.xmm`.

Intermediate representations stay in memory unless `-Emit` explicitly requests a file.
Xmm optimization passes are enabled by default in both project and direct-file builds.
