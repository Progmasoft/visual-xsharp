# Visual X# Kotlin project runtime

This Java 25/Kotlin component evaluates the single `Visual.XSharp.kts` project file used by `vxs`. It resolves `.vxs`
sources, compiler settings, targets, dependencies, tests, and publication metadata into the native compiler plan.

Split project scripts and separate module scripts are not supported. Project discovery walks from the requested path toward
the filesystem root until it finds `Visual.XSharp.kts`.

Dependency resolution writes `Visual.XSharp.Lockfile.sqlite3` as binary SQLite data. Human-readable dumps belong to the
separate `vxdc` tool and are not emitted during ordinary project evaluation.

Run the component tests with:

```text
gradlew test
```
