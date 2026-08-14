# Visual X# project files

## Discovery

Each project has one `Visual.XSharp.kts`. Project discovery walks from the requested location toward the filesystem root until
it finds that file. Split module scripts, compatibility setters, and separate project-state files are not supported.

## Minimal project

```kotlin
project {
  name = "Example"
  version = "0.1.0"
  stability = Stability.DEV
}

sources {
  main {
    srcDir = "Sources"
    entry = "Example.Main"
  }
}
```

`sources.main` and `sources.main.entry` are required. The entry is a namespace-qualified class, not a function name. The final
segment is the class name and is not restricted to `Main` or `Program`: `Namespace.Program`, `Namespace.Namespace.Program`,
and `Company.Tool.Bootstrap` are valid. A trailing namespace such as `Namespace.` is invalid. The selected class must declare
a parameterless `public static void Main()` method. Entry resolution uses namespace and type identity; it never derives a
file path from the entry. File names and directory layout do not have to match namespace segments.

## Compiler settings

```kotlin
compiler {
  version = "latest"
  standard = "latest"
  backend = Backend.LLVM
  buildMode = BuildMode.DEBUG
  emit = Emit.BINARY
  warnings = Warnings.MEDIUM
  warningsAsErrors = false
  experimentalWarnings = false
  shadowWarnings = false
  undefinedWarnings = true

  unsafe {
    typeSafeFormat = true
    xppOptimizationPasses = true
    xmmOptimizationPasses = true
  }

  llvm {
    optLevel = LlvmOptLevel.O0
    compiler = LlvmCompiler.AOT
    lto = LlvmLto.NONE
  }
}
```

## Sources and tests

```kotlin
sources {
  main {
    srcDir = "Sources"
    entry = "Example.Main"
    exclude("Generated/**")
  }

  test("unit") {
    testDir = "Tests/Unit"
    framework = "tests"
    exclude("Fixtures/**")
  }

  test("integration") {
    testDir = "Tests/Integration"
  }

  viget {
    publish = false
    exclude("build/**")
  }
}
```

Source roots must stay inside the project root. The Kotlin runtime validates the configured roots and forwards them with
their exclusion policy; it does not walk the roots or create a `.vxs` file list. Source discovery and namespace-based entry
resolution belong to the compiler. Exclusions accept project-relative glob patterns. `sources.main.exclude`, each named
test suite's `exclude`, and `sources.viget.exclude` have no implicit pattern: their plan value is `null` until an
`exclude(...)` declaration is present.

Test suites have case-sensitive Visual X# identifiers and retain their own root, framework, and exclusion policy across the
typed project plan and native compiler boundary. A suite defaults to `Tests/<suite-name>` when `testDir` is omitted. Duplicate
suite names are configuration errors; suites are never flattened into one anonymous test directory.

## Project plugins

Plugins are declared in a top-level preamble so the host can resolve them before Kotlin compiles the rest of the project
script:

```kotlin
plugins {
  plugin("Progmasoft") {
    name = "CMake"
    version = "0.1.0"
  }
}
```

Production Kotlin DSL plugin JARs use
`https://viget.xsharp-lang.xyz/dslplugins/<Publisher>/<Name>/`. The catalogue's version-index and JAR filename layout are not
yet connected to this runtime. ViGet is the only remote source; there is no configurable repository list. The host currently
loads installed artifacts from the project-local `.visual-xsharp/plugins` cache, which is not another repository. A plugin
JAR contains
`META-INF/visual-xsharp-plugin.properties` and exactly one matching `ProjectPlugin` service. The runtime validates the
coordinate, exact requested version, plugin API version, safe Kotlin imports, and SHA-256 identity before
activation. An adjacent `<plugin.jar>.sha256` file makes the expected digest explicit; the digest is checked again in the
Kotlin process before service loading.

Plugins can register named DSL extensions, contribute deterministic plan metadata, and run finalization hooks. Extension
name collisions and descriptor/service identity mismatches are errors. Plugins are trusted build logic with full JVM and
filesystem access; this infrastructure intentionally does not sandbox or restrict them.

A project can explicitly load a local Kotlin plugin JAR without introducing another repository:

```kotlin
plugins {
  plugin("local") {
    path = "plugin.jar"
  }
}
```

The path is project-relative, must remain inside the project root, and must end in `.jar`. Publisher, name, version, and API
identity still come from the verified JAR descriptor rather than the file name or project declaration.

DSL plugins do not have a stability field. `stability` is neither optional nor accepted in hosted or local plugin
declarations, descriptors, plans, or lockfiles.

## Dependencies

Dependencies use exact `Publisher.Name` coordinates and exact semantic versions:

```kotlin
dependencies {
  dependency("Publisher") {
    name = "Name"
    version = "1.2.3"
    stability = Stability.STABLE
  }
}
```

Optional dependencies name a feature and record whether that feature is enabled. The current runtime validates the typed
publisher, package name, stability, and exact version declarations, detects conflicts, and writes that direct manifest to the
lock database. It does not call this a resolved dependency graph. A complete transitive package solver and network package
client are separate implementation work.

Standard-library namespaces are not repeated as package dependencies.

A local Visual X# package is an explicit file source, not a repository coordinate:

```kotlin
dependencies {
  dependency("local") {
    path = "dependency.vipkg"
  }
}
```

The project-relative path must stay inside the project root and use `.vipkg`. The declaration is preserved separately in the
plan and lockfile; it is not misreported as a solved or downloaded ViGet dependency.

Visual X# packages are distinct from Kotlin DSL plugins. They are Visual X#-authored `.vipkg` artifacts catalogued under
`https://viget.xsharp-lang.xyz/<Publisher>/<Name>/`; dependency solving and registry transport remain separate work.

## Lock file

Project evaluation writes `Visual.XSharp.Lockfile.sqlite3`. It is a binary SQLite database, not a text or JSON lock file.
Ordinary evaluation never emits a textual copy. Use the separate Visual XSharp Dumpfile Creator when a reviewable or
replayable SQL representation is needed:

```powershell
vxdc -Projectfile .\Visual.XSharp.kts -Output .\Project.sqlite3.dump
```

VXDC evaluates the named project, refreshes its binary lockfile, validates the lockfile format, and writes deterministic
UTF-8 SQL. The output contains schema and quoted row data inside a transaction and can be replayed by SQLite. Its name must
be supplied explicitly, but VXDC does not impose a filename extension. It refuses to overwrite the binary lockfile with
text.

## Additional blocks

The DSL also supports:

- `outdirs` for Debug and Release output directories;
- `targets` for target triples;
- `authors` for publication metadata;
- `pml` for PML enablement; and
- `workspaces` for named project paths.

`panic("message")` immediately aborts project-script evaluation. Because the evaluator emits the plan, source registry, and
lock only after the script returns normally, a panic does not start Visual X# compilation and does not emit partial project
state.

`eprint(value)` and `eprintln(value)` mirror Kotlin's standard output helpers but write to standard error. Both accept
nullable values; `eprintln` appends the platform line separator.

Author declarations take exactly two values named `user` and `mail`; the function is not variadic:

```kotlin
authors {
  author("Leitwolf", "leitwolf@example.me")
}
```
