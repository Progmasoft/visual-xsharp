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

`sources.main` and `sources.main.entry` are required. The entry is a namespace-qualified class, not a function name. The class
must declare a parameterless `public static void Main()` method.

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
glob patterns.

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

## Lock file

Project evaluation writes `Visual.XSharp.Lockfile.sqlite3`. It is a binary SQLite database, not a text or JSON lock file.

## Additional blocks

The DSL also supports:

- `outdirs` for Debug and Release output directories;
- `targets` for target triples;
- `authors` for publication metadata;
- `pml` for PML enablement; and
- `workspaces` for named project paths.
