<!--
SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
SPDX-License-Identifier: MPL-2.0
-->

# xs project runtime

This directory contains the mandatory Kotlin/JVM 25 project runtime bundled with the `xs` package. It invokes the required
external `kotlin` command and emits a deterministic versioned project plan or exact source registry for the native `xs`
driver. It never reads or compiles `.xs` files. JRE 25 or newer and an executable `kotlin` scripting command are runtime
requirements of the unified package.

Projects use one `xs.project.kts` file or the `xs.settings.kts` + `xs.build.kts` pair. Only
`project(name, channel, version)` is required. `source.include` names recursive directory
roots; source/test excludes may use `*`, `**`, and `?` glob patterns. A case-sensitive `main.xs` is placed first when it
exists, but it is not required for library-only projects. Setting `XS_EXTENSION` replaces the selected suffix and the
recognized `main`/`lib` filenames. Optional sections cover authors, external modules,
multi-value settings such as `TARGET`, tests, compiler diagnostics, variables, and host-dependent `cfg(...)` branches. See `xs.project.kts` for the complete
working DSL example.

External coordinates use `Publisher.Name` names and exact versions. Required modules use
`addModule(name, stability, version)`. Feature-gated coordinates use
`addOptionalModule(feature, name, stability, version)` and remain inactive unless their feature is enabled:

```kotlin
dependencies {
  addModule("XSharp.Core", "STABLE", "1.0.0")
  addOptionalModule("JSON", "XSharp.JSON", "BETA", "2.0.0")
}

features {
  dependency("XSharp.JSON") {
    enable("JSON")
  }
}
```

`STABLE`, `BETA`, and `ALPHA` are the supported stability values; input is normalized to uppercase. Optional declarations,
their enabled state, and the active module registry are persisted in the reproducible `xs.lock.sqlite3` SQLite lock file.
The current resolver validates the direct registry but does not fetch packages or solve transitive dependencies yet.

`PUBLISH` is a single boolean project variable with a `false` default; it currently records publication intent without
performing a registry upload.

`BUILD_MODE` defaults to `Release`; `RELEASE_OUTDIR` and `DEBUG_OUTDIR` default to `build/release` and `build/debug`.
`XSPKG_TYPE` accepts a list containing `xlib`, `dylib`, `staticlib`, `cdylib`, and `bin`. If omitted, `lib.xs` infers
`xlib`, `main.xs` infers `bin`, and both files infer both artifacts. These filenames are inference inputs, not mandatory
project files.

When no source block is present, the resolver behaves as if `source { include("Sources") }` were written.
An existing project-root `Modules` directory is likewise the default module include; without that directory the module
registry defaults to empty.
Without an explicit test include, each effective source root contributes its existing `Test` child directory. Missing
default test directories are ignored and leave an empty test registry.

Source, module, and test exclusion metadata defaults to `*/**`. It is consumed by editor views and xspkg packaging,
not by compiler source discovery. Supplying exclusions replaces the category's default metadata; empty `exclude()` clears
it. Include roots remain recursive and authoritative for compiler input.
Resolved compiler registries cannot overlap: test roots win over module assignments, and module assignments win over
general source roots. The same physical file is emitted in at most one registry.
Each `source`, `test`, and `module` block accepts `filter(listOf(...))`; omitted filters are populated from the other two
effective include registries after filesystem defaults are known.

Non-canonical entry files use `set("BINARY", mapOf("name" to "tool", "path" to "Sources/tool.xs"))` or the analogous
`LIBRARY` setting. Multiple maps define multiple named artifacts, matching repeated `[[bin]]`/`[[lib]]` records.

In split mode, `xs.settings.kts` is evaluated before `xs.build.kts`, but sections are not assigned to either filename.
Both scripts may use the full DSL; the second script extends the state produced by the first.

The canonical recursive source form is:

```kotlin
source {
  include("Sources")
  exclude("Sources/tests/**")
}
```

Include roots do not accept globs. Discovery is recursive relative to the project root; exclude metadata is not applied
to compiler inputs. The deterministic result places a case-sensitive `main.xs` first when one exists. The resolver emits exact paths and does not
pass unresolved patterns to the compiler.
The internal registry also carries the evaluated `compiler {}` warning, warnings-as-errors, and verbose policy. The
The native compiler may replace those values for one invocation through its corresponding command-line overrides.

```text
./xs_kts/gradlew --daemon --build-cache -p xs_kts test
./xs_kts/gradlew --daemon --build-cache -p xs_kts run --args="evaluate xs_kts"
```

Project files are build programs and must be trusted. They are executed by the external `kotlin` script runner; `xs` does
not maintain or embed a substitute Kotlin interpreter. Kotlin scripting is not presented as a security sandbox.

Users build from the project directory with `xs build`. The native compiler invokes this bundled runtime for source
metadata and keeps all X# frontend/backend work inside `xs`. The runtime's `sources0` form is an internal protocol, not an
alternative X# build command or a separately installed user command.

`xs test` requests the separately resolved test registry and validates it together with project and module sources. The
compiler generates an isolated native `.vxse` harness for each supported top-level `#[Test]` function; `Ignore` and
`ShouldPanic` control execution expectations.
