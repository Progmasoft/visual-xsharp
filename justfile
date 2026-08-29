# SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
# SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

# `just` is the repository-level command surface. CMake continues to own native
# targets and Gradle continues to own the Kotlin DSL; recipes only compose them.
set shell := ["powershell.exe", "-NoLogo", "-NoProfile", "-Command"]

default:
    just --list

# Configure the selected ClangCL preset (debug or sanitize).
configure profile="debug":
    cmake --preset "clangcl-{{profile}}"

# Build every native target after configuration.
build profile="debug": (configure profile)
    cmake --build --preset "clangcl-{{profile}}"

# Run the native CTest suite for the selected preset.
test-native profile="debug": (build profile)
    ctest --preset "clangcl-{{profile}}" --output-on-failure --parallel 2

# Format and verify the programmable Kotlin project DSL.
check-kotlin:
    .\vxs_kts\gradlew.bat -p vxs_kts check

# Match the independently owned native and Kotlin verification layers used by CI.
check profile="debug": (test-native profile) check-kotlin

# Apply Kotlin DSL formatting without changing native files implicitly.
format-kotlin:
    .\vxs_kts\gradlew.bat -p vxs_kts spotlessApply

# Format only paths deliberately supplied by the caller.
format-native *files:
    clang-format -i {{files}}
