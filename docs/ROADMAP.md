<!-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com> -->
<!-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0 -->

# Visual X# roadmap

This roadmap records intended sequencing rather than a compatibility promise.
An item remains unavailable until its implementation, runtime contract, and
tests are merged.

## Current compiler priority

The primary goal is to complete the production compiler pipeline:

```text
Source -> Lexer -> Parser -> Renamer -> Name Resolution -> Type Checker
       -> Desugarer -> Core -> Core Optimizations -> CorePrep
       -> Xpp -> Xpp Optimizations -> Xmm -> Xmm Optimizations -> LLVM
```

Intermediate representations remain internal unless an explicit emit format is
documented. CorePrep is an adapter between Core and Xpp and is not a user-facing
emit format.

Current work prioritizes semantic completeness, stage verification, native
executable generation, and removal of obsolete C implementation paths. New
runtime features must not displace completion of that pipeline.

## Concurrent cycle collection

Concurrent Bacon–Rajan cycle collection is planned only after the compiler is
complete. It is not implemented by the current runtime or compiler.

The intended project/CLI setting is:

```text
-Cycle-Collector true
```

The default is `false`. A program therefore continues to use normal AARC
behavior unless cycle collection is explicitly enabled.

When enabled, the collector must remain demand-driven:

- it does not run when no candidate cycle exists;
- it does not run after a candidate cycle has already been broken manually;
- ordinary acyclic AARC objects stay on retain/release paths;
- collection work is concurrent rather than a global stop-the-world scan;
- weak and unowned reference semantics remain unchanged;
- a strong cycle remains programmer-managed when the option is disabled.

The future design must specify candidate detection, color/state transitions,
root buffering, synchronization, safe-point interaction, shutdown behavior,
diagnostics, and deterministic testing before implementation begins.

No compiler pass should currently assume that a cycle collector exists. Closure
conversion and ownership lowering must preserve enough AARC information for the
future runtime to identify candidates without changing source semantics.

Acceptance work for that milestone will include stress tests for concurrent
mutation, candidate cancellation after manual cycle breaking, nested closure
cycles, shutdown races, weak promotion during collection, and deterministic
memory-accounting assertions. Benchmarks must compare the disabled path against
plain AARC to ensure that the default configuration does not pay collector
coordination costs.

The option will be rejected on runtimes which do not advertise the matching ABI
capability. It will not silently downgrade to a different collector, and it will
not change the ownership meaning of source declarations. Diagnostic output must
make activation, candidate processing, and unsupported-target failures
observable without exposing unstable internal colors as a language contract.
