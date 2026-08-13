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

## Exit behavior

Command parsing errors return a nonzero status. Compilation diagnostics are printed to standard error. `run` returns the
native program's exit status after a successful build.
