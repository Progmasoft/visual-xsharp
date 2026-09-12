<!-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com> -->
<!-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1 -->

# Ownership-flow verification

Visual X# resolves source ownership before native lowering, but the Xpp and Xmm
layers still defend their own explicit AARC contract. A structurally valid
register is not necessarily a live object reference: it may hold a weak control
handle, an unowned control handle, or a strong handle that has already been
released. The ownership-flow verifier rejects those distinctions before LLVM
emits an unsafe runtime call or load.

This document describes compiler verification. It does not add source syntax,
change the source type system, or make CorePrep a public artifact.

## Stage boundary

Xpp introduces explicit ownership operations after CorePrep:

| Operation | Required input | Produced result | Input after operation |
| --- | --- | --- | --- |
| RetainStrong | live strong | live strong | live strong |
| ReleaseStrong | live strong | none | consumed |
| MakeWeak | live strong | live weak | live strong |
| LockWeak | live weak | live strong | live weak |
| ReleaseWeak | live weak | none | consumed |
| MakeUnowned | live strong | live unowned | live strong |
| LoadUnowned | live unowned | live strong | live unowned |
| ReleaseUnowned | live unowned | none | consumed |

Xmm preserves these operations while replacing symbolic storage with virtual
registers. Both stages use the same analysis model, but each owns its adapter
and diagnostic namespace. This prevents Xpp storage behavior from leaking into
the Xmm register model.

An ordinary AARC operand requires a live strong handle. Weak and unowned values
must first pass through LockWeak or LoadUnowned. A return of an AARC value also
observes a live strong handle because return transfers a usable reference to the
caller.

## Abstract state

For every tracked storage or register, the analysis records a set containing
one or more of these states:

- absent;
- live strong;
- live weak;
- live unowned; and
- consumed.

The set is intentional. At a control-flow join, choosing one predecessor would
make correctness depend on block order. Instead, the analysis unions all
reachable predecessor states. A strong value on one path and a consumed value
on another therefore remains strong plus consumed until a definition replaces
it.

Definitions replace the destination state. An AARC-producing instruction
defines its result as strong unless the operation specifically produces a weak
or unowned handle. A non-AARC definition forgets any ownership fact associated
with reused storage. Reads do not change state. A release converts only its
required live representation to consumed.

## Fixed-point behavior

The analysis runs forward over the function CFG:

    In[entry] = declared AARC parameters as strong; all others absent
    In[B]     = union(Out[P]) for every reachable predecessor P of B
    Out[B]    = transfer(B, In[B])

Entry facts do not include facts from an entry backedge. A value created during
a later loop iteration cannot initialize or resurrect the first entry
execution. Non-entry loops iterate until their state sets stop changing.

The internal lattice bottom used while the fixed point is forming is distinct
from absent. Treating a not-yet-visited predecessor as an actual absent path
would make results depend on traversal order. Regression coverage runs loops
with reversed block vectors to protect this distinction.

Only reachable blocks participate in semantic ownership diagnostics. Structural
verification still reports duplicate blocks, missing entries, and invalid
targets independently.

## Diagnostics

Xpp reserves these ownership diagnostics:

| Code | Meaning |
| --- | --- |
| VXP1042 | a released strong, weak, or unowned handle is used or released again |
| VXP1043 | an operation receives the wrong runtime handle representation |
| VXP1044 | incoming paths disagree about liveness or representation |

Xmm reports the equivalent conditions as VXL1046, VXL1047, and VXL1048. Every
issue carries the owning function, block, and instruction index. Return
diagnostics use the terminator position, which is the instruction count of its
block.

The analysis deliberately leaves absent-only reads to definite initialization.
That pass already has the precise diagnostic for a value that was never
defined on a path. Ownership handles the additional cases that remain after a
value is initialized.

## What the verifier guarantees

Before LLVM lowering, a successful Xpp/Xmm ownership check guarantees:

- no reachable explicit release consumes the same handle twice;
- no reachable ordinary operation or return observes a consumed handle;
- weak-only operations receive weak handles;
- unowned-only operations receive unowned handles;
- strong operations and ordinary AARC uses receive strong handles;
- a join cannot hide a release performed on only some paths; and
- block serialization order cannot select an ownership outcome.

The pass does not yet prove global leak freedom, infer missing retain/release
operations, or decide whether a source-level object graph contains a cycle.
Those are separate responsibilities. Automatic retain/release insertion must
eventually produce explicit Xpp operations that satisfy this verifier.
Concurrent Bacon-Rajan with trial deletion remains a future optional cycle
collector and does not weaken deterministic AARC verification.

## Testing

Three layers protect the contract:

1. Compiler/Analysis/Tests/OwnershipFlowTests.cpp tests the reusable state
   lattice, fixed point, malformed CFG handling, loops, joins, and deterministic
   facts.
2. Compiler/Codegen/Xpp/Tests/OwnershipVerifierTests.cpp tests symbolic storage,
   direct function identities, strong/weak/unowned conversion, returns, and Xpp
   diagnostic codes.
3. Compiler/Codegen/Xmm/Tests/OwnershipVerifierTests.cpp repeats the boundary
   cases for typed virtual registers and confirms the main Xmm verifier
   publishes ownership failures.

Tests construct a complete operation sequence. Merely asserting that an opcode
exists does not prove its handle precondition, result representation, or
control-flow lifetime.
