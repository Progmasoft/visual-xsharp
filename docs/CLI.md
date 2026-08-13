# `vxs` command-line interface

## Commands

The parser recognizes:

```text
check
build
run
test
resolve
update
install
viget
version
```

`check`, `build`, `run`, and `test` accept either a discovered `Visual.XSharp.kts` project or a `.vxs` file selected with
`-File`.

## Canonical options

```text
-File PATH
-Standard 26|latest
-Compiler-Version VERSION|latest
-Warnings all|medium|low|none
-Werror true|false
-Wexperimental true|false
-Wshadow true|false
-Wundef true|false
-Type-Safe-Format true|false
-Backend llvm
-Llvm-OptLevel 1|2|3|g
-Llvm-Compiler aot|orc
-Llvm-Lto fat|thin|none
-Xpp-Optimization-Passes true|false
-Xmm-Optimization-Passes true|false
```

`build` additionally parses:

```text
-Emit binary|object|core|xpp|xmm|assembly|llvmll|llvmbc
-Build object|vxs|core|xpp|xmm|llvmll|llvmbc
```

Canonical compiler settings use one leading hyphen and the displayed capitalization. `--help` and `--version` are special
driver controls; other legacy long options are rejected.

## Defaults

```text
standard: latest
compiler version: latest
warnings: medium
warnings as errors: false
experimental warnings: false
shadow warnings: false
undefined warnings: true
type-safe format checking: true
backend: llvm
LLVM optimization: 2
LLVM compiler: aot
LLVM LTO: none
Xpp optimization passes: true
Xmm optimization passes: true
```

## Current reliable operation

Single-file validation:

```powershell
vxs check -File .\Sources\Main.vxs
```

The Haskell frontend can emit verified Core or continue to LLVM text/bitcode:

```powershell
vxs build -File .\Sources\Main.vxs -Emit core
vxs build -File .\Sources\Main.vxs -Emit llvmll
vxs build -File .\Sources\Main.vxs -Emit llvmbc
```

The accepted source subset is the subset implemented by the Haskell frontend. Native object, link, `run`, and `test`
reconnection remains pending; the CLI rejects those routes instead of falling back to the removed compatibility frontend.

## Core input status

The CLI reserves the real Core artifact commands:

```powershell
vxs check -Build core -File module.core
vxs build -Build core -Emit llvmll -File module.core
vxs build -Build core -Emit llvmbc -File module.core
```

The native C++20 route reads the Haskell `VXCR` v1 contract with byte, collection, text, type-depth, and expression-depth
limits. It verifies Core semantics before adapting nested expressions and source control flow to CorePrep, then runs the
existing verified CorePrep → Xpp → Xmm → LLVM pipeline entirely in memory. `check` writes nothing. The two `build` examples
write a sibling `.ll` or `.bc` file. Native object/executable output from Core and public Xpp/Xmm artifact codecs are not
connected yet. The older `VXCP` transport remains internal and is rejected when supplied as `.core`.

## Registered but not connected

The CLI reserves the renewed artifact vocabulary before all routes are implemented:

- `.xpp` and `.xmm` input routes report that the selected input is not connected;
- Core input does not yet emit native objects, executables, `.xpp`, or `.xmm` files;
- `install` and `viget` report that the ViGet client is not linked into the compiler build.

`resolve` and `update` evaluate the project configuration and refresh `Visual.XSharp.Lockfile.sqlite3`.

Lockfile text export is deliberately not a `vxs` command. The separately installed `vxdc` tool evaluates an exact
`Visual.XSharp.kts` and creates a deterministic SQL dump:

```powershell
vxdc -Projectfile .\Visual.XSharp.kts -Output .\Project.sqlite3.dump
```

## Exit behavior

Command parsing errors return a nonzero status. Compilation diagnostics are printed to standard error. `run` currently
fails with an explicit migration diagnostic because native object/link production is not connected.
