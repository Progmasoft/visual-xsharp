<!--
SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
SPDX-License-Identifier: MPL-2.0
-->

# Backend architecture and roadmap

LLVM is the only implemented and supported Visual X# backend today. The compiler is intentionally not defined as an LLVM-only
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
execution integration. It is not responsible for redefining Visual X# typing, ownership, overload resolution, macro expansion,
or control-flow semantics.

New XLIL instructions must update the model, text parser/writer, verifier, lowering, optimizer, and tests before a backend
uses them. Backend-specific extensions should not leak into the common XLIL v1 grammar merely to simplify one target.

## LLVM backend

LLVM remains the production path. It lowers verified XLIL into LLVM IR, applies the configured LLVM optimization pipeline,
emits object files, and links native `.vxse` executables on supported host targets. Its implementation and current
limitations are documented in [LLVM_BACKEND.md](LLVM_BACKEND.md).

## Planned C23 backend

The C23 backend is planned as `xslang/c23_backend/Cargo.toml` inside the Rust workspace. It will emit portable C23 and use
Clang to produce native artifacts. Generated C must preserve Visual X# destruction, ownership, and error behavior; it is an
implementation artifact, not a second source-level language contract.

## Planned JavaScript backend

The JavaScript backend is planned as `xslang/js_backend/Cargo.toml`. Its host runtime will use `deno_core`, V8, and Tokio.
`xs run` will execute through that embedded runtime and will not require Node.js.

Visual X# language semantics remain unchanged. JavaScript execution uses V8 garbage collection, but the backend may not expose
host-GC timing as new Visual X# semantics.

## Planned X Platform backend

X Platform is a separate, language-neutral runtime project. Its common pipeline is `XPI → xpic → XPLR register
bytecode`; Visual X# may reach it through `XLIL → XPI`, while other languages can emit XPI directly. XPI is intentionally lower
level than XLIL and does not contain Visual X# nominal types, string rules, ownership semantics, or source constructs.

The X Platform Garbage Collector (XPG) belongs to that runtime and is not an alternate mode inside `xslang`. The future
X Platform JIT (XPJ) is planned as four optimization levels over two code generations. Neither bytecode emission nor XPJ
is implemented by the current Visual X# compiler. Memory management follows backend selection without a separate flag: LLVM
always uses RAII and XPLR always uses XPG.

The emitted module format, deployment artifact layout, JavaScript interop ABI, and asynchronous host boundary remain
undecided and will be specified before implementation.

## Planned WebAssembly backend

The WebAssembly backend is planned as `xslang/wasm_backend/Cargo.toml`, with Wasmtime embedded for local execution.
Language-visible memory-management behavior is intended to match the LLVM backend and the selected Visual X# runtime mode.

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
