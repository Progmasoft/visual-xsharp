<!-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com> -->
<!-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0 -->

# AARC object and ownership ABI

This document describes the first executable Automatic Atomic Reference Counting
(AARC) contract shared by type classification, Xpp, Xmm, LLVM, and the native
runtime. It covers ordinary acyclic lifetime management. The optional concurrent
cycle collector is a later layer, combines Bacon–Rajan processing with trial
deletion, and is disabled by default.

## Storage classification

| Language family | Storage class | Identity and null |
| --- | --- | --- |
| scalar primitives and source `void`/the internal no-result marker | trivial value | no identity; non-null |
| `data`, `type`, classic `enum` | CoW value | value semantics; non-null |
| `class`, `data class`, `enum class`, `object`, interface | AARC reference | identity-bearing; nullable |
| arrays, `String`, callable values | AARC reference | identity-bearing; nullable |

A CoW value may share an internal allocation. That detail does not give it
reference identity and does not turn assignment into aliasing. `StorageClass`
and `NominalKind` in `Visual/XSharp/Core/Ownership.hpp` form the canonical native
table. A named type without declaration metadata stays `Unresolved`; spelling is
never used to guess its ABI.

## Object header and destruction

Every dynamic AARC payload has an `ObjectHeader`. A back-pointer immediately
before the aligned payload locates the header from a strong object pointer. The
header contains the ABI version, atomic lifecycle state, atomic strong and weak
counts, immutable metadata, live payload pointer, and allocation base.

The weak count includes one implicit entry while the strong count is non-zero.
The last strong release changes the state from `Alive` to `Destroying`, invokes
the type destructor exactly once, clears the live payload, publishes `Destroyed`,
and drops the implicit weak entry. The last weak or unowned handle then reclaims
the combined allocation. A destructor releases fields owned by its payload but
does not free its own header.

`TypeMetadata` fixes payload size, alignment, ABI version, flags, destructor, and
diagnostic type name. Allocation rejects incompatible metadata and invalid
alignment before creating an object.

## Strong, weak, and unowned ABI

Generated code targets unmangled C entry points:

| Entry point | Contract |
| --- | --- |
| `vxs_aarc_allocate` | creates a payload with one strong owner and one implicit weak entry |
| `vxs_aarc_retain_strong` | returns the same live payload with one added strong owner |
| `vxs_aarc_release_strong` | releases an owner and destroys on the last release |
| `vxs_aarc_make_weak` | creates a control handle that does not keep the payload alive |
| `vxs_aarc_lock_weak` | returns a nullable, newly retained strong result |
| `vxs_aarc_release_weak` | releases a weak control handle |
| `vxs_aarc_make_unowned` | creates a non-owning handle that keeps only the header alive |
| `vxs_aarc_load_unowned` | returns a nullable, newly retained strong result |
| `vxs_aarc_release_unowned` | releases an unowned control handle |

Weak lock and unowned load use compare/exchange on a non-zero strong count. A
state check followed by a raw pointer load would race the last release, so both
successful operations deliberately upgrade their result to a balanced strong
reference.

## Xpp, Xmm, and LLVM

Xpp and Xmm wire version 2 preserve `RetainStrong`, `ReleaseStrong`, `MakeWeak`,
`LockWeak`, `ReleaseWeak`, `MakeUnowned`, `LoadUnowned`, and `ReleaseUnowned`.
Producing operations preserve the operand's language type. Release operations
have no destination and carry `Unit` as the result marker. Both stage verifiers
reject scalar ownership, wrong arity, producing releases, and discarded loads.

`MakeClosure` creates a payload containing an invoke-thunk pointer followed by
ordered capture slots. LLVM emits a payload type, private metadata, a private
destructor, allocation, capture initialization, and a thunk whose first argument
is the environment. The thunk loads hidden captures and appends public call
arguments before invoking the lifted target. Strong AARC captures are borrowed
for the duration of a call because the closure keeps them alive. Weak and
unowned captures are upgraded to temporary strong references and released after
the lifted call. The generated destructor balances every owning/control slot.

String constants keep `i32` Unicode-scalar storage and call
`vxs_aarc_string_literal`, which creates a `System.String` AARC object without
introducing UTF-8 storage.

## Current boundary

This slice does not yet insert whole-program retain/release placement for every
source binding, package the runtime into every final native link, or collect
cycles. It establishes the checked IR vocabulary, concrete object ABI, runtime
primitives, String and first-class closure invocation lowering, and regression
coverage that those later passes target. The future concurrent Bacon–Rajan plus
trial-deletion collector remains opt-in with `-Cycle-Collector true`. The
ordinary acyclic path must not pay its cost when disabled, and no trial begins
when no candidate exists or the program has already broken the candidate cycle.
