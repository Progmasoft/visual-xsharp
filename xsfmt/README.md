<!--
SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
SPDX-License-Identifier: MPL-2.0
-->

# xsfmt

Experimental Visual X# formatter project.

`xsfmt` is intentionally isolated from the compiler implementation and uses Rust
nightly with Serde-backed TOML configuration.

User configuration for `xsfmt` is standardized on TOML.

The configuration model currently validates `max_width` (40–320),
`indent_width` (1–16), `use_tabs`, and `newline_style` (`auto`, `lf`, or
`cr_lf`). The first formatting pass normalizes line endings, trailing whitespace,
excess final blank lines, and the final newline without changing Visual X# syntax.

```text
cargo +nightly run --manifest-path xsfmt/Cargo.toml -- --check Main.vxs
cargo +nightly run --manifest-path xsfmt/Cargo.toml -- Main.vxs
```

Rust sources live under `xsfmt/sources/`.
