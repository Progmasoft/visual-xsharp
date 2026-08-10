<!--
SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
SPDX-License-Identifier: MPL-2.0
-->

# Public roadmap

## Haskell frontend through CorePrep

- Complete the source/span and diagnostic infrastructure shared by all frontend passes.
- Complete `Visual.XSharp.Lexer` and `Visual.XSharp.Parser` for the current public language specification.
- Complete the distinct parsed, resolved, and typed AST models.
- Complete renaming and namespace/member name resolution.
- Complete type checking, overload selection, conversions, and generic validation.
- Complete desugaring, Core construction, Core optimizations, and CorePrep.
- Preserve class-based `public static void Main()` entry semantics through CorePrep.

## C++20 middle end

- Finalize the owned CorePrep-to-Xpp interface.
- Complete Xpp construction, verification, optimization, and explicit `.xpp` emission.
- Complete Xmm lowering, verification, default optimization, and explicit `.xmm` emission.
- Keep Core, Xpp, and Xmm target independent and free of LLVM handles.
- Connect verified Xmm to LLVM bitcode and VPI outputs.

## C retirement

- Delete compatibility lexer/parser, semantic, and intermediate compiler subsystems after their Haskell/C++ replacements
  pass the relevant fixtures.
- Move remaining compiler-owned driver, package, and backend implementation to C++20 where appropriate.
- Keep only small, intentional C ABI/runtime surfaces after implementation ownership has moved.
- Convert or replace C tests as their owning subsystem migrates.

## CLI and project system

- Keep the CLI and `Visual.XSharp.kts` on one configuration model without retired aliases or compatibility setters.
- Complete `-Emit core|xpp|xmm` artifact handling while keeping intermediate data in memory by default.
- Keep Xmm optimization enabled by default for direct-file and project builds.
- Complete package resolution, lockfile, update, publication, and yank workflows.

## Backend and runtime

- Complete Xmm-to-LLVM lowering for the full supported language surface.
- Complete object, assembly, LLVM bitcode, VPI, and native `.vxse` artifact routes.
- Specify ownership, unwinding, strings, collections, async, and FFI at the runtime boundary.
- Advertise additional backends only after their verifier, artifact contract, diagnostics, and CI gates exist.
