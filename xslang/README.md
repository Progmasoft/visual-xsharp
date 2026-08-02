<!--
SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
SPDX-License-Identifier: MPL-2.0
-->

# xslang

`xslang` is the target-independent Rust compiler core for the X# programming language. It provides typed HIR, MIR,
monomorphization and codegen-unit models, and the human-readable XLIL v1 registry.

The public `xslang::xlil` API lets Rust tools and third-party language implementations construct, parse, verify, optimize,
and write XLIL without depending on LLVM. The corresponding C23 API is maintained in the xs-project repository under
`<xs/lil.h>` and `<xs/lil-c/*.h>`.

`xslang::xlil::Builder` provides an insertion-point API for declarations, definitions, values, calls, storage, and
control flow. Checked calls derive their signature from the module registry instead of requiring a repeated return type.
`Builder::finish` verifies the completed module before returning it; lower-level public model types remain available for
readers and specialized producers.

`parse_verified` and `VerifiedModule` provide an explicit verification boundary. Stream-oriented `read_module`,
`read_verified`, `write_module_io`, and `write_verified` helpers integrate XLIL with ordinary Rust I/O, while inspection
methods expose functions, values, blocks, successors, instruction opcodes, and result registers without LLVM.

Rust applications may optionally use `xslang::rust::XSResult<T>` and `xslang::rust::XSError` as a thread-safe boxed
error boundary. XLIL's structured parse, verification, build, and I/O errors remain available and do not require these
aliases.

The optional `xslang::printf!` macro is implemented by the `xslang::rust` support module and exported at the crate root.
It accepts C-style conversion specifiers and writes through a safe, synchronized standard-output path;
`xslang::rust::printf!` is intentionally not a valid path.

## Procedural XLIL producers

Enable the optional procedural-macro workspace through the main crate:

```toml
[dependencies]
xslang = { version = "0.2.6", features = ["proc-macros"] }
```

`xlil_create` preserves the attributed Rust function and adds a companion producer named `<function>_xlil`:

```rust
#[xslang::xlil_create]
fn max(a: i64, b: i64) -> i64 {
    if a > b { a } else { b }
}

let module = max_xlil()?;
```

The initial 0.2.6 subset lowers `bool`, the explicit numeric aliases and bit containers in `xslang::xlil::types`, integer
arithmetic and comparisons, and result-producing `if/else` control flow. `Utf32Builder` converts Rust text to numeric
UTF-32 code points without retaining the source text in XLIL. Unsupported Rust constructs are rejected during macro
expansion instead of producing incomplete XLIL.

This crate is pre-1.0 compiler infrastructure. Its APIs and version-0 intermediate formats may evolve together with the X#
compiler. The repository pins a Rust nightly toolchain for reproducible development and validation.

Documentation and source are available in the [xs-project repository](https://github.com/xss-lang/xs-project).
