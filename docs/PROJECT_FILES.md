<!--
SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
SPDX-License-Identifier: MPL-2.0
-->

# Project files and packages

The modern X# project resolver is part of `/usr/bin/xs`. The unified package requires JRE 25 or newer and an external
`kotlin` command with the Kotlin scripting runtime. The JRE and Kotlin runtime are not embedded. Project scripts are trusted
build code, not sandboxed input, and execution is delegated to the real Kotlin command; `xs` has no Kotlin interpreter.

A project may use one combined `xs.project.kts` file:

```kotlin
project("Example", "BETA", "0.1.0")

set("XS_VERSION", "0.2.4")
set("XS_BACKEND", "LLVM")
set("PUBLISH", false)
set("BUILD_MODE", "Release")
set("RELEASE_OUTDIR", "build/release")
set("DEBUG_OUTDIR", "build/debug")
set("XSPKG_TYPE", listOf("xlib", "bin"))
set(
  "TARGET",
  "x86_64-unknown-linux-gnu",
  "x86_64-unknown-linux-musl",
  "aarch64-unknown-linux-gnu",
  "armv7h-unknown-linux-gnueabihf",
  "riscv64gc-unknown-linux-gnu",
  "x86_64-apple-darwin",
  "aarch64-apple-darwin",
  "x86_64-pc-windows-msvc",
  "aarch64-pc-windows-msvc",
  "x86_64-unknown-freebsd",
  "aarch64-unknown-freebsd",
)

if (cfg(OS == LINUX) && cfg(ARCH == X86_64)) {
  set("NATIVE_TARGET", "x86_64-unknown-linux-gnu")
} else if (cfg(FAMILY == BSD)) {
  set("NATIVE_TARGET", "x86_64-unknown-freebsd")
} else {
  panic("unsupported build host")
}

authors(
  arrayOf("Leitwolf", "leitwolf@example.me"),
  arrayOf("Helmut", "helmut@example.me"),
)

dependencies {
  addModule("JSON", "stable", "0.1.0")
  addModule("XML", "stable", "0.1.0")
}

source {
  include("Sources")
  exclude("Sources/tests/**")
}

module {
  include("Modules")
}

test {
  include("Tests")
  exclude("Tests/fixtures/**")
  framework("xs-test")
}

compiler {
  warnings("all")
  werror(true)
  verbose(false)
}
```

Alternatively, configuration may be split between `xs.settings.kts` and `xs.build.kts`. Both files are required in split
mode, and `xs.project.kts` cannot coexist with them. Each file is evaluated separately by `kotlin`, so normal Kotlin
import and diagnostics retain their file boundary.

Only `project(name, channel, version)` is required. `source` is the source block name. An include names a project-relative directory and recursively selects files
with the configured source extension. Include roots do not accept globs. Omitting the block uses
`source { include("Sources") }`. The separate editor/package exclusion metadata defaults to `*/**`; it never removes or
changes compiler inputs selected by `include`. The default extension is `xs`. A
case-sensitive `main.xs`, when present, is placed first, but
library-only projects do not need one. `set("XS_EXTENSION", "xsharp")` selects `.xsharp` files and changes the recognized
entry/library names to `main.xsharp` and `lib.xsharp`.

When `test.include` is omitted, the resolver looks for a `Test` directory directly under each effective source root. Any
existing directories become test roots; if none exist, the test registry remains empty. An explicit test include replaces
this inference. Test editor/package exclusion metadata independently defaults to `*/**`.

The DSL also provides:

- `set(name, value, ...)` for one or more versioned/compiler and user build values;
- `get(name)` to read a previously defined variable, with an error for an unknown name. Read-only project identity is
  available as `PROJECT`; ordinary variables include values such as `XS_VERSION`, `XS_BACKEND`, and `TARGET` when the
  script defines them;
- `getAll(name)` to read every value without joining a multi-value setting;
- `authors(...)` for project authors;
- `dependencies { addModule(name, stability, version) }` for exact external module coordinates;
- `module { include(...); exclude(...); filter(...) }` for the pool assigned by `xs.module.kts`;
- `test { include(...); exclude(...); filter(...); framework(...) }` for recursive test discovery metadata;
- `compiler { warnings(...); werror(...); verbose(...) }` for diagnostic policy;
- `cfg(...)`, `OS`, `FAMILY`, and `ARCH` for ordinary Kotlin conditional configuration;
- `panic(...)` to reject the project configuration.

`PROJECT` is formatted as one stable identity string:

```kotlin
println(get("PROJECT"))
```

```text
Example, BETA, 0.1.0
```

The compiler policy is transferred with the resolved source registry to the native `xs` process. Command-line
`--warning`, `--werror`, and `--verbose` values are one-shot overrides applied after KTS evaluation; they never rewrite
the project script. The defaults are `warnings("medium")`, `werror(false)`, and `verbose(true)`.

`XS_BACKEND` currently accepts `LLVM`; `XPLR` is reserved for the future X Platform runtime. When LLVM is selected,
`XS_LLVM_LTO` is a boolean that defaults to `true`, and `XS_LLVM_OPT_LEVEL` accepts `0`, `1`, `2`, or `3` and defaults to
`3`. These LLVM-specific values are omitted from the effective plan for other backends. PGO is always active in the LLVM
pipeline and cannot be disabled. LLVM ORC JIT is not part of this backend; JIT execution belongs to higher compiler IR
layers and, in the future, to X Platform.

Memory management is derived from the backend and has no independent switch: LLVM always uses the X# RAII model, while
the future XPLR backend always uses XPG. `XGC_ENABLED` and `XPG_ENABLED` are rejected configuration keys.

`PUBLISH` is a reserved single-boolean project variable and defaults to `false`. Use `set("PUBLISH", true)` only to
express publication intent in the generated project plan. Package upload is not implemented yet, so evaluation never
publishes a module by itself.

`BUILD_MODE` accepts exactly `Release` or `Debug` and defaults to `Release`. The corresponding output directories default
to `build/release` and `build/debug` through `RELEASE_OUTDIR` and `DEBUG_OUTDIR`. `XSPKG_TYPE` accepts `xlib`, `dylib`,
`staticlib`, `cdylib`, and `bin`; a list may request several artifacts. When it is omitted, resolution infers `xlib` from
`lib.<XS_EXTENSION>`, `bin` from `main.<XS_EXTENSION>`, or both when both files exist. Neither filename is mandatory, and
an explicit `XSPKG_TYPE` overrides this inference.

Projects that do not use the canonical filenames declare named artifact roots explicitly. `BINARY` and `LIBRARY` are
the KTS equivalents of repeated `[[bin]]` and `[[lib]]` records:

```kotlin
set(
  "BINARY",
  mapOf("name" to "server", "path" to "Sources/server.xs"),
  mapOf("name" to "admin", "path" to "Sources/admin.xs"),
)
set("LIBRARY", mapOf("name" to "protocol", "path" to "Sources/protocol.xs"))
```

Every target name must be unique within its category and every path must be a selected project source inside the project
root. A requested `bin` without `main.<XS_EXTENSION>` requires `BINARY`; a requested library kind without
`lib.<XS_EXTENSION>` requires `LIBRARY`. Explicit artifact records also participate in default `XSPKG_TYPE` inference.

## External module lock

Each successful KTS project evaluation writes `xs.lock.sqlite3` in the project root. `xs resolve` performs the same
dependency resolution explicitly and atomically refreshes that lock without compiling X# sources. This is a real SQLite lock
file with format version `0`; its `modules` table stores the exact case-sensitive `name`, `stability`, and `version`
declared by `addModule`. Records are sorted and the database contains no timestamps, so lock updates are reproducible.
Repeating an identical coordinate is harmless, while declaring
the same module name with different stability or version values is an error.

The current resolver records and validates coordinates but does not download packages. Registry source identity,
integrity hashes, and dependency graphs will be added with external module resolution rather than being guessed in the
version-0 schema.

For example, this declaration:

```kotlin
dependencies {
  addModule("JSON", "stable", "0.1.0")
  addModule("XML", "beta", "0.2.0")
}
```

produces a database equivalent to the following SQLite contents:

```text
sqlite> SELECT key, value FROM metadata ORDER BY key;
format_version|0

sqlite> SELECT name, stability, version FROM modules ORDER BY name;
JSON|stable|0.1.0
XML|beta|0.2.0
```

The version-0 schema is intentionally small:

```sql
CREATE TABLE metadata (
  key TEXT PRIMARY KEY,
  value TEXT NOT NULL
) WITHOUT ROWID;

CREATE TABLE modules (
  name TEXT PRIMARY KEY,
  stability TEXT NOT NULL,
  version TEXT NOT NULL
) WITHOUT ROWID;
```

`xs.lock.sqlite3` is always binary SQLite data. Neither normal evaluation nor `xs resolve` emits a readable stand-in,
SQL script, or text serialization. The dump and schema above are documentation only.

## Source registries

`source` defines one or more directory roots for the exact source registry passed to the native compiler:

```kotlin
source {
  include("Sources")
  include("Platform/${OS.name.lowercase()}")
  exclude("Sources/generated/**")
  exclude("Sources/**/*_test.xs")
}
```

The supported pattern operators are:

| Pattern | Meaning |
| --- | --- |
| `*` | zero or more characters inside one path segment |
| `?` | exactly one character inside one path segment |
| `**` | zero or more complete path segments |

Include roots are evaluated relative to the project directory, must exist as directories, and must not escape the
project. They are searched recursively. Exclusion patterns are stored for editor views and xspkg packaging and do not
filter the compiler source registry. The package concept is named xspkg; its archive suffix is `.xspkg.tar.zst`, never
`.xspkg` alone. `source.exclude`, `module.exclude`, and `test.exclude` each default to `*/**`; an
explicit list replaces that category's default metadata, while empty `exclude()` clears it. Duplicate paths are removed, and the
resulting registry is sorted deterministically. At most one case-sensitive `main.<XS_EXTENSION>` may be selected; when
present it is ordered first. A registry without that file remains valid for non-`bin` package targets.

## Module registries

X# source files do not contain a `module` declaration. Projects that need importable modules select a module source pool
in `xs.project.kts` and assign every selected file in a sibling `xs.module.kts` file:

When `module.include` is omitted, an existing project-root `Modules` directory is selected automatically. If that
directory does not exist, the default module root is empty. An explicit include or the CLI `--module` override replaces
the filesystem default. Module exclusions are editor/package metadata and do not alter this compiler module pool.

```kotlin
// xs.project.kts
project("Example", "BETA", "0.1.0")

source {
  include("Sources")
}

module {
  include("Modules")
}
```

```kotlin
// xs.module.kts
module {
  name("MyModule")
  members {
    add("Modules/Example/*.xs")
  }

  submodule {
    name("util")
    add("Modules/Example/Utils/**/*.xs")
  }
}

module {
  name("FooModule")
  add("Modules/Foo/*.xs")
}
```

`add(...)` accepts a concrete source path or a glob. Direct `add` and `members` entries expose declarations as
`MyModule::<item>`; `submodule` above exposes them as `MyModule::util::<item>`. Every file selected by the project module
pool must be assigned exactly once. Missing/empty matches, duplicate assignments, and unassigned module files are
configuration errors. Compiler registries are always disjoint: test selection takes precedence over module membership,
and module membership takes precedence over the general source roots.

Each registry also accepts `filter(listOf(...))`. When omitted, filters reference the other two effective include
registries: source filters test and module roots, test filters source and module roots, and module filters source and test
roots. Filters establish registry ownership; they are distinct from `exclude`, which is editor/package metadata.

```kotlin
source { filter(listOf("Tests", "Modules")) }
test { filter(listOf("Sources", "Modules")) }
module { filter(listOf("Sources", "Tests")) }
```

The project normally declares the module root with `module { include("Modules") }`. If it does not, the compiler
invocation must provide it explicitly with `xs build --module ./Modules`.

`xs.module.kts` requires the Kotlin project runtime bundled with `xs`. Argument-free `xs build`/`xs check` ask that runtime
for a resolved registry containing physical paths and logical module names. A direct `-file` invocation
does not load module project metadata.

For `.xs` sources, `PascalCase` directories and `snake_case.xs` file names are canonical but not mandatory.

## Split project scripts

There is no section-to-file ownership rule. `xs.settings.kts` is evaluated first, then `xs.build.kts` receives and may
extend the complete accumulated state. Either file may call any DSL function. The following is only one possible
arrangement, with identity and shared metadata in the first file:

```kotlin
// xs.settings.kts
project("Example", "BETA", "0.1.0")
set("XS_BACKEND", "LLVM")
authors(arrayOf("Leitwolf", "leitwolf@example.me"))

dependencies {
  addModule("JSON", "stable", "0.1.0")
}
```

The second file then adds sources and compiler policy:

```kotlin
// xs.build.kts
source {
  include("Sources")
  exclude("Sources/tests/**")
}

set(
  "TARGET",
  "x86_64-unknown-linux-gnu",
  "x86_64-unknown-linux-musl",
  "aarch64-unknown-linux-gnu",
  "armv7h-unknown-linux-gnueabihf",
  "riscv64gc-unknown-linux-gnu",
  "x86_64-apple-darwin",
  "aarch64-apple-darwin",
  "x86_64-pc-windows-msvc",
  "aarch64-pc-windows-msvc",
  "x86_64-unknown-freebsd",
  "aarch64-unknown-freebsd",
)

test {
  include("Tests")
  framework("xs-test")
}

compiler {
  warnings("all")
  werror(true)
  verbose(false)
}
```

The split form is stateful. `xs.settings.kts` is evaluated first, `xs.build.kts` second, and an optional
`xs.module.kts` last. The module script receives the accumulated project state. Across the final state,
`project(...)` must be called exactly once and at least
one source include must exist; neither requirement is tied to a particular file. Normal Kotlin expressions, local
values, string interpolation, collections, loops, and conditionals remain available because these are real Kotlin
scripts rather than a simulated Kotlin parser.

BSD hosts are members of both the `BSD` and `UNIX` families. Consequently, `cfg(FAMILY == BSD)` and
`cfg(FAMILY == UNIX)` are both true on FreeBSD, OpenBSD, and NetBSD. Linux and macOS satisfy only `FAMILY == UNIX`;
they do not satisfy `FAMILY == BSD`.
Windows and ReactOS satisfy `FAMILY == WINDOWS`. ReactOS intentionally has no public `OS == REACTOS` DSL value and does
not satisfy `OS == WINDOWS`; family checks are the portable way to select both hosts.

Argument-free `xs build`, `xs check`, and `xs run` search the current directory and its parents through the bundled project runtime.
The resolver evaluates Kotlin, expands source metadata, and returns an exact source registry; it never parses or compiles
`.xs` contents. The native `xs` process owns source parsing, semantic analysis, code generation, and artifacts.
`XS_KOTLIN` may select the required Kotlin command. `XS_PROJECT_RUNTIME` is an internal testing/deployment override for the
bundled runtime launcher.

## Distribution packages

- `xs` installs `/usr/bin/xs`, the bundled project runtime, and the public headers and libraries.
- The package requires JRE 25 or newer plus the `kotlin` scripting command; there is no JVM-free compiler package and no
  separately installed `xs-project` command.
- Future `xsfmt` and `xstidy` packages remain independently installable tools.
