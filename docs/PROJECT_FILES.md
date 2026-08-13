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
a parameterless `public static void Main()` method.

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

  test {
    testDir = "Tests"
    framework = "tests"
    exclude("Fixtures/**")
  }

  viget {
    publish = false
    exclude("build/**")
  }
}
```

Source paths must stay inside the project root. The default source extension is `.vxs`. Exclusions accept project-relative
glob patterns. `sources.main.exclude`, `sources.test.exclude`, and `sources.viget.exclude` have no implicit pattern: their
plan value is `null` until an `exclude(...)` declaration is present.

## Project plugins

Plugins are declared in a top-level preamble so the host can resolve them before Kotlin compiles the rest of the project
script:

```kotlin
plugins {
  plugin("Progmasoft") {
    name = "CMake"
    version = "0.1.0"
    stability = Stability.STABLE
  }
}
```

Production Kotlin DSL plugin JARs use
`https://viget.xsharp-lang.xyz/dslplugins/<Publisher>/<Name>/`. The catalogue's version-index and JAR filename layout are not
yet connected to this runtime. ViGet is the only remote source; there is no configurable repository list. The host currently
loads installed artifacts from the project-local `.visual-xsharp/plugins` cache, which is not another repository. A plugin
JAR contains
`META-INF/visual-xsharp-plugin.properties` and exactly one matching `ProjectPlugin` service. The runtime validates the
coordinate, exact requested version and stability, plugin API version, safe Kotlin imports, and SHA-256 identity before
activation. An adjacent `<plugin.jar>.sha256` file makes the expected digest explicit; the digest is checked again in the
Kotlin process before service loading.

Plugins can register named DSL extensions, contribute deterministic plan metadata, and run finalization hooks. Extension
name collisions and descriptor/service identity mismatches are errors. Plugins are trusted build logic with full JVM and
filesystem access; this infrastructure intentionally does not sandbox or restrict them.

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

Visual X# packages are distinct from Kotlin DSL plugins. They are Visual X#-authored `.vipkg` artifacts catalogued under
`https://viget.xsharp-lang.xyz/<Publisher>/<Name>/`; dependency solving and registry transport remain separate work.

## Lock file

Project evaluation writes `Visual.XSharp.Lockfile.sqlite3`. It is a binary SQLite database, not a text or JSON lock file.

## Additional blocks

The DSL also supports:

- `outdirs` for Debug and Release output directories;
- `targets` for target triples;
- `authors` for publication metadata;
- `pml` for PML enablement; and
- `workspaces` for named project paths.

`panic("message")` immediately aborts project-script evaluation. Because the runtime emits the plan, source registry, and
lock only after the script returns normally, a panic does not start Visual X# compilation and does not emit partial project
state.

Author declarations take exactly two values named `user` and `mail`; the function is not variadic:

```kotlin
authors {
  author("Leitwolf", "leitwolf@example.me")
}
```
