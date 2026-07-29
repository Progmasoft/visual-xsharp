<!--
SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
SPDX-License-Identifier: MPL-2.0
-->

# xstidy

Experimental X# linter project.

`xstidy` is intentionally isolated from the compiler implementation and uses
Rust nightly with Serde-backed diagnostics and TOML configuration.

User configuration for `xstidy` is standardized on TOML.

The first syntax rule, `XT0001`, reports constructor-shaped `Type::new(...)`
calls and points users to canonical `new Type(...)` syntax. Comments and string
literals are ignored. `--json` emits one Serde JSON diagnostic per line.

```text
cargo +nightly run --manifest-path xstidy/Cargo.toml -- Main.xs
cargo +nightly run --manifest-path xstidy/Cargo.toml -- --json Main.xs
```

Rust sources live under `xstidy/sources/`.
