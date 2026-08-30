<!-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com> -->
<!-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0 -->

# Contributing

## Preserve architectural ownership

- Add frontend behavior through CorePrep in Haskell.
- Add Xpp/Xmm and native middle-end behavior in C++20.
- Keep LLVM details inside the backend.
- Adapt useful behavior from the retiring Rust tree only into its owning Haskell or C++20 layer; do not add new Rust
  compiler behavior.
- Reduce C23 ownership only after replacement behavior is verified.
- Do not add new language behavior to transitional C23 frontend layers.

## Language and API changes

- Use the `.vxs` source extension and `.vxse` executable extension.
- Do not introduce top-level runtime functions as entry points.
- Do not change public API names incidentally during implementation work.
- Fix spelling errors wherever they occur, including API spellings.
- Keep public documentation in en-US English.
- Do not expose internal planning notes in public files.

## C and C++ files

- C-only headers use `.h`.
- Headers shared by C and C++ use `.hh`.
- C++-only headers use `.hpp`.
- C++ implementation files use `.cpp` and compile as C++20.
- C++ namespaces, classes, and functions use PascalCase. The canonical C++ root is `Visual::XSharp`; legacy Rust naming
  must not be introduced as the target namespace for renewed C++ code.
- C++ local variables use camelCase, constants use `kPascalCase`, and macros use `UPPER_SNAKE_CASE`.
- New or retained C implementation files compile as strict C23 until migrated.

The checked-in `.clang-format` profile is the mechanical C++20 style authority and requires LLVM/Clang 23.1.0. It uses
four-space Allman layout, return types on their own line, system includes before project includes, indented namespace
bodies, leading continuation operators, west-const spelling, right-aligned pointer/reference declarators, and no invented
column limit. Run it only on project-owned C/C++ files; never reformat `third_party/` or generated sources.

The style profile does not replace the Visual X# naming contract above. In particular, do not rename existing functions
to camelCase or introduce storage/pointer prefixes from an external style guide. Public API naming changes remain explicit
architecture decisions rather than formatting work.

For design choices not expressible mechanically, prefer include-what-you-use, value/RAII ownership, explicit structured
runtime errors, early returns, references for required observers, pointers for nullable observers, and synchronization
owned by the state it protects. Exceptions must not cross stable ABI or C boundaries.

## Size and decomposition

Implementation, test, build, configuration, and internal source files must not exceed 1500 lines. Split files by responsibility
before they reach the limit. Public topic-oriented `Spec/` suites are exempt.

## Verification

Run the checks appropriate to the changed ownership boundary:

- Haskell: `cabal build all`, `cabal test all`, and `cabal check`.
- Kotlin: `ProjectSystem\gradlew.bat -p ProjectSystem test`.
- Native: Bazel `vxs` build plus the Windows-native Catch2 contract executable.
- C/C++ style: LLVM 23.1.0 `clang-format --dry-run --Werror` over project-owned `.cpp`, `.hpp`, `.hh`, and `.h` files.
- Documentation: link scan, spelling scan, and `git diff --check`.

Initialize recursive submodules before native configuration. Do not store machine-specific LLVM or toolchain paths in tracked
configuration.

## Commits

Use the repository Java helper for normal updates:

```powershell
java --source=21 scripts\java\git.java update "Detailed change description"
```

The helper excludes generated and local-only paths, applies recursive submodule file-mode hygiene, commits, and pushes the
current branch with force-with-lease.
