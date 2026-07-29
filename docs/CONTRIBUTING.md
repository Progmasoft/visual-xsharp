<!--
SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
SPDX-License-Identifier: MPL-2.0
-->

# Contribution and workflow rules

This repository is compiler infrastructure. Prefer small, testable patches that preserve architecture boundaries.

## Core rules

- C23 is the primary implementation language.
- Existing C23 frontend and backend layers stay in C. New semantic compiler-core work belongs in the isolated Rust `xslang`
  crate and crosses the C boundary through bulk, versioned APIs.
- Shell scripts are not added as persistent project artifacts.
- Use CMake 3.31 or newer; do not use Meson.
- Stay within the documented Clang/LLVM build path and avoid assumptions that are not covered by the supported toolchain.
- Files must not exceed 1000 lines. New or touched C and header files should stay much smaller when practical.
- In new/touched C code, use `nullptr` instead of `NULL`, and C23 `bool` instead of `<stdbool.h>`.

## Patch size

Do not try to finish an entire compiler stage in one huge patch. Preferred shape:

1. add the data model
2. connect parser/HIR/MIR
3. add focused tests
4. update documentation
5. pass build/test

## Tests

Every change should at least pass the touched subsystem’s tests. Run the full test suite when practical:

```text
ulimit -v 2097152
cmake --build --preset clang-debug
ctest --preset clang-debug --output-on-failure
```

For docs-only changes, a build is not required, but `git diff --check` should be run.

Run both language-specific static analyzers before submitting compiler changes:

```text
cargo +nightly-2026-07-10 clippy --manifest-path xslang/Cargo.toml --all-targets -- -D warnings
run-clang-tidy -j 1 -p build/clang-debug -warnings-as-errors='*'
```

The Rust crate treats the stable `clippy::all` group as its warning contract. The opt-in `pedantic` and `nursery`
suggestion collections are disabled globally; suitable checks from those groups should be enabled individually instead
of imposing conflicting style heuristics on all existing IR code.

## Documentation

If code behavior changes, the relevant documentation changes too. Every user-visible change is summarized in the root
`CHANGELOG.md`.

## Commits

Do not commit generated artifacts:

- `build/`
- `target/`
- `node_modules/`
- `dist/`
- `out/`

Preserve user changes; do not revert unrelated files.
