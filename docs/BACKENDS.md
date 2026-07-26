<!--
SPDX-FileCopyrightText: 2026 Leitwolf <xs-lang.chess031@slmails.com>
SPDX-License-Identifier: MPL-2.0
-->

# Backend architecture and roadmap

LLVM is the only implemented and supported X# backend today. The compiler is intentionally not defined as an LLVM-only
language: typed HIR, MIR, and XLIL do not contain LLVM handles or require the LLVM C API. Future backends must consume the
same verified, target-independent compiler state instead of bypassing semantic analysis.

## Status

| Backend | Project value | Status | Intended implementation |
| --- | --- | --- | --- |
| LLVM | `LLVM` | Implemented | Existing C23 XLIL-to-LLVM backend |
| C23 | `C` | Planned | Rust crate under the `xslang` workspace |
| JavaScript | `JavaScript` | Planned | Rust crate using `deno_core` and Tokio |
| WebAssembly | `WebAssembly` | Planned | Rust crate with embedded Wasmtime execution |

The planned selectors are:

```kotlin
set("XS_BACKEND", "C")
set("XS_BACKEND", "JavaScript")
set("XS_BACKEND", "WebAssembly")
```

and a future command-line override:

```text
--xs-backend C
--xs-backend JavaScript
--xs-backend WebAssembly
```

These non-LLVM values document the intended interface; they do not enable a backend in the current release. Backend
selection must fail clearly when the requested implementation is unavailable. It must never silently fall back to LLVM.

## Shared backend contract

Every native or hosted backend starts after the common language pipeline:

```text
source → AST → macro expansion → typed HIR → MIR → verification
       → borrow checking → optimization → monomorphization
       → codegen-unit planning → verified XLIL → selected backend
```

A backend is responsible for target representation, target-specific lowering, artifact emission, runtime binding, and
execution integration. It is not responsible for redefining X# typing, ownership, overload resolution, macro expansion,
or control-flow semantics.

New XLIL instructions must update the model, text parser/writer, verifier, lowering, optimizer, and tests before a backend
uses them. Backend-specific extensions should not leak into the common XLIL v0 grammar merely to simplify one target.

## LLVM backend

LLVM remains the production path. It lowers verified XLIL into LLVM IR, applies the configured LLVM optimization pipeline,
emits object files, and links native `.xse` executables on supported host targets. Its implementation and current
limitations are documented in [LLVM_BACKEND.md](LLVM_BACKEND.md).

## Planned C23 backend

The C23 backend is planned as `xslang/c23_backend/Cargo.toml` inside the Rust workspace. It will emit portable C23 and use
Clang to produce native artifacts.

Memory-management lowering depends on the project XGC mode:

- with XGC enabled, generated C23 binds to the public XGC runtime API;
- with XGC disabled, generated C23 emits deterministic, RAII-equivalent cleanup through explicit C23 lifetime and cleanup
  operations.

The generated support surface must expose the runtime operations required by the selected mode. This backend must preserve
X# destruction, finalization, ownership, and error behavior; generated C is an implementation artifact, not a second
source-level language contract.

## Planned JavaScript backend

The JavaScript backend is planned as `xslang/js_backend/Cargo.toml`. Its host runtime will use `deno_core`, V8, and Tokio.
`xs run` will execute through that embedded runtime and will not require Node.js.

X# language semantics remain unchanged. JavaScript execution uses V8 garbage collection rather than XGC. Finalizer and
`defer` behavior therefore requires an explicit lowering/runtime contract comparable to the guarantees provided by the
managed XGC mode; the backend may not expose host-GC timing as new X# semantics.

The emitted module format, deployment artifact layout, JavaScript interop ABI, and asynchronous host boundary remain
undecided and will be specified before implementation.

## Planned WebAssembly backend

The WebAssembly backend is planned as `xslang/wasm_backend/Cargo.toml`, with Wasmtime embedded for local execution.
Language-visible memory-management behavior is intended to match the LLVM backend and the selected X# runtime mode.

The WebAssembly target must define its runtime imports, module/component choice, WASI policy, linking model, and final
artifact naming before it becomes supported. `xs run` integration must use the bundled Wasmtime path rather than depending
on an unspecified system runner.

## Implementation gates

A planned backend becomes supported only when it has:

- a registered and validated backend selector;
- verified XLIL input and explicit unsupported-instruction diagnostics;
- deterministic artifact naming and lifecycle;
- runtime and ABI documentation;
- build, run, invalid-input, and cross-target tests;
- packaging and license metadata;
- no silent fallback to another backend.

Until those gates are met, public release notes must describe C23, JavaScript, and WebAssembly as planned backends.
