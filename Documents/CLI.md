<!-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com> -->
<!-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0 -->

# `vxs` command-line interface

## Commands

The parser recognizes:

```text
check
build
format
lint
run
test
resolve
update
install
viget
version
```

The general invocation shape is:

```text
vxs <command> [command options] [command positional values]
vxs -Help
vxs <command> -Help
```

Command, option, and enumerated-value spelling is case-sensitive. Compiler options use one leading hyphen and Pascal-style
segments. Version reporting is available only through the explicit `vxs version` command.

| Command | Purpose | Current connection |
| --- | --- | --- |
| `check` | validate source or a supported explicit artifact | connected for project/source/Core |
| `build` | emit the selected artifact | connected for project/source/Core within documented output limits |
| `run` | build a fresh binary and execute it | connected for the implemented source subset |
| `test` | execute a named project test suite | registered; framework runner not linked |
| `format` | format the complete project source set | connected dispatch; requires installed `vfmt` |
| `lint` | lint the complete project source set | connected dispatch; requires installed `vlint` |
| `resolve` | evaluate the project and refresh the lockfile | connected project operation |
| `update` | reevaluate dependencies and refresh the lockfile | connected project operation |
| `install` | install a ViGet package | registered; ViGet client not linked |
| `viget` | perform registry publication/update action | registered; ViGet client not linked |
| `version` | report compiler version through command form | recognized version outcome |

Registered commands fail explicitly. They never print success while skipping the requested package or test operation.

`check`, `build`, `run`, and `test` accept either a discovered `Visual.XSharp.kts` project or a `.vxs` file selected with
`-File`.

`format` and `lint` are project-only commands. They evaluate the project source policy, apply its roots and exclusions,
and invoke the separately installed ecosystem tool for every discovered `.vxs` source:

```text
vxs format  # requires Progmasoft.VisualFormatter
vxs lint    # requires Progmasoft.VisualLinter
```

Visual Formatter reads `Visual.Formatter.kts` from the project root when present and otherwise uses its defaults. Visual
Linter applies the equivalent rule for `Visual.Linter.kts`. Tool configuration remains owned by the tool rather than the
compiler CLI.

Package commands use typed positional forms:

```text
vxs install [-Global] Publisher.Name
vxs viget push|update
```

## Canonical options

```text
-File PATH
-Standard 26|latest
-Compiler-Version VERSION|latest
-Target TARGET-TRIPLE
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

`build` additionally parses `-Emit`. `build` and `check` parse `-Build` for an explicit input artifact:

```text
-Emit binary|object|core|xpp|xmm|assembly|llvmll|llvmbc
-Build object|vxs|core|xpp|xmm|llvmll|llvmbc
```

Canonical compiler settings use one leading hyphen and the displayed capitalization. `-Help` is the canonical help
control. Legacy GNU-style controls, including `--help` and `--version`, are rejected.

## Parser model

The parser converts raw process arguments into typed command, input, output, warning, Boolean, LLVM, and ViGet-action
values before dispatch. Its owning C++20 result separates ready, help, version, and error outcomes; parsing itself does not
write process output. The driver renders the selected outcome. Later compiler stages do not compare command strings or
reinterpret option values.

One declarative schema owns canonical spelling, value domain, arity, command scope, defaults, and help descriptions.
Consequently, `vxs check -Help` shows `check` options but not build-only `-Emit`, while `vxs build -Help` includes it.
Option and value spelling is case-sensitive.

## Configuration precedence

For every scalar compiler setting, the driver resolves values in this order, from strongest to weakest:

1. an option explicitly supplied in the current `vxs` command;
2. the value produced by the project's `Visual.XSharp.kts` evaluation;
3. the Kotlin DSL's built-in default for that value;
4. the CLI fallback used when no project is being evaluated.

The evaluator deliberately materializes both user-written values and Kotlin DSL defaults. The native driver therefore
uses the evaluated project as one authoritative layer and never lets an omitted CLI option overwrite it. This applies to
`emit`, warning policy, optimization settings, compiler/standard selection, and other transported compiler settings.

`-Target` accepts a case-sensitive LLVM target triple such as `x86_64-pc-windows-msvc`. In project mode, an explicitly
selected target must occur in the DSL's `targets { target(...) }` catalog when that catalog is non-empty. Without an
explicit target, LLVM selects its host-dependent default; the first catalog entry is not chosen implicitly.

An explicit non-source `-Build` always requires `-File`. For example, `vxs check -Build core` is rejected rather than
silently discovering a project; use `vxs check -Build core -File Module.core`.

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
target: host-dependent
Xpp optimization passes: true
Xmm optimization passes: true
```

### Option scope

| Option | `check` | `build` | `run` | `test` |
| --- | :---: | :---: | :---: | :---: |
| `-File` | yes | yes | yes | yes |
| `-Standard` | yes | yes | yes | yes |
| `-Compiler-Version` | yes | yes | yes | yes |
| `-Target` | yes | yes | yes | yes |
| warning and backend settings | yes | yes | yes | yes |
| LLVM and Xpp/Xmm settings | yes | yes | yes | yes |
| `-Build` | yes | yes | no | no |
| `-Emit` | no | yes | no | no |

The declarative schema is the authority for scope. A value accepted for one command is not ignored on another command; it is
reported as out of scope.

### Help behavior

```powershell
vxs -Help
vxs check -Help
vxs build -Help
```

Help is resolved before project evaluation and compilation. `build` help includes `-Emit`; `check` help does not. Unknown
options still fail even if their spelling resembles a common convention such as `--help`.

## Current reliable operation

Single-file validation:

```powershell
vxs check -File .\Sources\Main.vxs
```

The Haskell frontend can emit verified Core, LLVM forms, target-machine output, or a native executable:

```powershell
vxs build -File .\Sources\Main.vxs -Emit core
vxs build -File .\Sources\Main.vxs -Emit xpp
vxs build -File .\Sources\Main.vxs -Emit xmm
vxs build -File .\Sources\Main.vxs -Emit llvmll
vxs build -File .\Sources\Main.vxs -Emit llvmbc
vxs build -File .\Sources\Main.vxs -Emit object
vxs build -File .\Sources\Main.vxs -Emit assembly
vxs build -File .\Sources\Main.vxs
vxs run -File .\Sources\Main.vxs
```

The accepted source subset is the subset implemented by the Haskell frontend. Native binary, object, assembly, and `run`
routes use the renewed C++20 backend and never fall back to the removed compatibility frontend. Named test-suite execution
remains pending.

Project artifacts use the active Kotlin DSL output directory (`build/debug` or `build/release` by default). `binary`
produces one project executable with the `.vxse` extension. Source-oriented outputs such as `object` and `assembly` use
the source stem directly in that directory, so `Sources/MyApp/Main.vxs` maps to `build/debug/Main.o` in a debug object
build. Multiple sources consequently produce multiple artifacts, not one merged object. An artifact left by an earlier
build is replaced by `vxs build`. Two different inputs in the same build that map to the same output name are rejected as
an ambiguous source-name collision. Project binary emission is connected. Project-wide object and assembly emission remains
disabled until Core preserves source ownership, so the compiler cannot accidentally collapse multiple inputs into one file.

## Core input status

The CLI reserves the real Core artifact commands:

```powershell
vxs check -Build core -File module.core
vxs build -Build core -Emit llvmll -File module.core
vxs build -Build core -Emit llvmbc -File module.core
```

The native C++20 route reads the Haskell `VXCR` v3 contract with byte, collection, text, type-depth, and expression-depth
limits. It verifies Core semantics before adapting nested expressions and source control flow to CorePrep, then runs the
existing verified CorePrep → Xpp → Xmm → LLVM pipeline entirely in memory. `check` writes nothing. The two `build` examples
write a sibling `.ll` or `.bc` file. A Core build can also write a sibling `.o` or `.asm`, or link a `.vxse`; binary is the
default emit kind. The older `VXCP` transport remains internal and is rejected when supplied as `.core`.

### Xpp and Xmm artifact routes

```powershell
vxs check -Build xpp -File module.xpp
vxs build -Build xpp -File module.xpp -Emit xmm
vxs check -Build xmm -File module.xmm
vxs build -Build xmm -File module.xmm -Emit llvmll
```

`VXPP` and `VXMM` are bounded, versioned binary contracts. Both readers reject
invalid magic/version/flags, malformed tags, invalid Unicode, non-canonical
numeric payloads, excessive counts, truncated fields, and trailing bytes.
Decoded models pass their stage verifier before optimization or lowering.

Conversions move forward through the pipeline. Xmm cannot be converted back
to Xpp or Core, and Xpp cannot be converted back to Core. Rewriting the same
stage is permitted and replaces the artifact after successful verification.

## Registered but not connected

The CLI reserves the renewed artifact vocabulary before all routes are implemented:

- `install` and `viget` report that the ViGet client is not linked into the compiler build.

`resolve` and `update` evaluate the project configuration and refresh `Visual.XSharp.Lockfile.sqlite3`.

Lockfile text export is deliberately not a `vxs` command. The separately installed `vxdc` tool evaluates an exact
`Visual.XSharp.kts` and creates a deterministic SQL dump:

```powershell
vxdc -Projectfile .\Visual.XSharp.kts -Output .\Project.sqlite3.dump
```

## Exit behavior

Command parsing errors return a nonzero status. Compilation diagnostics are printed to standard error. `run` propagates
build failures and, after a successful link, returns the native process exit status.

Parse diagnostics retain context. Unknown commands/options name the rejected spelling; invalid typed values name the
option and its accepted domain; duplicate, missing-value, wrong-command-scope, positional, and invalid process-vector
errors are reported independently.

## Filesystem and overwrite behavior

Paths are interpreted by the owning command. Project discovery begins at the current/requested location and walks upward for
`Visual.XSharp.kts`; `-File` names one explicit input. The project evaluator and Haskell source loader canonicalize contained
roots rather than trusting textual prefixes.

`vxs build` replaces the artifact selected for the current invocation when that build succeeds. It does not preserve an old
artifact merely because it already exists, and it does not execute that old artifact after a failed `run` build. Temporary
Core/object files are implementation details and are cleaned on every owned exit path.

## Examples by intent

Validate a project with its evaluated defaults:

```powershell
vxs check
```

Override only the current target and warning policy:

```powershell
vxs check -Target x86_64-pc-windows-msvc -Warnings all -Werror true
```

Build a project executable using the project output directory:

```powershell
vxs build
```

Build one explicit source into LLVM IR without requiring its path to match a namespace:

```powershell
vxs build -File .\Scratch\Bootstrap.vxs -Emit llvmll
```

Validate a public Core artifact without writing output:

```powershell
vxs check -Build core -File .\Artifacts\Bootstrap.core
```

Convert that Core artifact to a native executable:

```powershell
vxs build -Build core -File .\Artifacts\Bootstrap.core -Emit binary
```

Refresh a project's binary lockfile:

```powershell
vxs resolve
vxs update
```

The two package-resolution commands currently share project evaluation and lock refresh behavior. A future transitive solver
or registry client must preserve their typed command identity instead of collapsing the public command surface into a string
switch.
