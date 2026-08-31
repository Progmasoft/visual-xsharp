<!-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com> -->
<!-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0 -->

# LLVM backend contract

## Boundary

The LLVM backend consumes verified Xmm. It does not accept source syntax, AST, Core, CorePrep, or Xpp, and it does not infer
missing semantic information. Xmm therefore retains every item required for deterministic lowering:

- stable function symbols and spellings;
- parameter registers and parameter types;
- function result types;
- instruction result types;
- function operands distinct from ordinary virtual registers; and
- explicit basic-block terminators and targets.

CorePrep, Xpp, and Xmm verification happen before LLVM objects are constructed. Xmm owns the final native IR verifier; the
LLVM `Verify` entry remains as a source-compatible adapter and `Lower` repeats the check for direct clients. A failed check
produces structured `VXP` or `VXL` issues and no LLVM artifact.

The canonical C++ spelling is `Visual::XSharp`, with PascalCase namespace segments, classes, and functions. The LLVM backend
is the first renewed subsystem that uses the canonical spelling as its real namespace rather than an alias. Older C++ middle-
end models still use their existing spelling during a subsystem-by-subsystem migration; `visual_xsharp` remains the Rust-side
naming convention and is not the target C++ namespace.

## Supported values

The connected slice lowers `Unit`, `Bool`, `Int`, `Long`, and `String`. Function types are used for checked call signatures.
Named types and unresolved type variables are rejected at the backend boundary until their concrete layout contract exists.
The backend never guesses a layout.

Source scalar names do not inherit C widths. The frontend/Core contract selects the Visual X# width, and LLVM lowering must
construct the corresponding integer or floating type explicitly. The current Core wire revision does not yet transport the
complete scalar catalog, so unsupported widths and floating payloads fail before target emission rather than being narrowed
to the older connected subset.

Source `void` is not a value. The frontend maps it once to the current resultless Core ABI marker. Value-producing source
`unit` remains semantically distinct even when a legacy native enum still contains a unit-like spelling.

`String` is not a UTF-8 byte string. A constant uses an immutable LLVM array of 32-bit Unicode scalar values, followed by a
zero sentinel for interoperation convenience, and a value containing a pointer plus the scalar count. The count excludes the
sentinel. Invalid Unicode scalar values are rejected.

## Registers and control flow

Xmm virtual registers represent lower-level mutable storage, not LLVM SSA names. The backend allocates typed storage for each
virtual register in the function entry block, stores parameters into their assigned registers, and loads or stores values at
instruction boundaries. This preserves assignments across branches without inventing phi nodes before a dedicated SSA pass
exists.

Every branch and jump target must belong to the current function. Branch conditions must be `Bool`; returns must match the
declared function result. Calls resolve by stable symbol identity and must match their complete function signature.

Integer division uses signed LLVM division. Floor division additionally adjusts a truncated quotient when the remainder is
nonzero and operand signs differ, preserving mathematical floor semantics for negative operands.

Numeric boolean context is normalized before Xpp. A branch therefore receives a canonical boolean value rather than asking
LLVM to reinterpret every integer width differently. This keeps source semantics out of the backend and lets the Xmm
verifier require one condition shape.

## Calls and entry bridge

Direct calls resolve a stable function symbol to a declared function. Parameter count/types and result type are checked in
Xmm before LLVM call construction. A function symbol is not encoded as an integer or ordinary virtual register.

Project entry selection names a class whose parameterless `public static void Main()` method returns `void`. Binary emission
adds the platform bridge required by the target executable format; source code does not change to an integer-returning C
`main`. The bridge is an emission concern and does not appear in Core/Xpp/Xmm as a user declaration.

Indirect callable invocation remains blocked with closure allocation. It requires a defined AARC closure object containing
target and environment information, not a raw function pointer approximation.

## Optimization and verification

The CLI optimization setting selects an LLVM new-pass-manager pipeline:

| Visual X# value | LLVM pipeline |
| --- | --- |
| `g` | `default<O0>` |
| `1` | `default<O1>` |
| `2` | `default<O2>` |
| `3` | `default<O3>` |

The generated module is verified before optimization. The backend builds LLVM's standard per-module optimization pipeline
with the C++ new pass manager and local analysis managers. LLVM contexts, modules, builders, printed IR streams, and bitcode
buffers have ordinary scoped C++ ownership and are released on every success or error path. The renewed backend does not use
the LLVM C API; LLVM-C remains confined to the isolated legacy C backend.

Optimization failure is an artifact failure. The backend does not serialize the pre-optimization module under the requested
output name after a selected pipeline fails. Tests should verify the module both before and after any custom pass additions.

## Target machine

`-Target` supplies a case-sensitive LLVM target triple. Without an explicit value, LLVM uses its host-dependent default.
Project target catalogs constrain an explicitly selected triple but do not implicitly select their first entry.

Target-machine creation owns:

- target lookup and diagnostic text;
- data layout and target triple on the module;
- relocation/code model defaults until public settings exist;
- assembly/object file selection;
- Windows COFF emission for the supported host target; and
- propagation of backend emission errors without leaving a plausible output.

Windows SDK and MSVC CRT/C++ development libraries are link inputs/toolchain resources. They do not make the Visual Studio
compiler or IDE part of the build architecture. ClangCL/LLD remain the native toolchain.

## Artifacts

Ordinary pipeline execution keeps printed LLVM IR and serialized bitcode in memory. Disk access occurs only through explicit
artifact APIs:

- `WriteLlvmIr` accepts only a `.ll` path;
- `WriteBitcode` accepts only a `.bc` path;
- `WriteObject` accepts only a `.o` path; and
- `WriteAssembly` accepts only a `.asm` path.

`vxs build -Emit llvmll` writes the sibling `.ll` file and `vxs build -Emit llvmbc` writes the sibling `.bc` file for either
`.vxs` or `VXCR` Core input. `vxs build -Emit object|assembly` writes target-machine output. Binary emission creates an
executable entry bridge, emits a temporary object, invokes LLD through a typed C++20 argument vector, verifies the `.vxse`,
and removes the temporary object. `vxs check` never emits. Xpp and Xmm artifact writers are not implied by this connection.

The Haskell frontend is the sole source owner. The backend remains responsible for target lowering and the driver remains
responsible for linking; no compatibility frontend, shell command construction, or DIMCLI route is used.

## Error discipline

The backend rejects rather than guesses when it encounters:

- an unverified Xmm module;
- duplicate function or register identity;
- a call whose symbol/signature does not match;
- a branch whose condition is not canonical `Bool`;
- a return inconsistent with the declared result;
- a named/type-variable value without a layout;
- an invalid Unicode scalar in a string constant;
- closure construction without the AARC ABI;
- an unsupported target or target-machine emission kind; or
- a requested file extension inconsistent with the writer API.

Errors include stage and relevant symbol/block/register context, but LLVM handles and internal object addresses are not part
of the public diagnostic contract.

## Backend extension checklist

Before adding a new value or instruction:

1. define its source/Core semantics in the owning frontend layer;
2. ensure the Core wire carries every required type/value without lossy reuse;
3. lower it through CorePrep, Xpp, and Xmm with verifier coverage at each boundary;
4. define target-independent representation before choosing LLVM layout;
5. specify ABI alignment, ownership, calling convention, and destruction where relevant;
6. add valid and malformed native tests;
7. verify textual IR and a target artifact where supported; and
8. update implementation status rather than presenting partial support as complete.

This sequence prevents an LLVM convenience type from becoming an accidental language or public ABI decision.
