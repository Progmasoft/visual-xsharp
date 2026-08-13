<!--
SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
SPDX-License-Identifier: MPL-2.0
-->

# Changelog

This file summarizes user-visible and developer-visible changes in the xs-project repository.

The 0.0.x development period is the pre-1.0 xs-project compiler infrastructure line. It does not imply a complete X#
source-to-native executable pipeline.

## 0.3.1 - 2026-08-13

- Replaced the C lexer/parser and compatibility semantic route with the Haskell frontend as the sole `.vxs` source owner.
- Rewrote the public driver around a typed C++20 command schema and removed DIMCLI.
- Connected `.vxs` checking and explicit Core/LLVM emission through the bounded Core boundary and C++20 backend pipeline.
- Removed unreachable C AST, macro, HIR/MIR, compiler-core bridge code and the obsolete Rust direct-IR FFI session wrapper;
  retained Rust IR models, algorithms, and tests.

- Simplified Kotlin dependency features: optional features now remain disabled by default and are activated only with
  `feature(name)` or `enable(name)`; the redundant `disable(name)` DSL operation was removed.

- Migrated monomorphization and codegen-unit planning from manual C allocation tables to GNU++26 ownership while
  preserving their existing C ABI, and added move-only C++ planning views plus Catch2 coverage.
- Added the Kotlin project setting `XS_LLVM_COMPILER = "AOT" | "ORC"`; it defaults to `AOT` and is omitted from plans
  targeting non-LLVM backends.
- Added resolved user-defined enum-data patterns to typed HIR and XHIR, including exact overloaded payload selection,
  inherited owner/tag identity, duplicate detection, scoped bindings, and exhaustive coverage validation.
- Lowered enum-data match statements and expressions through MIR and XLIL with tag extraction, `i32` comparison,
  conditional branches, selected payload-slot extraction, and exhaustive-final-arm fallthrough.
- Split conditional lowering from the growing MIR control-flow module and added verifier plus canonical-text regression
  coverage for payload-bearing, payload-free, overloaded, and inherited enum-data matches.

## 0.3.0 - 2026-08-12

- Added a versioned, bounded CorePrep binary contract shared by the Haskell frontend and C++20 native pipeline.
- Preserved recursive type information, symbol spellings, qualified names, and UTF-32 string values across the compiler boundary.
- Connected verified CorePrep input to the in-memory Xpp and Xmm lowering route.
- Added explicit `.core` artifact I/O without introducing intermediate files into normal compilation.
- Added cross-language golden bytes, malformed-input rejection, semantic verification, and ClangCL sanitizer coverage.
- Enabled `vxs build -Build core -File PATH.core` and `vxs check -Build core -File PATH.core`.

## 0.2.8 - 2026-08-02

- Accepted canonical fixed-width hexadecimal `i64` bit patterns while preserving the legacy decimal `const.i64` form,
  restoring writer/parser round trips for enum-data aggregates with inactive `Int` payload slots.
- Aligned enum-data regression tests with XLIL v1 numeric type-registry identifiers, explicit borrowed `Str` payloads,
  and the rule that a payload-free declaration is a regular `enum` rather than `enum data`.

## 0.2.7 - 2026-08-02

- Added a target-independent enum-data registry that validates same-category inheritance, deduplicates shared ancestors,
  assigns deterministic flattened tags, and resolves typed variant overloads by exact nominal or primitive payload type.
- Lowered typed and payload-free enum-data constructors into explicit HIR values carrying the selected declaration owner,
  flattened tag, and exact payload type; XHIR now round-trips and validates those records.
- Lowered enum-data constructors through MIR and XLIL as deterministic tagged aggregates, including overloaded payload
  slots, inherited variants, canonical inactive values, first-class parameters and returns, and LLVM native calls.
- Added a source-native enum-data overload fixture that produces and runs a `.vxse`, plus direct XHIR and middle-end
  regression coverage for payload selection, hierarchy validation, registry layout, and text round trips.
- Migrated the direct XHIR and XMIR compiler-core adapters to GNU++26 translation units while retaining their stable C
  entry points, beginning the incremental frontend-to-modern-C++ transition without rewriting working C23 subsystems.

## 0.2.6 - 2026-08-02

- Expanded the Rust XLIL producer attribute with immutable local bindings, scoped value blocks, floating-point operations,
  unary operations, value conditionals, and short-circuit boolean control flow.
- Added configurable module and companion names plus a canonical-text companion for attributed XLIL producers.
- Split procedural-macro inference from body lowering and added a dedicated public Rust XLIL API guide.
- Added the optional `xslang` `proc-macros` feature and the `#[xslang::xlil_create]` attribute. It preserves a supported
  Rust function and generates a verified XLIL module producer beside it.
- Added explicit Rust-side `I8` through `I128` and `F16` through `F128` XLIL type selectors plus a `Utf32Builder` that
  emits numeric UTF-32 code points without retaining source text.
- Added `xslang::xlil::typed`, a checked producer facade with typed signatures, registers, slots, constants, arithmetic,
  comparisons, calls, branches, and returns while retaining the canonical raw builder and verifier as the only XLIL
  model implementation.

## 0.2.5 - 2026-08-02

- Strengthened the public Rust XLIL surface with checked signature-derived calls, verified-module proof wrappers,
  combined parse/verification errors, stream-oriented read/write helpers, and read-only model inspection methods.
- Added optional `xslang::rust::XSResult<T>` and `XSError` aliases for thread-safe application error boundaries without
  replacing XLIL's structured build, parse, verification, and I/O errors.
- Added the root-exported `xslang::printf!` Rust convenience macro with checked C-style format conversions and a safe
  standard-output implementation under `xslang::rust`.
- Added feature-gated Kotlin project dependencies through `addOptionalModule`, deterministic feature selection, and the
  version-1 SQLite lock schema. Required, optional, enabled, and disabled dependency records now round-trip without a
  readable lock-file surrogate.
- Added reusable MIR CFG, dominator/frontier, natural-loop, and local-liveness analyses backed by the compiler core's
  graph/dataflow dependencies.
- Added a verified optimization pipeline with levels, fixpoint iteration, dead pure-definition elimination, unused-local
  pruning, identical-target branch simplification, and canonical reverse-postorder block layout.

- Migrated the `xs` executable entry and command schema to GNU++26. DIMCLI 7.6.0 now provides subcommand parsing,
  validation, and generated help while the existing C23 compiler driver and public C ABI remain intact.
- Added command-specific `--help`, pinned DIMCLI source, and the corresponding third-party notice.
- Made CI initialize pinned submodules, install the required system fmt package, and use pinned Catch2 3 when the runner
  does not provide a compatible system test framework.

## 0.2.4 - 2026-08-02

- Began the compiler's incremental Clang GNU++26 transition while retaining the existing strict C23 frontend and stable
  C ABI. Added an owning C++ XLIL API under `<xs/lil/*.hpp>` with RAII modules and builders, typed handles, parsing,
  verification, canonical text emission, calls, and control flow.
- Made every `<xs/lil-c/*.h>` entry point safe to consume from C++ through explicit C linkage and moved the reserved C
  AOT header to `<xs/lil-c/aot.h>`.
- Added the first Rust/GNU++26 compiler-core bridge through the `cxx` crate and a Clang-built native bridge library.
- Added system `fmt` and Catch2 integration without vendoring or package-manager-specific build logic.

- Added structured typed-HIR `Result<T, E>` values, contextual `Ok`/`Error` constructor checking, and exact error-channel
  validation for postfix `@`.
- Lowered Result propagation through explicit MIR discriminant branches, active-payload extraction, early error return,
  canonical aggregate reconstruction, XLIL, LLVM IR, object emission, and native `.vxse` execution.
- Added exhaustive `Ok(binding)`/`Error(binding)` match arms with lexical payload scope. Result matches reject duplicate
  variants, non-Result selectors, and payload-type disagreement, and lower without introducing Result-specific XLIL or
  LLVM instructions.
- Added native source and Kotlin-project coverage for successful propagation, failure propagation, nested
  `Result<Optional<T>, E>`, Result payload matching, malformed constructors, mismatched errors, and invalid match arms.
- Advanced the canonical XHIR, XMIR, and XLIL text format header to version `1`. Readers continue to accept version `0`
  artifacts for compatibility, while writers now emit version `1`.
- Lowered `Optional<T>` coalescing through target-independent MIR aggregate extraction, conditional control flow,
  merge storage, XLIL, LLVM IR, object emission, and native `.vxse` execution. Both `Some` and `nil` source paths are
  covered in direct source and Kotlin project builds.
- Completed mutable-local Optional coalescing assignment and postfix forced unwrapping through typed HIR, canonical
  XHIR/XMIR, MIR verification, XLIL, LLVM trap lowering, and native `.vxse` tests. `??=` evaluates its replacement only
  on the `None` edge, while postfix `!` makes the failed-unwrapping path explicit as a panic terminator.
- Added safe `?.` access for fields of optional `data` values. The lowering maps `Optional<Data>` to
  `Optional<FieldType>` with target-independent aggregate/control-flow records and recursively builds deterministic
  zero payloads for absent non-recursive aggregates.
- Made aggregate-registry discovery traverse complete HIR expression trees, including nested calls, blocks, Optional
  operations, tuples, maps, and control flow, so expression-only aggregate result types are registered before MIR.
- Extended the public C23 XLIL parser to consume canonical hexadecimal two's-complement bit patterns emitted by the Rust
  XLIL writer for signed `i32` and `i64` constants.

## 0.2.3 - 2026-07-29

- Moved the experimental managed-heap implementation out of `xslang` into the independent X Platform project, where it
  continues as XPG. Removed the former XGC project switch and CLI override from the X# compiler.
- Added LLVM-only Kotlin project controls for LTO and optimization level. PGO remains mandatory and ORC JIT remains
  outside the LLVM backend contract.
- Established the separate X Platform `0.0.1` foundation with language-neutral XPI v0, the `xpic` verifier and
  canonicalizer, and a directly interoperable Swift/GNU++ XPG heap core.
- Completed the public C23 XLIL v0 producer surface behind `<xs/lil.h>`. Third-party frontends can construct every
  implemented type, function, block, instruction, and terminator record; verify it; emit owned canonical text; parse it
  back; and inspect typed payloads without exchanging a C `FILE *` across their FFI boundary.
- Added the official Rust `xslang::xlil` producer/reader surface. Its insertion-point `Builder`, public read-only model,
  parser, verifier, and canonical writer provide the same target-independent XLIL v0 boundary without exposing LLVM.
- Added the `XsLilBuilder` convenience layer, stable opaque function handles, packed C API version negotiation, public
  symbol visibility annotations, and an installed shared `xs_lil` library as the foundation for official FFI bindings.
- Added the official Java 25 FFM binding with XLIL writer and reader packages plus the reusable
  `org.xsslang.ffi.c.CString` helper. The binding delegates parsing, verification, construction, and canonical text
  emission to the stable `lil-c` ABI.
- Published `org.xsslang:xlil:2026.1` through the independent Maven-compatible repository at
  `java.xsharp-lang.xyz`.
- Connected canonical `new Type(...)` data-constructor expressions to typed HIR, MIR aggregate values, XLIL, and LLVM.
  Constructor results can now be returned, projected, and passed directly through the scalarized native data-call ABI;
  the former constructor-shaped `Type(...)` call is rejected.
- Integer literals now retain the default inferred `Int` type while accepting an explicit `Bool` context. Contextual
  values are normalized with zero as false and every nonzero integer as true; the native Bool representation remains u8.

## 0.2.2 - 2026-07-26

- Defined XGC as three physically separate managed heaps: Young and Old share the Main Heap, while LOH and POH have
  independent region directories and allocation paths.
- Added concurrent collection plans for Young copying, Old relocation/evacuation, fragmentation-oriented LOH
  compaction, and non-moving POH mark-sweep with coalescing free lists.
- Added XGC read/load forwarding barriers, SATB and card-table write barriers, cross-region remembered edges, and
  selection policy for fragmented LOH regions.
- Added the ordered XGC failure path: ordinary concurrent collection, one emergency concurrent retry, and bounded
  stop-the-world recovery.
- Added runtime allocation-class selection, separate heap routing, POH allocation/reuse, and expanded XGC telemetry.

- Added the buildable `xsrt` runtime with ABI version 0 and explicit boxed ownership for immutable UTF-16
  `Optional<Str>` values.
- Added the public C23 `.xspkg.tar.zst` writer/verifier boundary with deterministic tar metadata, path and resource
  validation, and SHA-256 artifact identity.
- Added `xs resolve`; it reevaluates KTS dependency coordinates and atomically refreshes a real binary
  `xs.lock.sqlite3` without compiling sources.
- Unified compiler and Kotlin project resolution in the `xs` package. `/usr/bin/xs` is the only user command; the
  mandatory JRE 25 project runtime is bundled under `libexec` and still delegates script execution to `kotlin`.
- Documented the planned C23, JavaScript, and WebAssembly backend contracts while keeping LLVM as the only implemented
  backend.
- Removed the discontinued legacy manifest subsystem and its compiler flag.
- Added the first bounded `xsfmt` pass for newline and trailing-whitespace normalization, plus `--check` mode.
- Added the first `xstidy` diagnostic rule for legacy constructor syntax with human and JSON output.
- Added the initial `xs-analyzer` LSP lifecycle and full-document UTF-16 synchronization capability.
- MIR optimization now removes terminal boolean negations by reversing branch targets and collapses branches whose
  successors are identical.
- Added `xs test` for modern Kotlin projects and `xs test -file` for one source. The resolver transfers its disjoint test
  registry to the native compiler; each valid top-level `#[Test] fn name()` receives an isolated native harness and is
  compiled through HIR, MIR, XLIL, LLVM, object emission, linking, and `.vxse` execution. `Ignore` and `ShouldPanic` are
  honored, and unexpected native failure makes the command fail.
- `panic!` statement calls now survive expanded-AST materialization and lower to the existing MIR/XLIL panic terminator
  and LLVM trap path.

- Added KTS build mode, output-directory, and package-artifact settings. `BUILD_MODE` defaults to `Release`, while
  omitted `XSPKG_TYPE` is inferred from optional `lib.<extension>` and `main.<extension>` source files. Structured
  `BINARY` and `LIBRARY` records support named artifacts whose entry files use non-canonical names. Projects that omit
  `source.include` now use the recursive `Sources` root by default, while an existing `Modules` directory supplies the
  optional default module root. Existing `<source-root>/Test` directories supply default test roots.
- Source, module, and test editor/package exclusion metadata now defaults to `*/**`. It remains independent from
  compiler include registries; explicit lists replace the metadata default and empty `exclude()` clears it.
- Compiler registries are disjoint by construction: test selections override module membership, and module membership
  overrides broad source roots.
- Added source, module, and test `filter(listOf(...))` metadata with cross-registry defaults resolved after project
  include defaults are known.

### Added

- `xs run` and `xs run -file` now execute the native `.vxse` produced by the supported compiler
  pipeline and propagate the program exit code. Compiler installation now has an explicit component that installs the
  `xs` command, both public C23 header trees under one `include/xs` hierarchy, and project license notices.
- Tuple destructuring declarations now cross the structural AST into typed HIR as explicit tuple storage and projection
  bindings. Inferred and explicitly typed nested patterns, `else` discard fields, MIR/XLIL aggregate extraction, LLVM
  lowering, and native `.vxse` execution share the existing target-independent tuple registry.

### Changed

- Constructor calls use `new Type(...)`; constructor-shaped `Type::new(...)` expressions are rejected while `::` remains
  available for ordinary associated and static members.

## 0.2.1 - 2026-07-21

### Added

- Kotlin project dependencies now use structured `addModule(name, stability, version)` coordinates and successful KTS
  evaluation writes them to a deterministic, versioned `xs.lock.sqlite3` SQLite lock file. The reserved boolean
  `PUBLISH` project setting defaults to `false` and records future publication intent without uploading packages.
- Payload-free normal enums now retain nominal declarations and ordered variant tags through XHIR, lower to a
  target-independent single-tag aggregate in MIR/XMIR/XLIL, and support construction, direct calls, local storage,
  equality, statement `match`, LLVM lowering, and native `.vxse` execution.
- Explicit concrete turbofish calls to top-level generic functions now create deterministic Rust compiler-core
  monomorphizations and continue through typed HIR, MIR, XLIL, LLVM, object emission, and native `.vxse` execution.
- Reachable generic specializations are discovered transitively, including same-instance recursion. Expanding
  polymorphic recursion is rejected before it can exhaust memory, and interface constraints are checked against direct
  and inherited implementations.
- XHIR v0 declaration records now preserve interface declarations in addition to class and data declarations.
- `xslang/build.rs` now exposes crate and supported XHIR/XMIR/XLIL format versions to the Rust compiler core at build time.

### Changed

- The repository's original source code is now licensed under MPL-2.0. Generated Gradle wrapper launchers retain their
  upstream Apache-2.0 notices.
- Rust static analysis now has a warning-free `clippy::all` baseline with explicit legacy public-module documentation
  boundaries, and the C artifact-path helper is clean under the configured Clang-Tidy correctness checks.
- Project-file documentation now includes the concrete version-0 `xs.lock.sqlite3` schema and example query results.
- CMake ownership now follows monorepo component boundaries: shared policy is in the root `CMakeLists.txt` and compiler
  modules are under `xs/cmake`.
- Semantic compiler-core lowering failures now cross the C23/Rust boundary as session diagnostics; malformed FFI packets
  remain a distinct ABI error.

## 0.2.0 - 2026-07-18

### Added

- XLIL v0 now has runtime-sized `.array %aN : T` registry entries and `%rN:i64 = len.array %rA`. Rust and public C23
  model/parser/writer/verifier APIs preserve the same readable registry contract.
- Runtime-sized `[T]` values now cross function signatures, checked indexing and mutation, `count`, `for` iteration,
  LLVM lowering, object emission, and native `.vxse` execution.
- The `xslang` Rust compiler-core crate is now packaged as a documented public crate.

### Changed

- An unconstrained square-bracket array literal remains runtime-sized `[T]`; only an explicit `[T; N]` context selects a
  fixed layout. Runtime-sized arrays retain a fixed element count after construction and remain distinct from
  count-changing `ArrayList<T>` values.

## 0.1.9 - 2026-07-18

### Added

- Public `<xs/c23_features.h>` and selective `<xs/c23/trait.h>` / `<xs/c23/impl.h>` headers provide the first strict-C23
  trait-object, implementation binding, and dynamic-call helpers. A standalone public-header test exercises real dispatch.
- Chained `data` field projection from arbitrary values, including aggregate-returning calls, now has a typed XHIR `member`
  record and lowers through MIR/XLIL aggregate extraction to LLVM `extractvalue` and native `.vxse` execution.
- Built-in collection spelling is now consistent across the specification: resizable sequences use `ArrayList<T>`, maps
  use `[K: V]`, sets use `[T] = {...}`, and none of these built-in forms requires `import collections`.
- Ordered, transitive, and multiple `data` base lists now cross the C23 structural AST into Rust HIR and canonical XHIR.
  Inherited fields are laid out base-first in source base-list order, participate in object initialization, member access,
  mutation, scalar call ABI lowering, XMIR aggregate registries, LLVM IR, and native `.vxse` execution. Unknown/cyclic/
  cross-category bases and ambiguous inherited field names are rejected before backend lowering.
- XHIR v0 now preserves nominal `data` declarations, field order, field types, and field mutability before function
  records. Direct XHIR rebuilds the same aggregate registry used by source and XMIR, allowing nested nominal data values
  to survive source → XHIR → native `.vxse` round-trips.
- XMIR v0 program documents now preserve aggregate and fixed-array registry entries in a structured `types` section.
  Direct XHIR rebuilds the same deterministic registry from its higher-level tuple/array types. Tuple-valued calls and
  fixed-array operations now survive both source → XHIR → native and source → XMIR → native `.vxse` round-trips.

### Changed

- XLIL v0 writes signed 64-bit constants with the explicit `%rN:i64 = const.i64 N` opcode. The old untyped
  `const N` spelling is rejected, keeping scalar constant records uniformly width-explicit without introducing
  target-dependent pointers or ABI details into XLIL.
- Direct `xs build --hir -file <program.xhir>` and `xs build --mir -file <program.xmir>` now parse complete version-0
  program documents in the Rust compiler core, validate their typed/control-flow models, lower through verified XLIL, and
  reuse the LLVM object/link path to produce `.ll`, `.o`, and native `.vxse` artifacts. Canonical XHIR program output now
  records leading function parameters separately from ordinary locals, preserving call ABI across source → XHIR → native
  round-trips.
- `xs build --output hir|mir|xlil -file <source.vxs>` and the `--hir`/`--mir`/`--xlil` short forms now write real
  compiler-core output beside the source as `.xhir`, `.xmir`, or `.xlil`. Kotlin project builds use the merged program
  session and write beside the selected entry source. Multi-function XHIR/XMIR files have one versioned `program` record,
  explicit `.function end` boundaries, and a final `.program end`; their Rust readers round-trip the complete document.
- Fixed-size built-in arrays now support source-level `for (value in values)` iteration through typed XHIR, MIR control
  flow, checked array indexing, LLVM IR, and native `.vxse` execution. The iterable is evaluated once, the element
  binding is lexical and immutable, and `break`/`continue` target the generated loop exit/update blocks.
- Positional and named tuple values now cross the Rust compiler core as structural typed-HIR tuples, use target-independent
  MIR/XLIL aggregate construction and extraction, lower to LLVM structure values, and execute in native `.vxse` fixtures.
  XHIR v0 preserves tuple types, field names, literals, element projections, and element assignments in its human-readable
  text form. Tuple values can cross same-module function signatures, nest inside other tuples, and appear as fixed-array
  elements. Chained positional projection such as `value.0.1` remains source syntax; arrays continue to use `value[index]`.
- Fixed-array `for` iteration accepts recursively nested tuple patterns, optional tuple type annotations, and `else` discard
  elements. Compiler-core desugars each pattern to one hidden typed element binding plus explicit tuple projections before
  MIR CFG construction, so the target-independent aggregate and checked iteration paths remain shared.

## 0.1.8 - 2026-07-16

### Added

- The first XGC foundation in `xslang` defines logical heap addresses, fixed 2 MiB regions, mark bitmaps, card tables,
  adaptive remembered sets, thread-local SATB buffers, precise roots and stack maps, collection-set scoring, and
  saturating telemetry. Kotlin `set("XGC_ENABLED", true)` and the `--xgc-enabled` override reach JVM-free compiler
  policy. Allocation, collector threads, barriers, and runtime integration remain deliberately inactive.
- The public XGC Rust API is available exclusively through `xslang::xgc::*`. Executable use requires an explicit X#
  runtime binding for object layout, reference tracing, and root rewriting; no legacy `xslang::gc` alias is provided.
- Data constructor overloads now lower through typed HIR, MIR, XLIL, LLVM IR, and native `.vxse` emission. Constructor
  selection uses exact parameter types, hidden constructor symbols preserve overload identity, and definite field
  initialization rejects paths that leave primitive storage uninitialized.
- Top-level function overloads and instance/static `data` methods now use exact parameter-type selection. Instance
  receivers desugar to explicit target-independent HIR arguments, and deterministic hidden symbols remain distinct through
  direct XHIR/XMIR round-trips and native `.vxse` emission.
- `<xs/lil.h>` is now the umbrella for the selective public C23 XLIL producer headers under `<xs/lil-c/*.h>`.
  Rust producers use `xslang::xlil::*`; both surfaces can build and verify human-readable XLIL v0 modules.
- Canonical `[T; N]` fixed arrays and `[T]` declarations initialized by square-bracket literals now preserve their element
  count through typed HIR, MIR, XLIL, LLVM, and native `.vxse` emission.
- Fixed-array literals now fill omitted numeric elements with zero and discard elements beyond the declared layout, as
  required by the collection specification. Literal index assignment reconstructs the target-independent MIR aggregate
  and can mutate array contents without reassigning a `val` binding.
- Brace-delimited homogeneous literals now have a dedicated built-in set structural-AST and typed-HIR representation.
  The public source type remains `[T]`; there is no nominal `HashSet<T>` spelling.
- Fixed arrays expose canonical `count`, `capacity`, `is_empty`, `start_index`, `end_index`, `first`, and `last` value
  members. Compiler-core resolves them before nominal field lookup and lowers them through the existing typed HIR and
  native array path.
- Kotlin projects may define importable source membership in `xs.module.kts`. Direct members and optional `submodule`
  blocks accept concrete paths or globs and cross the xs-project/xs-compiler boundary in a module-aware v3 registry.
- Project source, test, and module includes are recursive directory roots rather than globs. Exclude patterns retain glob
  support, `--module` supplies an omitted module root, and `XS_EXTENSION` replaces the default `.vxs` discovery suffix.
- Positional and named tuple types/literals now have explicit structural-AST tuple field records, including positional
  `.0` member syntax.
- Source files now support one optional source-scoped namespace and any number of nested block-scoped namespaces.
- `#[MacroExport]` is retained on macro declarations and is restricted to module or namespace scope. Importing the
  exporting module makes the macro callable by its unqualified name without placing it in the prelude.

### Changed

- `[T] = [...]` denotes an array and `[T] = {...}` denotes a built-in set. A `[T]` declaration without an initializer is
  an array. The literal delimiter supplies the distinction when an initializer is present.
- Fixed arrays now support calculated `Int` indices for reads and writes through checked MIR/XLIL array access records,
  the public C23 XLIL model, and bounds-checked LLVM lowering. Native `.vxse` fixtures cover both operations.
- Prefix operators now consume a postfix-complete operand, so `!value.member` is parsed as `!(value.member)`.
- The source keyword is now singular `import`; legacy `imports` is no longer tokenized as a keyword. Source-level
  `module` declarations have been removed in favor of project metadata, and control-flow parentheses are optional.
- Module names are case-sensitive. Omitted declaration visibility is `internal` and is enforced against the exact logical
  module; cross-module use requires `public` plus the correctly cased import.
- `using namespace` no longer acts as a module import for exported macros; `import stdio;` is required before calling
  Stdio exports such as `println!`.

## 0.1.7 - 2026-07-15

### Added

- The structural parser now accepts canonical built-in collection type forms: `[T]`, fixed-size `[T; N]`, and map
  `[K: V]`. Square-bracket array literals and `[key: value]` map literals now have distinct structural AST records. These
  forms are compiler-known and do not require a standard-library import; `String` inside them still canonicalizes to the
  existing `Optional<Str>` sugar. Array and map declarations retain dedicated compiler-core type records rather than
  desugaring to nominal collection names. Typed HIR now preserves array/map literals, performs homogeneous inference and
  contextual element checks, and round-trips those records through XHIR.
- First-class fixed arrays now lower through typed HIR, MIR, XLIL `%aN` registry records, the public C23 XLIL API, LLVM
  array values, object emission, and native `.vxse` linking. XLIL construction and constant indexing use `array` and
  `extract.array` records; dynamic arrays and maps remain deferred beyond typed HIR.
- Nominal `data` return values now use first-class aggregate values across typed HIR, MIR, XMIR, XLIL, LLVM, and native
  `.vxse` emission. Aggregate-returning calls can initialize local `data` places, including nested layouts, and their fields
  are extracted back into the existing place model without exposing LLVM types to HIR or MIR.
- XLIL v0 now has sequential nominal aggregate type registry records, first-class aggregate construction and field
  extraction instructions, matching Rust and public C23 parse/write/verify APIs, and LLVM named-structure lowering.
  Direct XLIL builds exercise the complete path through object emission and a native `.vxse` executable.
- HIR now resolves automatic `Optional`, `Result`, `Error`, and `Task` types together with import-gated standard-library
  types and functions through one standard-library registry. Associated enum-data variants, static user functions,
  `Task<Result<...>>` propagation contexts, and destructuring/for-each pattern bindings participate in name checking.
- Every complete program under `Spec/Programs` now has an individual semantic `xs check` regression test in addition to
  the recursive structural-syntax suite.
- `xsfmt` gained a small, isolated TOML configuration model for width, indentation, tabs, and newline policy. Formatting
  X# source remains a later milestone.
- The structural frontend now retains generic type-qualified associated expressions, typed object literals, typed
  patterns and tuple-pattern bindings, async function expressions, and unconditional `loop` statements. `loop` also
  crosses the compiler-core, MIR, XLIL, LLVM, and native `.vxse` path for the supported body subset.
- `Str` `==` and `!=` now lower through typed HIR, XMIR `eq.str`/`ne.str`, XLIL, the public C23 model, and hosted LLVM
  native emission. Equality compares UTF-16 code-unit length and content rather than backing-pointer identity.
- Floating-point `!=` now crosses XHIR, XMIR, XLIL `ne.f32`/`ne.f64`, the public C23 XLIL model, and LLVM ordered
  not-equal comparison lowering. Contextual unary `+` and expression-inferred call locals also use the compiler-core
  path.
- Boolean `&&` and `||` now lower as real short-circuit control flow through typed HIR, MIR branches, XLIL `br_if`, LLVM,
  and native `.vxse` builds. Inferred `:=` locals now derive their type from supported expressions rather than literals
  alone, so chained logical results retain `Bool` without an explicit annotation.
- Same-module direct calls now support self recursion and mutual recursion across KTS-selected source files through typed
  HIR, MIR, XLIL, LLVM, and native `.vxse` emission. Unit-returning call statements lower without a result register, while
  semicolon-terminated value calls retain evaluation and explicitly discard their result.
- Prefix and postfix integer updates now preserve their different expression results through typed HIR, MIR local
  storage, XLIL, LLVM, and native `.vxse` output. The remaining arithmetic/bitwise compound assignments also use the Rust
  compiler-core route instead of the temporary C source bridge.
- Primitive fields of locally initialized `data` values now cross the structural AST, typed HIR, MIR storage, XLIL, and
  LLVM pipeline. Object literals, field reads, and field assignments are covered by native `.vxse` regression tests.
- Nested `data` values now scalarize recursively for local storage, whole-value copies, nested-field replacement, and
  same-module calls. Non-recursive `data` parameters use declaration-order primitive leaf parameters in the current native
  ABI; direct object literals and initialized local places can both supply those calls.

### Changed

- LLVM/Clang/LLD tool selection and validation now lives in the root `CMakeLists.txt`. The formerly monolithic CTest
  manifest is split into direct XLIL, source value/control/call, Kotlin, and library test modules.
- Source-native body compilation now has one authoritative implementation: structural AST enters the Rust compiler core,
  then returns verified XLIL through the public C23 boundary for LLVM emission. The obsolete C source-body fallback was
  removed after the complete source-native fixture matrix passed with it disabled.
- Native project tests now evaluate real `xs.project.kts` files with the external Kotlin 2.4.0 runner and JRE 25, including
  multi-file and fixed-width integer projects.
- Rust compiler-core sessions can merge multiple expanded structural-AST source trees into one program-wide declaration,
  typed HIR, MIR, XLIL, and LLVM module. Same-module helper calls across project files now produce native `.vxse` output.
- Compiler-core sessions retain type-check, MIR-lowering, borrow-check, and optimization diagnostics instead of silently
  dropping failed function bodies. The C23 driver exposes those messages when native emission cannot proceed.
- Recursive by-value `data` parameters are rejected with an explicit indirect-ABI diagnostic instead of recursing during
  compiler lowering. Non-recursive `data` returns now use the shared aggregate registry rather than the scalar parameter
  ABI.
- Kotlin `sources` includes now expand `*`, `**`, and `?` globs, apply excludes, require exactly one resolved `main.vxs`,
  and emit a deterministic main-first source registry.
- `--warning all|medium|low|none`, `--werror true|false`, and `--verbose true|false` provide one-shot compiler-policy
  overrides for KTS and direct source invocations. The KTS resolver now transfers its evaluated
  `compiler {}` policy with the source registry. Defaults are medium warnings, warnings-as-errors disabled, and verbose
  progress enabled.
- The Kotlin project DSL exposes strict `get(name)` and lossless `getAll(name)` lookup. `set(name, value, ...)` supports
  multi-value settings, replacing the former `targets { target(...) }` section; the complete example prints project,
  compiler, backend, and `TARGET` values through ordinary Kotlin `println` calls.
- The Kotlin/JVM 25 `xs-project` resolver evaluates combined `xs.project.kts` files or split `xs.settings.kts` and
  `xs.build.kts` files through the required external `kotlin` script runner. Explicit source registries require one
  case-sensitive `main.vxs` entry and are compiled by the JVM-free `/usr/bin/xs` process.

### Changed

- The Kotlin project resolver now accepts JRE 25 and newer releases; 25 is the minimum runtime version rather than an
  exact-version requirement.
- Kotlin project host matching treats BSD systems as members of both the BSD and UNIX families.
- The compiler flag is spelled `--werror`; the former misspelling is rejected. ReactOS is a distinct internal host OS,
  has no public OS constant, and satisfies `FAMILY == WINDOWS` without satisfying `OS == WINDOWS`.
- Split Kotlin project files have no section ownership rule. `xs.settings.kts` is evaluated first and `xs.build.kts`
  extends its complete state; either file may call any DSL function.

## 0.1.6 - 2026-07-13

### Added

- Arithmetic, division/remainder, bitwise operations, shifts, equality, and ordering now preserve every fixed integer
  width through XHIR, verified XMIR, optimized XLIL, signed/unsigned LLVM lowering, object emission, and native `.vxse`
  builds. The public C23 XLIL API exposes the same typed operation model to third-party producers.
- The complete fixed-width integer literal family now lowers through typed HIR, decimal XMIR constants, fixed-width XLIL
  hexadecimal bit patterns, LLVM IR, object emission, and native `.vxse` builds. Range checking includes signed minimum
  values and full u128 values. Public C23 `XsUInt128`/`XsInt128` types use two 64-bit words; no `__int128` extension is used.
- `Char` literals now cross the structural AST into typed HIR as one UTF-16 code unit, lower through XMIR `const.u16`,
  XLIL `%rN:u16 = const.u16 0xXXXX`, LLVM `i16`, object emission, and native `.vxse` builds. Parameters, locals, direct
  calls, and returns preserve the 16-bit value; the public C23 XLIL API can construct and inspect the record.
- Project-local VS Code settings exclude generated build, Cargo target, dependency, and output directories from file
  watching and workspace search to avoid indexing multi-gigabyte artifact trees.
- Borrowed-static `Str` literals now lower through XHIR, endian-neutral `utf16 [...]` MIR/XMIR, explicit
  `const.str utf16le|utf16be [...]` XLIL v0 records, LLVM static data, PIC object emission, and native `.vxse` linking.
  The public C23 XLIL API exposes the selected encoding and individual UTF-16 code units.
- HIR now documents its coordinated THIR and XHIR sides explicitly: THIR carries typed/source-oriented facts, XHIR carries
  normalized operations for MIR lowering, and `.xhir` represents XHIR rather than THIR.
- Explicit `String` annotations are source sugar for boxed `Optional<Str>`. String literal inference still produces the
  borrowed-static `Str` type; C23 semantic checks and the Rust compiler core preserve this distinction through canonical
  XHIR text.
- `SFloat`/`Float` addition, subtraction, multiplication, division, remainder, equality, and ordered comparisons now
  lower through typed HIR, MIR/XMIR, XLIL v0, LLVM IR, object emission, and native `.vxse` builds. XLIL spells these as
  width-explicit records such as `add.f32` and `lt.f64`; Linux native linking supplies the math runtime needed when LLVM
  legalizes floating remainder to `fmod`/`fmodf`.
- `SFloat` and `Float` literals now cross the C23 structural AST into Rust typed HIR, verified MIR/XMIR, XLIL v0, LLVM
  IR, object emission, and native `.vxse` builds. XMIR and XLIL store exact IEEE-754 f32/f64 bit patterns in fixed-width
  hexadecimal `const.f32` and `const.f64` records.
- Same-module `Int` functions now lower as real i64 functions through typed HIR, MIR/XMIR, XLIL, LLVM IR, object
  emission, and native `.vxse` builds. Division, remainder, bitwise operations, signed shifts, and ordered comparisons use
  the complete i64 instruction family without changing the required `Long`/i32 process entry ABI.
- Unary `+`/`-` on explicitly typed `Long` values and logical `!` on `Bool` values now remain in the Rust typed-HIR,
  XHIR, verified MIR/XMIR, optimized XLIL, LLVM, and native `.vxse` route. `Long` is lowered as i32; `Int` remains i64.
- `Long` division, remainder, bitwise AND/OR/XOR, and left/arithmetic-right shifts now cross the C23 structural AST into
  Rust typed HIR, XHIR, verified MIR/XMIR, XLIL, and the existing LLVM native `.vxse` backend. Context-free integer
  literals remain `Int`; an explicit `Long` return, binding, or parameter supplies the i32 lowering context.
- `format_args_nl!` is now a source-callable compiler-special built-in and shares format-string validation with
  `format_args!`; neither formatting-argument intrinsic can be declared or shadowed through `macro_rules!`.
- Value-producing `match` expressions with `Long`/`Bool` selectors, literal arms, and a final `else` now cross the C23
  structural AST into typed HIR, versioned XHIR, MIR control flow and storage, XLIL, LLVM IR, and native `.vxse` output.
- Value-producing `if` expressions now lower in general value contexts, including local initializers and function-call
  arguments, through target-independent MIR merge storage and the existing XLIL/LLVM native pipeline.

### Changed

- `Optional<T>` and `Result<T, E>` are compiler-provided `enum data` families with explicit `Some`/`None` and
  `Ok`/`Error` variants. User `enum data` declarations may inherit those unspecialized standard families and remain
  usable as their nominal bases.
- `Result<T, E>` accepts both payload types without an error-class bound. The only one-argument form is `Result<()>`,
  which defaults its error payload to the standard `Error` class; incomplete forms such as `Result<Int>` are rejected.
- Runtime and standard-library failures use the single standard `Error` class by default. Applications may derive custom
  error classes from it; former official domain-specific error and exception types are no longer part of the Spec.
- The canonical `format!(...)` expansion is `std::fmt::format(format_args!(...))` with no synthetic source-level block.
- `format!` and the `std::fmt::*` API are both provided by the non-prelude `Stdio` module; importing Stdio makes that
  standard formatting surface available.

### Removed

- Legacy exception declarations and control-flow syntax were removed. Recoverable failures use `Result<T, E>` and postfix
  `@`; panic remains a separate explicit termination mechanism.
- The cancelled XS Backend project was removed. LLVM is the compiler's native backend.

### Fixed

- Nested binary literals now retain an explicit `Long` target context recursively instead of reverting to the default
  `Int` type inside the expression tree.
- Fresh checkouts no longer require the intentionally untracked `xslang/Cargo.lock` as a Ninja input; the pinned Rust
  toolchain can generate the local lock file during the compiler-core build.
- Formatting syntax and the `print!`, `println!`, `eprint!`, `eprintln!`, and `format!` expansion structures now follow
  the exact Rust 1.57 contract. Empty line forms delegate to `print!("\n")`/`eprint!("\n")`; formatted line forms use
  the compiler-internal newline-format argument intrinsic.

## 0.1.5 - 2026-07-13

### Added

- Classic `for` statements now cross the C23 structural-AST boundary into checked HIR, target-independent MIR control
  flow, XLIL, LLVM IR, and native `.vxse` emission for the supported source subset.
- XHIR v0 can read and write explicit `for` records with optional initializer, condition, and update sections.

### Changed

- `write!` and `writeln!` are built-in writer macros and no longer require `import stdio;`. Stdio continues to export
  `print!`, `println!`, `eprint!`, `eprintln!`, and `format!`.
- Result is no longer implied by ordinary output examples. Functions using postfix `@`, `Ok(...)`, or `Error(...)` must
  declare a Result return type; ordinary functions remain free to return unit or another declared type.

## 0.1.4 - 2026-07-13

### Added

- Rust compiler-core HIR now import statement `match` with typed `Long`/`Bool` selectors, literal arms, and a required
  final `else`; HIR verification rejects mismatched, duplicate, and incomplete arm sets.
- HIR `match` lowers into explicit MIR test/body/merge blocks and continues through XLIL `eq.i32`/`br_if`, LLVM IR,
  object emission, and native `.vxse` linking.
- Rust MIR now carries `store.local` and `load.local` statements. XMIR parser/writer, MIR verifier and borrow checker,
  MIR-to-XLIL lowering, XLIL stack slots, and regression tests cover the new storage path.
- Source syntax now accepts `do { ... } while (condition);`. The frontend marks it as post-test loop sugar and lowers it
  through the existing loop, conditional-break, MIR CFG, XLIL, and native backend path.
- Prefix `++value`/`--value` and postfix `value++`/`value--` are distinct value expressions: prefix produces the updated
  value, postfix produces the previous value, and both lower mutation through the existing local storage path.

### Changed

- HIR local initializers and assignments now compute into temporary MIR values and store into stable local storage. Reads
  load fresh values, preserving mutation across branches, loops, and match merges.
- Existing native match and mutable-loop fixtures now use the Rust compiler-core pipeline without losing local updates.

## 0.1.3 - 2026-07-13

### Added

- Rust compiler-core HIR now import `while`, `break`, and `continue`, verifies loop conditions and jump placement, and
  lowers loops through target-independent MIR and XLIL control-flow graphs.
- Class declarations now retain multiple base specifiers with per-base access and virtual-inheritance metadata. HIR
  validates duplicate bases, cycles, sealed bases, and compatible `virtual`/`override`/`sealed` method slots.
- Interface, data, and `enum data` declarations now accept unlimited same-category inheritance; cross-category inheritance
  and payload-free enum base lists are rejected.

### Changed

- Omitted declaration, member, and base visibility is consistently `internal`, including declarations nested in public
  namespaces and external blocks.
- XHIR statement writing is split into a dedicated module and v0 text round-trips structured loop records with explicit
  `.end` block markers.

## 0.1.2 - 2026-07-13

### Added

- Compiler-core sessions now lower supported integer/boolean return expressions and arithmetic/comparison trees into the
  existing Rust HIR type checker and HIR-to-MIR pipeline. Unsupported function bodies remain deferred without invalidating
  the imported declaration module.
- Explicitly typed local declarations and simple assignments now import into Rust HIR, while leading function parameters
  lower as real MIR parameters instead of uninitialized locals.
- Same-module direct calls with resolved parameter and return types now cross the compiler-core import boundary and lower
  through typed Rust HIR, MIR, and XLIL call records.
- Statement `if`/`else` and value-producing `if` expressions now import into typed Rust HIR blocks. Returned conditional
  values lower to verified multi-block MIR and XLIL `branch_if` control flow.
- Supported single-unit source builds now consume compiler-core XLIL through the public C23 XLIL parser and continue through
  LLVM IR, object emission, LLD linking, and native `.vxse` generation. Unsupported source forms retain the existing C23
  source-native fallback while the Rust compiler core grows.
- Same-module compiler-core functions may carry `Long` and `Bool` parameters through MIR, XLIL, LLVM calls, and native
  execution.

### Changed

- Rust `hir.rs`, `mir.rs`, and `xlil.rs` are now small module/re-export surfaces. MIR and XLIL data models live in dedicated
  `model.rs` modules, while the canonical checked-HIR model is re-exported from `hir.rs`.
- HIR value lowering and XHIR block writing were split into focused modules as conditional control-flow support grew.

## 0.1.1 - 2026-07-13

### Added

- The C23 driver now flattens every macro-materialized structural AST into the versioned compiler-core ABI v0 packet:
  fixed node records, a child-index table, and a text-byte arena. Rust `xslang` validates that packet and retains an owned,
  target-independent syntax tree in a compilation-unit session for typed-HIR lowering.
- Compiler-core sessions now lower top-level module and function signatures, including parameter and return types, into the
  first Rust HIR declaration model. Primitive names are resolved during this import while other nominal types retain their
  source names.

## 0.1.0 - 2026-07-13

### Added

- Source-native `Long` and `Bool` local declarations now lower to MIR places, XLIL stack slots, and LLVM
  `alloca`/`store`/`load`; simple assignments update mutable locals through the same target-independent memory path.
- MIR borrow validation now distinguishes the first initialization store of an immutable local from a rejected
  reassignment.
- Source-native builds now lower a statement-level `if` with one simple assignment per branch and a post-branch local
  read through MIR branch/merge blocks. Immutable initialization uses CFG-aware path state, so assignments on mutually
  exclusive branches do not spuriously conflict.
- The source-native `Long` slice now supports `+=`, `-=`, `*=`, `/=`, `%=`, `&=`, `|=`, and `^=` compound assignments,
  including multiple assignments in a supported `if` branch.
- Source-native conditional assignment lowering now supports nested `if` statements. The driver was split into expression/
  statement lowering and program/native-module construction units to keep the C23 implementation modular.
- Source-native `while` now lowers a supported Bool condition, assignment-only body, and loop exit through MIR CFG,
  XLIL branches, and LLVM IR.
- Source-native `break` and `continue` now target the nearest supported `while` exit and header, including from nested
  conditional blocks.
- MIR local-place validation now rejects a `load` unless the local is initialized on every reachable path; immutable-store
  validation retains its conservative reassignment protection across control-flow joins.
- Supported source-native `if` and `while` bodies may now declare `Long`/`Bool` locals with lexical block lifetime;
  their MIR places still lower through XLIL stack slots and LLVM allocas.
- Supported source-native nested `return <expression>;` statements now lower directly to MIR return terminators, including
  from a `while` body or conditional branch.
- Source-native `else if` chains now lower as ordered MIR conditional blocks with one shared merge target.
- Source-native classic `for (initializer; condition; update)` loops now lower through MIR header/body/update/exit blocks;
  `continue` targets the update block and `break` targets the exit block.
- Source-native lexical scopes now allow an inner block to shadow an enclosing local while still rejecting duplicate names
  in the same scope; leaving the block restores the enclosing binding.
- Postfix `++` and `--` are now represented by the structural AST and lower for supported mutable `Long` locals, including
  canonical `for (...; i++)` updates.
- Function expressions now use the inferred `fn(a, b) { a + b }` form; written lambda parameter and return types are
  rejected while function type annotations remain typed.
- Class construction now requires `new Type(...)`. The untyped `new()` spelling is restricted to the dedicated `else:`
  discard form.
- Specification declarations and examples now consistently use snake_case for modules, namespaces, functions, methods,
  fields, and local bindings while preserving PascalCase for types and enum variants.
- Source-native statement `match` now lowers `Long` and `Bool` selectors with literal arms and a required final `else`
  through MIR branches, XLIL, LLVM IR, ELF object emission, and native `.vxse` linking.

## 0.0.8 - 2026-07-13

### Added

- C MIR and XLIL now carry an explicit `panic` terminator through verification and text round-tripping; LLVM lowers it to
  `llvm.trap` followed by `unreachable`.
- XLIL v0 now has typed stack slots (`.slot %sN:type`) plus register-to-slot `store` and slot-to-register `load`
  instructions in both the public C23 model and Rust `xslang`.
- C MIR root-local places now lower to XLIL stack slots, and LLVM emits corresponding `alloca`, `store`, and `load`
  instructions. Direct XLIL native builds exercise the complete path through `.vxse` linking.

### Changed

- `Str` is documented as the X# UTF-16 counterpart of Rust `&'static str`, with compiler/runtime-selected UTF-16LE or
  UTF-16BE endianness. `Optional<Str>` is documented as the owned optional string case corresponding to Rust
  `Option<String>`.
- `macro_rules!` definitions now use `->` between a matcher and its expansion block, matching the current X# macro syntax
  direction.
- Class properties now use canonical X# name-first declarations such as `public Name: Str { getter; setter; }`.
- X# source syntax now distinguishes discarded and tail values: `expression;` discards to unit, while a final block
  expression without `;` remains value-producing and feeds implicit-return desugaring.
- Class property declarations now use `getter` and `setter` accessors. Plain `get` and `set` remain available as ordinary
  identifiers.
- Source examples now use `()` as the written unit type instead of source-level `void`; XLIL `void` and
  `std::cffi::Void` remain backend/FFI markers.

### Deprecated

- Legacy exception syntax remains parseable for now but is scheduled for removal in X# 2.0.0.

## 0.0.7 - 2026-07-13

### Added

- Source-native builds now lower same-module `Long -> Long` helper calls through C MIR, XLIL, LLVM IR, object emission, and
  native `.vxse` linking.
- Source-native builds now also lower same-module `Long -> Bool` helper calls for supported `if` conditions and explicit
  `Bool` local initializers.
- C MIR now has a call instruction and MIR-to-XLIL call lowering for the supported source-native direct-call slice.
### Changed

- `Optional<T>` and `Result<T>`/`Result<T, E>` implicit namespace behavior is documented: Optional still behaves as if
  `import optional; using namespace std::optional;` existed, while Result treats `import result;` as optional and brings
  `std::result` names such as `Result`, `Ok`, and `Error` into scope.
- The C23 HIR type resolver now accepts canonical `std::result::Result<T>`, `std::result::Result<T, E>`, shorthand
  `Result<T>`, and short `Error` names without a user-defined project symbol.
- Pattern defaults and placeholder spellings use `else`; `_` is no longer produced as a wildcard pattern or inferred
  lifetime/type placeholder.

- HIR CFFI validation now checks the first standard CFFI attribute shapes and scopes for extern blocks, extern functions,
  and extern static declarations.
- `import Module;` now records module usability without placing module symbols in local scope. `Result` and `Panic` are
  implicit standard import; `Panic` assertion/panic macros no longer require an explicit import. `Stdio` remains explicit
  and is not prelude.

## 0.0.6 - 2026-07-13

### Added

- The C23 lexer/parser now accepts `#[...]`/`#![...]` attribute syntax and top-level
  `extern "ABI" { fn ...; static ...; }` blocks.
- HIR symbol collection now descends into extern blocks, collecting foreign functions and foreign static globals with
  inherited block visibility.
- HIR type resolution now recognizes the initial standard CFFI type family and validates its generic arity.
- HIR semantic validation now accepts only explicitly represented C ABI extern blocks, requiring `#[repr(C)] extern "C"`.
- `Spec/Attrs.vxs` and `Spec/CFFI.vxs` document the standard attribute registry and the explicit opt-in CFFI surface.

### Changed

- `format_args!` is now validated as a built-in macro instead of a Stdio export.
- Formatting macro validation now accepts `write!` and `writeln!` forms plus common debug and alignment specs such
  as `{:?}`, `{:#?}`, `{:08x}`, and `{:_>8}` while still checking placeholder counts.
- `Optional<T>` now resolves as an implicit compiler-provided `Optional` import alias for `std::optional::Optional<T>`, with
  value constructors available in source as `None` and `Some(...)`.

### Deprecated

- Legacy exception syntax is now deprecated. `throws`, `throw`, and `try` remain parseable but produce warnings; new code
  should use `Result<T, E>` and postfix `@` propagation.

### Fixed

- The C23 HIR expression checker now accepts postfix `@` inside functions returning `Result<T, E>` or
  `Result<T, E>` and rejects it outside Result-returning functions. Direct same-file function call operands are now also
  checked to return a Result type.
- `Spec/` examples now use Result-style error handling for active examples. Legacy `throws`/`throw`/`try`/`catch` syntax is
  kept only in deprecated legacy notes.

## 0.0.5 - 2026-07-12

### Added

- Imported `Stdio` macros now include `print!`, `println!`, `eprint!`, `eprintln!`, and `format!`, with
  newline-only `println!()`/`eprintln!()` forms.
- `Spec/Stdio.vxs` now documents `std::stdin()` line input examples, and `Spec/Result.vxs` documents the explicit `Result`
  model, postfix `@` propagation, `expect`, and `unwrap`.
- The C23 lexer/parser now accepts postfix `@` as a structural Result propagation expression; semantic checking and lowering
  are intentionally left for later Result work, and HIR expression checking reports that gap explicitly instead of silently
  accepting it.
- Rust `xslang` XHIR now models and round-trips Result propagation with a `propagate` expression record.
- Rust `xslang` type checking and inference now implement the first Result propagation rule: `Result<T>@` and
  `Result<T, E>@` yield `T`, and the enclosing function must return a Result type with a compatible error channel.
  HIR-to-MIR lowering still intentionally rejects propagation until control-flow lowering for error returns exists.
- Rust `xslang` now includes an explicit Result propagation desugar pass. Surface `value@` is translated into a
  Result-match/early-return intent model before MIR lowering work begins, so backend stages do not need to treat `@` as a
  primitive operation.
- Rust `xslang` MIR lowering now exposes a `DesugaredFunction` entry point. Desugared functions without ResultMatch lower
  through the existing HIR path, while explicit ResultMatch nodes are rejected until MIR Result control-flow lowering exists.
- Rust `xslang` XHIR text writing can now emit desugared functions, including explicit `result_match` records for `@`
  desugar output.
- Rust `xslang` treats single-argument `Result<T>` as using the standard `Error` error type for propagation type checking
  and desugaring.
- The C23 HIR type resolver now recognizes the standard wrapper type names `Optional<T>`, `Result<T>`,
  `Result<T, E>`, shorthand `Result<T>`, and `Error` without requiring a user-defined project symbol.
- C MIR, XLIL, MIR optimization, and LLVM lowering now support signed i64 bitwise operations, shifts, inequality, and
  signed ordering comparisons.
- Plain source native builds now accept explicit `Long` and inferred i32-compatible local bindings before the final
  `return`, and those locals can be returned or used by the current i32 expression/condition lowering slice.
- Plain source native builds now accept explicit and inferred `Bool` local bindings for the current `if` condition lowering
  slice.
- C MIR, XLIL, LLVM lowering, and the source-native slice now support materialized boolean negation through `not.bool`, so
  `Bool` local initializers can use unary `!` in the current native path.
- The lexer/parser and source-native `Long` expression slice now support bitwise `^`, lowering to the existing `xor.i32`
  MIR/XLIL/LLVM path.
- C MIR constant folding now covers the i32 arithmetic, bitwise, shift, equality, and signed comparison family used by the
  current source-native `Long` slice.
- C MIR constant folding now turns `branch_if` with a known `const.bool` condition into a direct `goto`, allowing CFG cleanup
  to remove the dead branch.
- Source-native `if` expressions with a syntactically constant boolean condition now lower only the selected branch in the
  current narrow native slice.
- Source-native `if` expressions with constant i32 literal/arithmetic comparisons now also lower only the selected branch.

### Changed

- Source-native builds now run MIR constant folding and CFG cleanup before MIR is lowered to XLIL.

### Known gaps

## 0.0.4 - 2026-07-12

### Added

- Native executable artifacts now use the `.vxse` extension; the first implemented container target is Linux ELF, with PE
  planned after ELF support.
- `xs build -file <Main.vxs>` can now produce `.ll`, `.o`, and `.vxse` artifacts for the
  first supported source-native entry slice: top-level `main` returning `Long` with i32-range literals, `+`, `-`, `*`, and
  one top-level `if` expression over i32 comparisons.
- XLIL, MIR, LLVM lowering, and source-native `if` conditions now support signed `ne.i32` inequality.
- XLIL, MIR, LLVM lowering, and source-native `Long` return expressions now support bitwise `and.i32` and `or.i32`.
- XLIL, MIR, LLVM lowering, and source-native `Long` return expressions now support signed i32 `shl.i32` and arithmetic
  `shr.i32` shifts.
- C MIR, XLIL, MIR optimization, and LLVM lowering now support signed `div.i64` and `rem.i64`, completing the current
  direct i64 arithmetic family.
- C MIR to XLIL lowering now supports `const.bool`, and source-native `if` conditions accept `true`/`false`.
- XLIL, MIR, LLVM lowering, and the source-native slice now support signed `div.i32`.
- XLIL, MIR, LLVM lowering, and the source-native slice now support signed `rem.i32`.
- Source-native `Long` return expressions now support unary `-`.
- Source-native `if` conditions now support unary `!` by swapping branch targets.
- Source-native `Long` return expressions now support unary `+`, matching the documented narrow arithmetic slice.
- XLIL v0 and direct LLVM lowering now support signed `i32` comparison instructions: `lt.i32`, `le.i32`, `gt.i32`,
  and `ge.i32`.
- C MIR to XLIL body lowering now creates all destination blocks before lowering terminators, so `br` and `br_if` can target
  later blocks.

### Changed

### Known gaps

## 0.0.3 - 2026-07-10

### Added

- Direct XLIL native builds now run the LLVM verification and optimization pipeline before writing LLVM IR and object
  artifacts.
- XLIL v0 and the LLVM backend now support `add.i32`, `sub.i32`, `mul.i32`, and `eq.i32` instructions.

### Changed

### Known gaps

## 0.0.2 - 2026-07-10

### Added

- XLIL v0 now has explicit `.param %rN:type` body records and registry-verified direct `call` records.
- The public C23 XLIL API verifies direct call signatures, and the LLVM backend lowers XLIL parameters and calls.
- The C23 XLIL model and LLVM backend now support `add.i64`, `sub.i64`, `mul.i64`, and `eq.i64` instructions.
- Direct `xs build --xlil -file` now emits LLVM IR, an object file, and a local native executable for a defined
  `.func main : () -> i32` XLIL entry point.
- XLIL v0 now supports `%rN:i32 = const.i32 N` for direct native process exit values.
- Rust `xslang` now carries `const.i32` through XMIR text, MIR verification, MIR → XLIL lowering, and XLIL v0 text.
- GitHub Actions now verifies C23 debug and sanitizer builds, Rust formatting/tests, and patch hygiene.
- X# source comments are line-only; `include!` now enters the driver after the initial structural AST parse.

### Changed

### Known gaps

## 0.0.1 - 2026-07-09

### Added

- LLVM-project-style monorepo layout.
- `xs` compiler project and public C23 compiler APIs.
- X# lexer, structural AST parser, and base diagnostic system.
- Macro validation, token expansion, statement/declaration reparsing, and expanded-view infrastructure.
- Initial HIR symbol table, import resolution, name resolution, and type resolution infrastructure.
- Generic arity, duplicate generic parameter, and generic interface constraint checks.
- Primitive type metadata for `bool`, `byte`, `sbyte`, `char`, integer types, supported float types, and `str`.
- MIR model API, MIR writer, borrow-check skeleton, and initial optimizer passes.
- XLIL model, assembly-like text writer/parser/verifier, and limited MIR-to-XLIL body lowering.
- LLVM backend infrastructure for context/module/target/data layout management, signature lowering, LLVM IR text emission,
  object emission, and linker abstraction.
- CLI recognition for direct file forms: `xs build --output hir|mir|xlil -file ...` and
  `xs build --hir|--mir|--xlil -file ...`.
- Direct `.xlil` inputs with supported version/module headers and top-level `.extern`/`.func` signatures can produce LLVM IR
  declarations.
- Public XLIL C23 API can parse v0 text registry files and expose read-only function signature metadata.
- LLVM backend can lower the first XLIL body subset: parameters, `const i64`, `const.bool`, direct calls, `br`, `br_if`,
  `ret`, and `ret %rN` functions.
- Documentation clarifies that `.xhir`, `.xmir`, and `.xlil` are human-readable text formats, not binary or opaque
  serialized compiler state.
- XHIR and XMIR documentation now separates their structured text design from XLIL's assembly-like registry format.
- Rust `xslang` gained initial structured XHIR/XMIR text writers plus parser subsets.
- XHIR parsing now covers the checked-function subset emitted by the Rust writer.
- XMIR tests now cover goto, local return values, unreachable terminators, and the current local statement records.
- XLIL text now starts with `.xlil version 0` before module records.
- MIR/XMIR can now carry explicit function parameter/return types, optional XLIL local value types, and `const.i64`
  statements; Rust MIR → XLIL lowering can emit signatures, `const`, and `ret %rN` for typed i64 local returns.
- Rust `xslang` XLIL gained typed `call` instructions plus writer/parser/verifier coverage; MIR/XMIR can carry typed call
  statements that lower when their arguments already have XLIL values.
- Rust `xslang` gained a MIR structural verifier for duplicate ids, missing terminators, and unknown local/block references.
- Rust `xslang` optimizer gained a verified entry point that checks MIR before and after optimization.
- XMIR text support gained optimizer analysis writer/parser coverage for optimization pass reports.
- XMIR text support gained structural verifier analysis writer/parser coverage.
- XHIR text support gained type-check analysis writer/parser coverage.
- Root README and strengthened documentation set under `docs/`.
- `xs --version` reports the current compiler version.
- XLIL text uses register-style value names such as `%r0`.

### Changed

- Module selected-import syntax corrected its former plural misspelling to singular `from`.
- `byte` resolves to `u8` at HIR level, and `sbyte` resolves to `i8`.
- `char` is documented as a 16-bit UTF-16 code unit.
- `str` is treated as UTF-16 and unbounded up to runtime allocator and target environment limits.
- HIR and MIR are documented as independent from LLVM and tied to the XLIL vocabulary instead.
- XLIL text moved away from C-like function records toward assembly-like `.xlil`, `.extern`, `.func`, label, typed SSA
  value, `br`, and `ret` records.
- Structural parser can consume a `>>` token as two closing `>` tokens when ending generic type/parameter lists.
- `match` examples and tests use the parenthesized subject form, such as `match (value) { ... }`.
- X# no longer exposes `float16` or `double` as language primitive type names.

### Known gaps

- Full AST macro replacement does not yet perform in-place parent-child rewrites.
- General expression type inference, overload resolution, and nominal interface membership checks are incomplete.
- MIR statement/expression lowering and async state machine generation are incomplete.
- Borrow checker is not yet a full region/loan/move/drop model.
- End-to-end `xs build`/`xs run` object/link/native executable flow is incomplete.
- Direct XLIL build mode currently lowers only the supported initial body subset.
