<!--
SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
SPDX-License-Identifier: MPL-2.0
-->

# XLIL Java

`org.xsslang:xlil:2026.1` is the official Java 25 binding for the stable
`lil-c` ABI. It provides:

- `org.xsslang.xlil.writer` for constructing and verifying XLIL modules;
- `org.xsslang.xlil.reader` for parsing XLIL v0 into immutable Java snapshots;
- `org.xsslang.ffi.c.CString` for explicit UTF-8 C string conversion.

The binding uses the Java 25 Foreign Function and Memory API. Applications must
make the native `xs_lil` shared library available and enable native access for
the binding module:

```text
--enable-native-access=org.xsslang.xlil
```

The X# Gradle platform plugin configures the official repository. Consumer
builds therefore declare the module through `xsslangPlatform` without adding a
repository URL directly.

```kotlin
plugins {
    id("java")
    id("org.xsslang.platform") version "26.1"
}

dependencies {
    xsslangPlatform {
        implementation("org.xsslang:xlil:2026.1")
    }
}
```

From the monorepo root, build and test the binding with the shared Gradle
wrapper:

```text
./xs_kts/gradlew -p xlil-java test
```

Create the complete Maven-compatible staging tree with:

```text
./xs_kts/gradlew -p xlil-java publishXlilPublicationToRepositoryRepository
```
