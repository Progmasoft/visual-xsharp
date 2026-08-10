<!--
SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
SPDX-License-Identifier: MPL-2.0
-->

# LLVM backend

The LLVM backend is separate from the Haskell frontend and the target-independent C++20 middle end. Its long-term input is a
verified Xmm module.

## Responsibilities

- create and destroy LLVM contexts and modules;
- select the target triple, target machine, CPU features, and data layout;
- lower Xmm types, constants, functions, control flow, and operations;
- verify generated LLVM IR before optimization and emission;
- run the selected LLVM optimization pipeline;
- emit LLVM assembly, LLVM bitcode, native assembly, and object files;
- invoke LLD for native `.vxse` artifacts;
- translate LLVM and linker failures into compiler diagnostics.

## Boundary rules

- AST, Core, Xpp, and Xmm models contain no LLVM handles.
- The backend does not parse Visual X# source or resolve language names.
- Xmm verification completes before LLVM module construction.
- Target layout decisions are made in the backend, not encoded into frontend types.
- Backend failure never causes a fallback to a retired compiler stage.

## Optimization

The CLI selects the LLVM optimization level independently from Xpp and Xmm optimization. Xmm optimization is enabled by
default; LLVM optimization runs only after valid LLVM IR has been produced. Verification runs at the boundary so malformed
lowering is reported before object emission.

## Native artifacts

The native route is:

```text
verified Xmm → LLVM IR → LLVM verification/optimization → object → LLD → .vxse
```

Runtime libraries and required DLLs are copied beside the produced executable as part of the artifact step. Windows debug
and AddressSanitizer configurations use the runtime matching the selected ClangCL toolchain.

## Completion criteria

The backend is complete for a language feature only when Xmm lowering, LLVM verification, object emission, linking, execution,
and negative diagnostics are covered. A data-model declaration without native-route tests is not backend completion.
