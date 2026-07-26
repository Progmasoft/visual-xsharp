// SPDX-FileCopyrightText: 2026 Leitwolf <xs-lang.chess031@slmails.com>
// SPDX-License-Identifier: MPL-2.0

// XGC:

//
// XGC is the optional X# garbage collector.
//
// RAII is the default memory-management mode. XGC is disabled unless a build
// explicitly enables it. Enabling XGC changes reclamation and cleanup timing;
// it does not disable ownership, move, borrow, or lifetime checking.
//
// The Kotlin project DSL enables XGC with:
//
// set("XGC_ENABLED", true)
//
// A direct compiler invocation may use:
//
// --xgc-enabled true
//
// XGC_ENABLED defaults to false. A project setting and command-line override
// must resolve to one effective boolean value before semantic analysis.
//

// ============================================================
// Memory model
// ============================================================

// XGC is a precise, mostly concurrent collector with three physically separate
// managed heaps.
//
// - The heap is divided into fixed-size regions.
// - The Main Heap contains Young and Old regions. Eden and Survivor are Young
//   roles; promoted regions become Old.
// - The Young collector performs concurrent marking and concurrent copying.
//   Mutator reads use a read/load barrier while forwarding is active, and
//   mutator writes use both SATB and remembered-set write barriers.
// - The Old collector performs concurrent SATB marking followed by concurrent
//   relocation through selected-region evacuation. This operation is called
//   relocation, not reallocation: object identity stays stable while its
//   physical location may change.
// - The Large Object Heap (LOH) is a physically separate heap for objects at
//   or above the runtime large-object threshold. It minimizes fragmentation
//   through concurrent marking and selective concurrent compaction. The
//   read/load barrier resolves forwarded LOH references during compaction.
//   Low fragmentation is a required post-condition, not an optional heuristic:
//   a successful LOH cycle leaves at most five percent fragmented free space.
//   Failure to meet the bound enters the recovery chain below.
// - The Pinned Object Heap (POH) is a physically separate, non-moving heap for
//   runtime/ABI allocations that require stable storage. It uses concurrent
//   marking, concurrent sweeping, and coalescing free lists.
// - Main Heap, LOH, and POH regions are never exchanged with each other.
// - Compiler-produced stack maps identify precise roots.
// - Safepoints delimit locations where collection may occur.
// - Type metadata provides precise object traversal.
// - Read and write barriers preserve relocation, SATB, and remembered-set
//   invariants while mutator threads continue to run.
// - Card tables and remembered sets avoid scanning the entire old generation
//   during young collections.
// - Global, static, thread-local, stack, and live register roots are scanned.
//
// "Concurrent" does not mean pause-free. Root publication, final mark
// reconciliation, and collector-state transitions use bounded safepoint
// handshakes. Tracing, copying, relocation, compaction, and sweeping perform
// their bulk work concurrently with mutator threads.
//
// Collection failure uses one ordered fallback chain:
//
// 1. The ordinary concurrent collection attempt.
// 2. One emergency concurrent retry with a larger reserve and a more
//    aggressive collection set.
// 3. A stop-the-world recovery collection if the emergency retry also fails.
//
// Allocation pressure, exhausted evacuation reserve, relocation conflicts,
// mark-queue exhaustion, or failure to finish within the collector deadline
// may trigger this chain. A successful attempt resets the next collection to
// ordinary concurrent mode. XGC never skips directly from the first
// concurrent failure to stop-the-world recovery.
//
// Collection timing is not observable language behavior. Programs must not
// depend on a collection happening at a particular allocation or time.
// Cycles do not keep otherwise unreachable objects alive.
//

// ============================================================
// No pinning
// ============================================================

// XGC has no user-visible object-pinning operation, pinned reference type,
// pinning attribute, or operation that moves an existing Main Heap or LOH
// object into POH.
//
// POH is an internal allocation class selected by runtime metadata for ABI
// objects whose address must be stable from allocation onward. Its existence
// does not expose a language pinning facility and does not make an ordinary
// managed reference safe to retain as a native pointer.
//
// A GC-managed object may move at every safepoint. Its address is not a stable
// identity and cannot be retained in a raw pointer across a safepoint, function
// call that may reach a safepoint, await point, or foreign call.
//
// Compiler-tracked references and roots are updated after movement. Code that
// requires a stable native address must copy the value or use explicitly
// unmanaged/RAII storage outside the XGC heap. CFFI cannot pin or reclassify an
// existing XGC object. Runtime components may allocate predefined ABI objects
// directly in POH, but cannot expose their managed payload address as a general
// raw-pointer escape hatch.
//

// ============================================================
// Destructors and finalizers
// ============================================================

// In the default RAII mode, a destructor runs deterministically at its
// ownership-defined drop point.
//
// With XGC enabled, the same cleanup member is a finalizer. Finalizer timing
// and ordering are unspecified. A finalizer may run on a runtime-managed
// thread after the object becomes unreachable, and it is not guaranteed to
// run before process termination.
//
// Finalizers must not be used for locks, files, transactions, or other cleanup
// that requires deterministic ordering. Finalizer reachability and
// resurrection rules are runtime semantics; resurrection is forbidden.
//

// ============================================================
// Deterministic cleanup with defer
// ============================================================

// `defer` is available only when XGC is enabled. It registers deterministic
// scope cleanup without changing the lifetime of the containing binding.
// Deferred actions execute exactly once in reverse registration order (LIFO)
// when control leaves their lexical scope through normal fallthrough, return,
// break, continue, or propagated panic unwinding.
//
// Conceptual forms:
//
// defer file.close();
//
// defer fn() {
//     release_temporary_resources();
// };
//
// The expression form captures its receiver and arguments when the `defer`
// statement is evaluated. The function form captures according to ordinary
// closure ownership and borrowing rules. A deferred action cannot outlive its
// lexical scope.
//
// Deferred actions are not executed when the process is forcibly terminated
// or when the runtime aborts without unwinding. A deferred action that panics
// while another panic is unwinding terminates the process.
//
// Using `defer` while XGC is disabled is a compile-time error. RAII mode uses
// ordinary deterministic destruction instead.
//

// ============================================================
// Ownership and concurrency
// ============================================================

// Move validity, aliasing, mutability, Send, Sync, and lifetime rules are the
// same in RAII and XGC modes. XGC determines when unreachable managed memory is
// reclaimed; it does not make an invalid borrow valid or make a type thread-safe.
//
// Each thread participates in safepoints and root publication. Collection may
// perform concurrent marking and parallel evacuation, but language-visible
// reads and writes keep their normal data-race and synchronization
// requirements.
//

// ============================================================
// Mode boundary
// ============================================================

// XGC mode is selected for the complete linked program. Object files compiled
// with incompatible memory-management modes cannot be linked into one X#
// program unless a future ABI explicitly defines such a boundary.
//
// XGC implementation details belong to the runtime and backend. HIR, MIR, and
// XLIL must represent ownership, roots, safepoints, barriers, and cleanup
// requirements without depending on LLVM APIs.
