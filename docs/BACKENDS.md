<!--
SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
SPDX-License-Identifier: MPL-2.0
-->

# Backend architecture

Every backend starts after the common target-independent pipeline:

```text
Haskell frontend → CorePrep → Xpp → Xmm → backend
```

LLVM is the production native backend. VPI is the Visual Plataforma integration boundary. Other backend ideas are not
advertised as supported until they have a complete artifact contract and CI coverage.

## LLVM

The LLVM backend consumes verified Xmm and owns LLVM context, module, target machine, data layout, optimization, object
emission, and LLD linking. It may emit LLVM assembly, LLVM bitcode, object files, native assembly, or a `.vxse` executable as
selected by the CLI.

LLVM types and handles do not leak into Core, Xpp, or Xmm. Backend diagnostics are translated into the shared compiler
diagnostic model and retain the responsible source or generated unit where available.

## VPI

VPI is the alternative output of the common compiler pipeline for Visual Plataforma. It is not an alias for LLVM IR and does
not change frontend language semantics. Its verifier, serialization, runtime contract, and execution integration must be
complete before it is presented as a production backend.

## Support gates

A backend becomes public only when it has:

- a documented input and artifact contract;
- validation before code generation;
- complete lowering for the advertised language subset;
- deterministic diagnostics;
- object/runtime/link behavior where applicable;
- direct and project-mode CLI integration;
- positive and negative tests in CI.
