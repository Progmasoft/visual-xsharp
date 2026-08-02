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

### Typed producer facade

`xslang::xlil::typed::TypedBuilder` is a checked facade over the same canonical XLIL model. It pairs register and slot
identifiers with the explicit markers in `xslang::xlil::types`, catching common signature mistakes before whole-module
verification:

```rust
use xslang::xlil::{IntegerBinaryOperation, module_to_string};
use xslang::xlil::typed::{Signature, TypedBuilder};

let mut builder = TypedBuilder::new("Example");
builder.begin(
    Signature::returning::<i64>("sum")
        .parameter::<i64>()
        .parameter::<i64>(),
)?;
builder.append_block("entry")?;

let left = builder.parameter::<i64>(0)?;
let right = builder.parameter::<i64>(1)?;
let result = builder.integer(IntegerBinaryOperation::Add, left, right)?;
builder.return_value(result)?;

let module = builder.finish()?;
println!("{}", module_to_string(&module));
```

Typed values can be erased into `AnyValue` for heterogeneous argument lists and checked back with `downcast`. The facade
also covers checked calls, boolean and conditional control flow, typed stack slots, exact-width constants, `f32`/`f64`
operations, and UTF-32 comparisons. `TypedBuilder::raw` and `TypedBuilder::into_raw` keep specialized producers able to
use lower-level records without creating a second XLIL representation.

`F16` and `F128` are exact bit containers. Their signatures and pass-through values are supported, but arithmetic stays
explicitly deferred until the corresponding XLIL operation and backend coverage exists.

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

`xlil_create` preserves the attributed Rust function and adds verified module and canonical-text companions named
`<function>_xlil` and `<function>_xlil_text`:

```rust
#[xslang::xlil_create]
fn max(a: i64, b: i64) -> i64 {
    if a > b { a } else { b }
}

let module = max_xlil()?;
let text = max_xlil_text()?;
```

The 0.2.6 subset lowers immutable local bindings, `bool`, explicit numeric aliases and bit containers, integer and
`f32`/`f64` operations, unary negation, result-producing `if/else`, and short-circuit boolean control flow. Attribute
options can set the XLIL module and companion function names. `Utf32Builder` converts Rust text to numeric UTF-32 code
points without retaining the source text in XLIL. Unsupported Rust constructs are rejected during macro expansion instead
of producing incomplete XLIL. See the [Rust XLIL API guide](../docs/XLIL_RUST.md) for the complete supported subset.

This crate is pre-1.0 compiler infrastructure. Its APIs and version-0 intermediate formats may evolve together with the X#
compiler. The repository pins a Rust nightly toolchain for reproducible development and validation.

Documentation and source are available in the [xs-project repository](https://github.com/xss-lang/xs-project).
