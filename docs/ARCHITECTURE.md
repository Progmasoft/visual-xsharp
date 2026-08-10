<!--
SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
SPDX-License-Identifier: MPL-2.0
-->

# Compiler architecture

Visual X# uses a staged, target-independent compiler pipeline. The frontend through CorePrep is implemented in Haskell;
Xpp, Xmm, and the native backend boundary are implemented in C++20. Rust components remain available during the migration
and continue to own the compiler services already assigned to them.

## Pipeline

```text
.vxs source
    → Visual.XSharp.Lexer
    → Visual.XSharp.Parser
    → Visual.XSharp.AST.Parsed
    → Visual.XSharp.Resolver.Renamer
    → Visual.XSharp.Resolver.NameResolution
    → Visual.XSharp.AST.Resolved
    → Visual.XSharp.TypeChecker
    → Visual.XSharp.AST.Typed
    → Visual.XSharp.Desugarer
    → Visual.XSharp.Core
    → Core optimizations
    → Visual.XSharp.Core.CorePrep
    → Xpp
    → Xpp optimizations
    → Xmm
    → Xmm optimizations
    → LLVM bitcode or VPI
```

Core, CorePrep, Xpp, and Xmm are independent of LLVM. Intermediate representations remain binary and in memory unless an
explicit `-Emit` option requests a file. Their explicit artifact suffixes are `.core`, `.xpp`, and `.xmm`.

## Frontend ownership

The Haskell frontend owns source decoding, tokenization, parsing, name resolution, type checking, desugaring, Core, and
CorePrep. Its public module boundaries deliberately keep the major compiler responsibilities separate:

- `Visual.XSharp.Lexer` produces tokens with source spans and diagnostics.
- `Visual.XSharp.Parser` produces only parsed syntax; it does not resolve names or types.
- `Visual.XSharp.AST.Parsed`, `.Resolved`, and `.Typed` are distinct models.
- `Visual.XSharp.Resolver.Renamer` assigns stable identities without deciding type semantics.
- `Visual.XSharp.Resolver.NameResolution` resolves namespaces, imports, declarations, and references.
- `Visual.XSharp.TypeChecker` records checked types, overload choices, and required conversions.
- `Visual.XSharp.Desugarer` removes surface syntax before Core is formed.
- `Visual.XSharp.Core.CorePrep` prepares target-independent Core for the native middle end.

Diagnostics retain source ownership throughout these stages. A later stage must not reconstruct information discarded by
an earlier stage or silently invent semantics for an unsupported language form.

## Native middle end

The C++20 middle end receives verified CorePrep data through an explicit owned boundary.

- Xpp preserves high-level operations needed for whole-program planning and optimization.
- Xmm is the lower, target-independent machine model consumed by native backends.
- Xpp and Xmm each have their own verifier and optimization pipeline.
- Readers, writers, verifiers, optimizers, lowering code, and tests change together when an IR contract changes.

The compiler does not use serialized intermediate files to pass ordinary builds between stages. Serialization is a tooling
and debugging feature selected with `-Emit`.

## Backend boundary

LLVM is a backend, not a frontend dependency. LLVM context, target machine, data layout, module construction, verification,
optimization, object emission, and LLD invocation remain behind the backend boundary. VPI is the alternative output boundary
for Visual Plataforma integration.

The backend never accepts parsed or typed AST nodes directly. It receives verified Xmm and reports failures through the
compiler diagnostic model.

## Entry point

A project names the namespace and class containing its entry point. An executable entry point is a class member such as
`public static void Main()`; top-level functions are not entry points. File names may help diagnostics and project discovery,
but they do not define language semantics.

## Migration boundary

The current tree contains a tested compatibility implementation while the renewed pipeline is connected. That code is a
temporary bridge, not the architecture for new compiler features. Migration follows stage ownership:

1. establish and test the Haskell frontend through CorePrep;
2. connect the owned CorePrep-to-Xpp boundary;
3. complete the C++20 Xpp/Xmm and backend route;
4. remove the replaced C compiler subsystems instead of translating obsolete designs line by line;
5. retain only intentional C ABI/runtime surfaces, each isolated behind a public boundary.

New language behavior belongs in the renewed pipeline. Compatibility code may receive correctness and migration fixes, but
it must not become a second permanent frontend.
