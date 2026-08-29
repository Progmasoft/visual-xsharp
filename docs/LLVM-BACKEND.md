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

## Artifacts

Ordinary pipeline execution keeps printed LLVM IR and serialized bitcode in memory. Disk access occurs only through explicit
artifact APIs:

- `WriteLlvmIr` accepts only a `.ll` path; and
- `WriteBitcode` accepts only a `.bc` path.

`vxs build -Emit llvmll` writes the sibling `.ll` file and `vxs build -Emit llvmbc` writes the sibling `.bc` file for either
`.vxs` or `VXCR` Core input. `vxs check` never emits. Object, assembly, executable, Xpp, and Xmm writers are not implied by
this connection.

The Haskell frontend is the sole source owner. Object and executable production will return when the new bitcode path is
connected to target-machine emission and LLD; no compatibility frontend fallback remains.
