<!-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com> -->
<!-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1 -->

# Find the owning component

The public `vxs` command is one compiler, but implementation ownership is split deliberately. Put a rule where it first
becomes meaningful, then test the next boundary that consumes it. Do not add a second parser, semantic model, or option
validator in a convenient downstream component merely because that is where a symptom appears.

## Compiler route

```text
.vxs source
  -> Haskell Syntax and Frontend (lexer, parser, resolver, type checker, desugarer)
  -> Haskell Core, optimization, and CorePrep
  -> C++20 Core reader/adapter, Xpp, and Xmm
  -> C++20 LLVM backend, linker, and native artifact
```

The frontend's current process boundary uses a verified private Core transport. CorePrep adapts Core for native lowering;
it is not a public `-Emit` format. User-visible artifact formats include `.core`, `.xpp`, `.xmm`, object, assembly, LLVM IR,
and `.vxse` where the connected CLI supports them. [Pipeline](../COMPILER-PIPELINE.md) and [artifact wire](../ARTIFACT-WIRE.md)
explain the contracts and implementation limits.

| Change | Primary owner | Typical validation |
| --- | --- | --- |
| Token, grammar, source span, parse recovery | `Compiler/Haskell/Syntax` or `Compiler/Haskell/Frontend` | Package-local Haskell tests, then `cabal test all` |
| Name binding, type rule, desugaring | `Compiler/Haskell/Frontend` | Positive/negative frontend tests and Core-shape tests |
| Typed Core, optimization, private transport | `Compiler/Haskell/Core`; native reader in `Compiler/Core` | Independent codec, verifier, malformed-input, and golden tests |
| Xpp/Xmm operation or ownership check | `Compiler/Codegen/Xpp`, `Compiler/Codegen/Xmm`, or `Compiler/Analysis` | Component tests plus connected `Compiler/Driver/Tests` |
| LLVM lowering or object emission | `Compiler/Backend/LLVM` | Backend tests and native artifact smoke checks |
| AARC runtime/ABI | `Compiler/Runtime/AARC` and owned public headers | C++ runtime tests and C11 caller ABI test |
| Native CLI spelling, precedence, help, dispatch | `Compiler/Cli` | CLI tests and a `vxs` process test |
| Project file evaluation, plugins, lockfile, VXDC | `ProjectSystem`; transport in `Compiler/ProjectSystem/Bridge` | Kotlin tests and native bridge tests |
| REPL cell/session behavior | `Interactive` | `Interactive/Tests` and a `vxsi` smoke check |

Compiler-owned C++ public headers live under `Compiler/Headers/Visual/XSharp/`; callers include them as
`Visual/XSharp/...`. Haskell module names and the `Visual::XSharp` C++ namespace are API boundaries, not invitations to
rename a subsystem while fixing an unrelated bug. The current layout and allowed dependency direction are in the
[repository map](../MONOREPO.md).

## Language design versus implementation

`Spec/` is the public source for intended Visual X# syntax and semantics. Its `.vxs` topic files contain independent
valid and invalid examples; they are not one compilable project. A change to language meaning starts with the affected
`Spec/` topic and discusses compatibility, diagnostics, and examples. It then needs an owning test and implementation at
the appropriate compiler stage. Conversely, an implementation bug should not be "fixed" by weakening the specification
just to match current behavior.

Use [implementation status](../IMPLEMENTATION.md) to distinguish connected, implemented, registered, planned, and legacy
features. If a proposal needs a new public name or artifact format, say so explicitly in the issue and PR. Do not create
a public CorePrep output or resurrect retired C/Rust compiler routes.

## Ecosystem and other repositories

`Analyzer/`, `Formatter/`, and `Linter/` own independent Haskell and Kotlin components with their own validation gates.
They consume compiler semantics but do not redefine the language. Their package-local tests and CI workflows should be
updated when their behavior changes. [Ecosystem tools](../ECOSYSTEM.md) records their connected and planned boundaries.

The Visual X# website (`Progmasoft/website`), Progmasoft account and ViGet service (`Progmasoft/progmaweb`), Xide
(`Progmasoft/xide`), and Catch3 (`Progmasoft/catch3`) are separate Git repositories. A compiler change may require a
follow-up in one of them, but a Visual X# root commit cannot silently include their nested working trees. Open separate
issues or PRs and link them when a cross-repository change is required.
