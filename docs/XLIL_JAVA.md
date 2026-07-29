<!--
SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
SPDX-License-Identifier: MPL-2.0
-->

# XLIL Java binding

The official Java binding is published as `org.xsslang:xlil:2026.1`. It targets Java 25 and calls the installed
`libxs_lil` shared library through the standard Foreign Function and Memory API.

The binding does not maintain a second XLIL implementation:

- `org.xsslang.xlil.writer` constructs modules through `XsLilBuilder`, invokes the native verifier, and emits canonical
  XLIL v1 text;
- `org.xsslang.xlil.reader` delegates parsing and verification to `xs_lil_module_parse_text`, then takes immutable Java
  snapshots through read-only C accessors;
- `org.xsslang.ffi.c.CString` provides explicit NUL-terminated UTF-8 conversion for FFM callers.

The X# Gradle platform integration selects `https://java.xsharp-lang.xyz/`, so application builds do not hard-code that
repository URL. A consumer declares the platform plugin and dependency:

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

Run modular applications with:

```text
--enable-native-access=org.xsslang.xlil
```

The native `xs_lil` library must be discoverable through the operating system's normal shared-library rules. Tests and
development tools may instead set the `org.xsslang.xlil.library` system property to an absolute library path.

The independent Maven-compatible repository is available at [java.xsharp-lang.xyz](https://java.xsharp-lang.xyz/).
