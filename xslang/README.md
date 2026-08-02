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

This crate is pre-1.0 compiler infrastructure. Its APIs and version-0 intermediate formats may evolve together with the X#
compiler. The repository pins a Rust nightly toolchain for reproducible development and validation.

Documentation and source are available in the [xs-project repository](https://github.com/xss-lang/xs-project).
