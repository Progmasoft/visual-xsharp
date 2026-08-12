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

Single-file native build is available for the source subset supported by the current Rust compiler core and LLVM backend:

```powershell
vxs build -File .\Sources\Main.vxs
```

Success depends on the source body being supported by that route. This command does not prove that the intended Haskell to
CorePrep to Xpp/Xmm pipeline is already the production owner.

## Connected CorePrep input

Version `0.3.0` supports explicit CorePrep artifacts:

```powershell
vxs check -Build core -File module.core
vxs build -Build core -File module.core
```

The driver validates the `.core` extension, applies bounded wire decoding and semantic verification, then creates Xpp, Xmm,
LLVM IR, and LLVM bitcode in memory. `-Xpp-Optimization-Passes` and `-Xmm-Optimization-Passes` control the middle-end passes;
both default to `true`. `-Llvm-OptLevel` selects the backend pass pipeline.

The build command can explicitly persist either final LLVM representation beside the input artifact:

```powershell
vxs build -Build core -File module.core -Emit llvmll
vxs build -Build core -File module.core -Emit llvmbc
```

These commands write `module.ll` and `module.bc`, respectively. `check` always remains non-emitting and rejects `-Emit`.

## Registered but not connected

The CLI reserves the renewed artifact vocabulary before all routes are implemented:

- explicit `-Emit` requests other than `llvmll` and `llvmbc` from `.core` input report that the selected writer is not
  connected;
- `-Build` inputs other than `vxs` and `core` currently report that the selected input is not connected; and
- `install` and `viget` report that the ViGet client is not linked into the compiler build.

`resolve` and `update` evaluate the project configuration and refresh `Visual.XSharp.Lockfile.sqlite3`.

## Exit behavior

Command parsing errors return a nonzero status. Compilation diagnostics are printed to standard error. `run` returns the
native program's exit status after a successful build.
