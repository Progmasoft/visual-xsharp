<!--
SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
SPDX-License-Identifier: MPL-2.0
-->

# Build and test guide

xs-project is built on C++23 Preview, Rust, C23, CMake, Ninja, Clang/LLVM, and LLD. The documented and tested build path uses
the Clang/LLVM toolchain.

## Required tools

- `cmake` 3.31 or newer
- `ninja`
- `clang`
- LLVM tools:
  - `llvm-ar`
  - `llvm-ranlib`
  - `llvm-nm`
  - `llvm-objcopy`
  - `llvm-objdump`
  - `llvm-strip`
- `ld.lld`
- libarchive development headers and library
- OpenSSL development headers and Crypto library
- fmt development headers and library
- `rustup` and `cargo`; the pinned `xslang/rust-toolchain.toml` toolchain must be installed
- JRE 25, Gradle 9.6.1 or newer, and the Kotlin 2.4.0 `kotlin` script runner for the `jvm` project-test label

Clone the repository with submodules, or initialize them before configuring:

```text
git submodule update --init --recursive
```

The pinned DIMCLI source under `third_party/dimcli` implements the C++23 Preview command schema and generated help for `xs`.
It is built directly by the project. Tests prefer a system Catch2 3 package and fall back to the pinned
`third_party/catch2` source when the supported system does not provide one.

Useful helper tools:

- `fd`
- `rg`
- `bat -p`
- `sd`
- `busybox wc`
- `tokei`
- `uutils-coreutils` tools

## Default build

```text
cmake --preset clang-debug
cmake --build --preset clang-debug
ctest --preset clang-debug --output-on-failure
```

Preset details:

- generator: Ninja
- compilers: Clang for strict C23 and Clang++ for C++23 Preview
- shared monorepo and LLVM toolchain policy: the root `CMakeLists.txt`
- build directory: `build/clang-debug`
- default project: `xs`

The root build selects and validates Clang/Clang++, LLVM archive/object utilities, LLD, strict C23, C++23 Preview, and Ninja before project
targets are configured. The root build keeps project definitions separate from this tool selection. Test registration is
likewise split by direct XLIL, source values/control flow/calls, Kotlin projects, and library-level suites under
`cmake/XSTests*.cmake`.

The `xs` target builds `/usr/bin/xs` package payload code and the Rust `xslang` static library. Its C++23 Preview executable
entry and DIMCLI argument layer dispatch into the existing C23 compiler driver while subsystem migration continues.
Monomorphization and codegen-unit planning are C++23 Preview-owned subsystems exposed through stable C entry points, so the
remaining C23 driver can consume them without a simultaneous frontend rewrite. C++ consumers may use the move-only
`<xs/mono/Plan.hpp>` and `<xs/codegen/Plan.hpp>` views.

## Compiler installation layout

Install the compiler component into a staging prefix with:

```text
cmake --install build/clang-debug --prefix /tmp/xs-root --component compiler
```

With the normal system prefix `/usr`, this component installs the native compiler command as `/usr/bin/xs`, merges the common
`include/xs/` and compiler-owned `xs/include/xs/` public C23 headers under `/usr/include/xs/`, and installs
`LICENSE.txt` plus `NOTICE.txt` under `/usr/share/licenses/xs/`. Source-tree ownership remains separate even though the
installed include surface is unified. CMake fails rather than silently replacing an identically named header from the two
source trees.

The mandatory Kotlin project runtime included in the `xs` package is built with Gradle:

```text
./xs_kts/gradlew --daemon --build-cache -p xs_kts test installDist
```

It targets JVM 25 and runs on JRE 25 or newer. Runtime project evaluation also requires an external `kotlin` command with
scripting support; neither is embedded. The native executable does not link a JVM, but JRE 25 and Kotlin are mandatory
runtime dependencies of the unified `xs` package.

## OOM-safe workflow

Parser/compiler tests have previously triggered OOM conditions. Use a 2GB virtual memory limit for native tests, excluding
the JVM-labelled Kotlin project integration tests:

```text
cmake --build --preset clang-debug --target xs_project_runtime
ulimit -v 2097152
cmake --build --preset clang-debug
ctest --preset clang-debug --output-on-failure -LE jvm -FA kotlin_project_resolver
```

Then run the real `xs.project.kts` integration tests outside that virtual-address-space limit. A JVM reserves more virtual
address space than its live heap, so applying `ulimit -v 2097152` to this step is not valid:

```text
ctest --preset clang-debug --output-on-failure -L jvm
```

Tests are expected to run quickly. If a test suddenly consumes a lot of memory, treat it as a possible infinite loop, parser
progress bug, or runaway macro expansion.

## Sanitizer build

AddressSanitizer and UndefinedBehaviorSanitizer are available through the separate Clang preset:

```text
cmake --preset clang-sanitize
cmake --build --preset clang-sanitize
ctest --preset clang-sanitize --output-on-failure
```

Do not apply the 2GB virtual-memory limit to AddressSanitizer runs: its required shadow-memory reservation exceeds that
limit even when the program's real memory use is small.

## Project selection

Stable projects:

```text
cmake --preset clang-debug -DXS_ENABLE_PROJECTS=xs
cmake --preset clang-debug -DXS_ENABLE_PROJECTS=all
```

Future projects (`xsfmt`, `xstidy`, and `xs-analyzer`) intentionally produce CMake errors for now.

## Useful checks

```text
git diff --check
rg -n "\bNULL\b|#include <stdbool\.h>" xs xsrt tests include
busybox wc -l <file.c> <file.h>
```

Files must not exceed 1000 lines. New or touched C code should use `nullptr` and C23 `bool`.

## Build outputs

`build/` is a generated area. It may look dirty after build/test runs and should not be included in normal commits.
