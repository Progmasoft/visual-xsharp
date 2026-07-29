<!--
SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
SPDX-License-Identifier: MPL-2.0
-->

# Current X# compiler status

The compiler is written in C23 and uses Clang, CMake, Ninja, LLVM tools, and LLD. New and touched C code uses C23
`bool` directly, does not include `<stdbool.h>`, and prefers `nullptr` over `NULL`.

The documented compilation order is preserved:

```text
.xs sources
    → lexing and parsing
    → structural AST
    → macro expansion
    → semantic analysis, dependency graph, and type checking
    → HIR (THIR + XHIR)
    → MIR
    → borrow checker
    → MIR optimization
    → monomorphization
    → codegen unit splitting
    → XLIL
    → LLVM IR and LLVM optimization
    → object code
    → linking
```

## Implemented infrastructure

### Monorepo layout

- The repository has moved to an LLVM-project-like monorepo selection model.
- The monorepo root is `xs-project`; the top-level CMake project name is `xs_project`.
- Top-level CMake recognizes `XS_ENABLE_PROJECTS`; the default stable project is `xs`.
- `XS_ENABLE_RUNTIMES` was added in parallel with LLVM’s `LLVM_ENABLE_RUNTIMES`; `xsrt` is buildable.
- `XS_ENABLE_PROJECTS=all` selects all stable projects.
- `xs` is the stable buildable compiler project.
- The root `include/` directory is reserved for shared public C headers across projects; `xs/include/` is for `xs` only.
- `xsfmt` and `xstidy` are future Rust nightly + Serde projects; `xs-analyzer` is a future Rust language server and
  TypeScript VS Code extension project. `xslang` is the active Rust compiler-core static library linked into `xs`;
  `xsrt` provides the initial runtime ABI boundary.

### HIR model boundary

- HIR is one intermediate layer with two coordinated sides, not two unrelated IR stages.
- THIR is the typed/source-oriented side; XHIR is the normalized operational side consumed by MIR lowering.
- `.xhir` serializes XHIR only. It is not a text form of THIR.
- The Rust implementation currently shares model types between typed checking and XHIR text support. Making the
  THIR/XHIR boundary explicit inside HIR without inventing a second pipeline stage is active compiler-core work.

### XLIL-bound middle-layer rule

- HIR and MIR do not and will not depend on LLVM.
- HIR and MIR are tied to the target-independent XLIL type/data vocabulary.
- HIR/MIR headers do not contain LLVM C API, target triple, data layout, or LLVM type/value concepts. They may include XLIL
  headers.
- XLIL is positioned before LLVM IR as the shared low-level vocabulary for HIR/MIR and the common input for backends.
- LLVM IR generation is a separate backend layer; HIR/MIR/XLIL do not carry LLVM C API concepts.
- Backend consumers lower typed, borrow-checked, monomorphized MIR to XLIL and then to their own target IR.
- Planned public API surfaces are `#include <xs/hir/jit.h>` for the HIR baseline JIT, `#include <xs/mir/jit.h>` for the MIR
  performance JIT, and `#include <xs/lil/aot.h>` for XLIL AOT. These APIs must not make HIR/MIR depend on the LLVM C API.
- LLVM is the only currently implemented backend, but the architecture remains open to Cranelift, a C backend, an interpreter, or
  other targets.
- Target-specific assembly, if needed, belongs in a separate backend/runtime layer. NASM `.asm`/`.inc` use is allowed, but it
  must not lock the design to x86-64; ARM64 compatibility must be preserved.

### Supported build toolchain

- CMake is configured and tested with Clang, Ninja, LLVM tool equivalents, and LLD.
- Archive, symbol, object-copy, object-dump, strip, and linker tools are selected from the LLVM tool family.
- Other compiler or generator combinations are outside the supported build contract.
- The compiler is built in strict ISO C23 mode with `-std=c23` and `CMAKE_C_EXTENSIONS OFF`.
- The build uses strict ISO C23 rather than compiler-extension dialects.

### Project system

- Kotlin `xs.project.kts` or split `xs.settings.kts` + `xs.build.kts` projects are evaluated by the project runtime bundled
  with `xs`, on JRE 25 or newer through an external `kotlin` scripting command. Argument-free `xs` starts this internal
  runtime and retains all `.xs` compilation work itself.
- Kotlin source, test, and module includes name directory roots; the resolver recursively selects the configured source
  extension while excludes retain glob support. `XS_EXTENSION` defaults to `xs` and may select another extension.
- `xs resolve` reevaluates declared module coordinates and atomically replaces `xs.lock.sqlite3` through the Kotlin
  resolver. The artifact is binary SQLite; text/SQL forms exist only as documentation examples.
- The version-4 Kotlin resolver protocol transfers source, module, and test registries separately. `xs test` combines
  those registries for frontend and semantic validation, discovers top-level unit-returning `#[Test]` functions from the
  expanded structural AST, and creates one synthetic `main` per test in the Rust compiler core. Every non-ignored harness
  follows MIR/XLIL/LLVM/object/link and isolated native `.xse` execution; `ShouldPanic` reverses the exit expectation.
- Standard `panic!` statement calls remain compiler-recognized nodes when no user `macro_rules!` replacement exists.
  They lower to HIR panic, the MIR/XLIL panic terminator, and the LLVM trap path. Formatted panic payload transport is not
  implemented yet.

### Runtime and package boundary

- `xsrt` is a selectable CMake runtime and exports ABI version 0 through `<xs/runtime.h>`.
- The first runtime-owned model is boxed `Optional<Str>`: `None` is a null opaque box, while `Some` owns an immutable
  copied sequence of UTF-16 code units. Clone, borrow, and drop have explicit C23 APIs and tests.
- `<xs/package.h>` exposes deterministic `.xspkg.tar.zst` writing and verification. The current container requires
  `xspkg.json`, rejects unsafe/non-regular/duplicate entries, enforces resource limits, and reports SHA-256 identity.
- Package manifest schema validation, registry downloads, publication, and compiler-to-runtime owned-string lowering
  remain outside this first boundary.
- `include!` is the built-in source-inclusion macro. It runs after the enclosing source first has a structural AST, then
  reparses the included local source at the call site; it is not a lexer/preprocessor step or a `macro_rules!` declaration.
- `xs build --output hir|mir|xlil -file <source.xs>` and `--hir`/`--mir`/`--xlil` write real compiler-core program output
  beside the source. Kotlin project output uses the merged source session and the selected entry source as its artifact base.
- Direct `.xhir` inputs use the Rust program reader, type checker, HIR → MIR lowering, MIR verification/borrow checking,
  optimization, and MIR → XLIL lowering before entering the existing C23 XLIL/LLVM native backend.
- Direct `.xmir` inputs begin at the Rust XMIR program reader and run structural verification, borrow checking,
  optimization, MIR → XLIL lowering, XLIL verification, and the same native backend.
- XHIR direct compilation deterministically rebuilds tuple and fixed-array layouts from higher-level function types. XMIR
  persists those layouts in its structured program-level `types` section because MIR values retain registry ids rather than
  source tuple field syntax. Both paths round-trip tuple calls and fixed-array operations into native `.xse` artifacts.
- Direct `.xlil` inputs are parsed and verified through the public XLIL C23 parser API. A supported local-target native
  input runs through LLVM lowering, module verification, the configured optimization pipeline, object emission, and the
  Clang/LLD `.xse` executable path.
- Plain source native builds support the first expression/local/call slice for top-level `fn main() -> Long` plus
  same-module helper functions with supported primitive signatures. A zero-parameter `Str` helper returning a string
  literal now crosses XHIR, target-independent UTF-16 MIR/XMIR, explicit-endian XLIL, LLVM IR, and native `.xse` linking.
  The existing local/control-flow slice covers helper functions with `Long`/`Bool` parameters and results: explicit
  `Long`/`Bool` local bindings, inferred `:=` bindings with i32-compatible or bool-compatible initializers, simple
  assignment to mutable `Long`/`Bool` locals, local
  identifier returns, direct helper calls,
  i32-range integer literals, unary `+`/`-`, `+`, `-`, `*`, `/`, `%`, `&`, `|`, `^`, `<<`, `>>`, and one top-level `if`
  expression with a bool literal, `Bool` local, direct same-module `Bool` helper call, unary `!`, or i32 comparison
  condition. `!=` and unary `!` can either swap branch targets for condition-only control flow or materialize as a `not.bool` value for supported `Bool` local
  initializers. Syntactically constant conditions, such as `false`, `!true`, or i32 literal comparisons like `1 < 2`, lower
  only the selected branch in this slice. This lowers through C MIR, XLIL, LLVM IR, object emission, and native `.xse`
  linking.
- Top-level generic functions with explicit concrete turbofish arguments are specialized by the Rust compiler core. Type
  substitutions apply to parameter, result, and local types; each distinct concrete argument list receives a deterministic
  monomorphized symbol before MIR/XLIL/LLVM lowering. The specialization worklist follows explicit generic calls nested in
  reachable generic templates and permits recursion of the same concrete instance. Polymorphic recursion that keeps
  changing the concrete type arguments is rejected. Interface constraints are checked against direct and transitive base
  relations. Type inference for omitted generic arguments and generic methods remain later semantic-analysis work.
- The Rust compiler-core route carries `Long` `/`, `%`, `&`, `|`, `^`, `<<`, and `>>` expressions through typed HIR,
  versioned XHIR, verified MIR/XMIR, and XLIL. Integer literals still default to `Int` (i64) without an expected type;
  an explicit `Long` (i32) return, binding, or parameter context is propagated through nested binary expressions.
- Unary `+` and `-` use the same expected-type propagation. A `Long` negation lowers to target-independent i32 MIR,
  while an `Int` negation lowers to i64 MIR. Logical `!` lowers as a typed `Bool` operation through XMIR and XLIL.
- Same-module `Int` functions now cross the compiler-core/native boundary as i64 declarations and bodies. Their division,
  remainder, bitwise AND/OR/XOR, signed shifts, and ordered comparisons have XMIR/XLIL records, verification, safe constant
  folding, and LLVM lowering. The native process entry contract remains `fn main() -> Long` and therefore returns i32.
- Source-native locals use C MIR local/place records rather than remaining SSA aliases. Their initialization and later
  reads/assignments lower to XLIL slots and LLVM stack operations. MIR validation permits one initialization store for an
  immutable local and rejects a second store as reassignment. A place load additionally requires initialization on every
  reachable path. The initialization check follows CFG reachability, so two mutually exclusive branch initializations are
  not treated as sequential reassignment. The current source-native slice
  also lowers statement-level `if`/`else if` blocks with one or more assignments in each branch and a merge before the
  final return.
  `Long` local assignments include `=`, arithmetic, and bitwise compound assignment forms already represented by the
  structural parser. The Rust compiler core carries these forms through typed HIR and MIR. Prefix increment/decrement
  returns the updated local value; postfix increment/decrement returns its prior value. Both perform one checked
  mutable-local store. These supported conditional assignment blocks may nest. A `while` with a supported Bool condition and
  assignment-only body lowers to a MIR loop header, body, and exit. Supported conditional and loop blocks also accept
  `Long`/`Bool` local declarations with lexical lifetime in the source-native context. `loop { ... }` lowers with a
  constant true condition through the same HIR/MIR CFG. `do { ... } while (condition);` is post-test syntax sugar: its
  body executes once before the first condition test and it uses the existing loop CFG
  rather than introducing a MIR or XLIL instruction. Inner scopes may shadow enclosing
  bindings; duplicates remain invalid within one lexical scope. `break` and `continue` target that loop's exit and header,
  including when written in a nested supported
  conditional. Supported early return expressions lower to direct MIR return terminators from nested conditionals and loop
  bodies. Classic `for` loops with a variable initializer, supported Bool condition, and supported assignment or postfix
  `++`/`--` update lower through distinct header/body/update/exit blocks; `continue` targets update and `break` targets exit.
  Statement-level `match` lowers supported `Long`/`Bool` selectors and literal arms into ordered MIR comparisons and
  branches with a required final `else` arm. Fixed-size built-in arrays support `for (value in values)` with inferred or
  explicit element bindings. The iterable is evaluated once and lowers to an index/length MIR CFG with checked array
  indexing; `continue` targets the generated update block and `break` targets the exit. For-each protocols beyond the
  built-in fixed-array path, arbitrary statement blocks, and complete CFG lowering remain deferred.
- Checked source builds emit official version-0 `.xhir`, `.xmir`, and `.xlil` program text through the Rust compiler-core
  session. Each output remains available only when its corresponding lowering stage succeeds.

### Lexer and structural AST

- Documented keywords, operators, line comments, and multiline text literals are tokenized. X# accepts `//`, `///`, and
  `//!` line comments. Each comment ends at its physical newline; multiline and nested comments are excluded so comments
  cannot split or hide source line boundaries.
- ASCII identifier rules are applied.
- Decimal integers, floating-point numbers, scientific notation, and `'` digit separators are validated.
- String and character literal source spellings are carried into AST literal nodes. Typed HIR resolves `Char` as one
  16-bit UTF-16 code unit; a literal may contain one BMP scalar directly or use a fixed-width escape for any code unit.
- The parser produces an arena-based structural AST.
- AST nodes carry full source location: file id, offset, line, and column.
- Declaration, type, statement, expression, pattern, and macro node families are represented.
- Outer `#[...]` attributes are parsed before declarations and declaration members into `XS_SYNTAX_ATTRIBUTE` nodes.
  File-level inner `#![...]` attributes are parsed before declarations. Attribute syntax is built in, but official X#
  attribute names live under `std::attrs::*`; semantic validation of each attribute remains a HIR/name-resolution step.
- In top-level and class-member contexts, `name!();` macro calls are represented as `XS_SYNTAX_DECL_MACRO_CALL` declaration
  nodes. This node is the entry point for item/declaration-producing macro expansion; inserting produced items as real AST
  replacements in scope is a later macro-expansion step.
- Named, generic, canonical `[T]` collection, `[T; N]` fixed array, `[K: V]` map, pointer, reference, tuple, unit, and
  `fn(...) -> T` function type nodes are parsed into
  the structural AST.
- Built-in array, set, and map types remain first-class compiler types across the structural-AST/compiler-core boundary
  and typed HIR; they are not aliases or desugarings of nominal standard-library collections. `[T] = [...]` creates an
  array, `[T] = {...}` creates a built-in set, and an uninitialized `[T]` defaults to array. There is no public
  `HashSet<T>` type spelling. A square-bracket literal inferred without a fixed-size context remains runtime-sized `[T]`;
  only an explicit `[T; N]` annotation selects a fixed layout. Contextual element checks, set XHIR round-tripping, numeric
  default fill, excess-element discard, and element assignment are implemented. Constant indices use composite extraction;
  calculated `Int` indices use checked MIR/XLIL array access records. Fixed arrays continue through XLIL `%aN`, the public
  C23 XLIL boundary, bounds-checked LLVM array access, and native `.xse` emission. Runtime-sized `[T]` construction,
  same-module parameter/return flow, checked indexing and mutation, `count`, and index-based `for` iteration use the same
  target-independent registry and now lower through LLVM to native `.xse`. Fixed-array `count`, `capacity`,
  `is_empty`, `start_index`, `end_index`, `first`, and `last` members are resolved in compiler-core and desugared to typed
  constants or existing checked index expressions. Runtime-sized storage currently uses hosted allocation and has no
  reclamation contract yet. Set runtime layout and operations, `ArrayList<T>` count-changing operations, and map
  value/layout lowering remain deferred.
- Positional and named tuple types, literals, and projections now remain structural through typed HIR and XHIR v1. Tuple
  layouts share the target-independent aggregate registry with nominal `data` values, then reuse MIR/XLIL `aggregate` and
  `extract` operations through LLVM structure lowering and native `.xse` emission. The same registry discovers nested tuple
  layouts in tuple and fixed-array element types. Tuple-valued parameters, returns, calls, direct element assignment, and
  chained positional projection use the same typed path. Fixed-array for-each supports recursively nested tuple binding
  patterns, typed tuple annotations, and `else` discard elements by desugaring them to explicit typed projections before
  MIR construction. General tuple destructuring declarations use the same desugaring for inferred or explicitly typed,
  nested patterns and continue through native code generation. Tuple match patterns remain a separate lowering step.
- The lexer keeps `>>` as a shift-right operator token; the structural parser may consume that token as two separate `>`
  tokens when closing generic type/generic parameter contexts.
- Type-qualified associated expressions such as `new Vector<Str>()`, expression turbofish, typed object literals,
  typed for-each patterns, tuple-pattern bindings, asynchronous function expressions, and unconditional `loop` statements
  have dedicated structural nodes. Every complete-language example under `Spec/Programs` is covered by both structural
  parsing and semantic `xs check` tests.
- HIR standard-library lookup has one registry for automatic types and constructors plus import-gated modules. It checks
  `Optional`, `Result`, `Error`, `Task`, common standard-library types/functions, user static associated functions,
  enum-data variants, and local names introduced by tuple or for-each patterns. This is semantic availability checking;
  native runtime implementations and nominal object layout are still incomplete.
- Lifetime spellings in reference types follow X# forms based on Rust lifetimes (`&'a T`, `&'a mut T`, `&'static T`,
  `&'else T`) and are carried into the AST as `XS_SYNTAX_LIFETIME` nodes. X# uses `else` where Rust examples often use
  `_`; lifetime elision and validation are left to the borrow-checker stage.
- Function parameters, return type, and function bodies are structural nodes; bodies are not stored as raw ranges. The
  `->` return type is marked with `XS_SYNTAX_FLAG_RETURN_TYPE`.
- `op Name(...) -> Type` is parsed as a distinct referentially transparent operation declaration. The current C23 AST stores
  it on the function-shaped declaration node with `XS_SYNTAX_FLAG_REFERENTIAL_TRANSPARENT`, but the semantic category is
  separate from ordinary `fn`.
- Top-level `extern "ABI" { fn ...; static ...; }` blocks are parsed as external declaration blocks. The structural AST
  stores the ABI string token on the block and on each contained function/static symbol. Functions are marked
  external/incomplete, and static foreign globals are marked external/static. HIR currently accepts the first explicit C ABI
  shape, `#[repr(C)] extern "C"`, and rejects unsupported ABI strings or missing/non-C representation attributes. Library
  resolution, symbol binding, and backend lowering remain HIR/backend work.
- Recoverable failures use the compiler-provided `Result<T, E>` enum data and postfix `@` propagation. Both payload types
  are unrestricted. The standard `Error` class remains available as a root for nominal application error classes.
  Compiler-core represents Result as a structured typed-HIR type rather than parsing its generic arguments from a nominal
  name. `Ok(value)` and `Error(value)` are checked against the enclosing expected Result type. The removed exception
  spellings are ordinary identifiers and no longer introduce declarations, statements, or control-flow regions.
- `data` declaration bodies accept fields, constructors, methods, and `fn operator <token>(...)` declarations. Data
  constructors and methods form overload sets by parameter type list; identical parameter type lists produce a parser
  diagnostic. Data destructors remain invalid.
- Class constructor names must match the class name. Constructors may be overloaded by parameter type list; duplicate
  parameter type lists produce parser diagnostics.
- Interface declaration bodies accept only body-less function declaration signatures.
- Nominal types use one `:` base list. Each structural base-specifier retains its access and virtual-inheritance modifiers.
  Classes accept multiple class bases and implemented interfaces. Interfaces inherit only interfaces, data types inherit
  only data types, and `enum data` types inherit only other `enum data` types; each category has no base-count limit.
  Payload-free `enum` declarations cannot inherit.
  Legacy `extends` and `implements` spellings produce parser diagnostics.
- Class fields may carry `getter` and `setter` property accessors. Accessors are represented as
  `XS_SYNTAX_PROPERTY_ACCESSOR` children; accessor bodies are parsed as ordinary blocks when present. Properties use the
  canonical X# field spelling, for example `public Name: Str { getter; setter; }`.
- Visibility modifiers are explicit: `public` is externally visible, `private` is limited to the declaring scope,
  `protected` is available to derived classes, and `internal` is limited to the current logical module. Current HIR
  symbol collection applies one default everywhere: declarations, members, and base entries are `internal` unless an
  explicit modifier says otherwise. Current HIR visibility
  checks enforce public, private, and internal module access; protected inheritance-aware access remains later
  member-resolution work.
- The C23 HIR expression checker performs the first property validation slice: duplicate accessors are rejected, getter
  return values are checked against the field type where that type is currently understood, setter bodies get an implicit
  immutable `value` binding of the field type, and `self.<property>` inside that property's own accessor is rejected as
  recursive. Actual property read/write lowering remains future MIR work.
- Body-less function declarations outside interfaces require `incomplete fn ...;`. An `incomplete fn` with a body produces a
  parser diagnostic.
- Regular enum variants cannot contain payload types and must have unique names. `enum data` requires at least one typed
  variant, rejects tuple payloads, and permits same-name typed variants only when their payload types differ. Constructor
  overload selection remains a later HIR type-checking responsibility. The compiler-provided generic families are
  `Optional<T> { Some: T, None }` and `Result<T, E> { Ok: T, Error: E }`. An `enum data` declaration may inherit an
  unspecialized standard family (`enum data Option : Optional`, `enum data MyResult : Result`); the derived declaration
  remains nominally usable as the base and inherits its variants and operations.
- The compiler core now preserves payload-free enum declarations and assigns deterministic declaration-order tags.
  Enum values remain nominal in HIR/XHIR and are not integer-convertible. MIR, XLIL, and LLVM use a target-independent
  one-field `{ i32 tag }` aggregate for the current native ABI. Variant construction, parameters, returns, direct calls,
  locals, equality, and statement-level `match` use this route. Payload-carrying `enum data` layout is separate work.
- Function expressions use inferred signatures: `fn(a, b) { a + b }` and `move fn() { work() }` are represented as
  `XS_SYNTAX_EXPR_FUNCTION` nodes. Parameter and return types are supplied by context rather than written on the lambda;
  `move` capture is a separate AST flag.
- Class construction uses `new Type(...)` and is represented as `XS_SYNTAX_EXPR_NEW` with an explicit type child. The
  otherwise-untyped `new()` spelling is accepted only inside the dedicated `else: expression;` discard form.
- Data object initialization uses normal object field literals, and field access uses ordinary member access.
- Postfix Result propagation syntax, `expression@`, is represented as `XS_SYNTAX_EXPR_RESULT_PROPAGATION`. The C23 HIR
  expression checker now requires an enclosing function whose return type is `Result<T, E>` or unit shorthand
  `Result<()>`. The compiler-core checker proves that the operand is a Result, yields its success payload type, and requires
  the operand and enclosing function to use the same error payload type. MIR lowering turns the operation into explicit
  discriminant extraction, success/error blocks, payload extraction, and an early Result return on the error edge.
- `if`, `for`, for-each, `while`, `match`, `return`, `break`, `continue`, and `else: expression;` are parsed. The `else:`
  statement explicitly discards its expression value, analogous
  to a Rust discard binding, but X# spells the discard/default position as `else`.
- Expression statements distinguish discarded and tail values: `expression;` is marked with `XS_SYNTAX_FLAG_DISCARDED`, while a
  final block expression without `;` stays value-producing and is the input for implicit-return desugaring.

### Module discovery and import graph

- Kotlin projects recursively discover configured-extension sources under `source.include` directory roots. Module source
  roots come from `module.include` or the explicit `--module` option.
- `xs.module.kts` assigns every selected module source to one case-sensitive logical module name; file paths never infer
  module identity. Direct and `members` entries belong to the named module, while `submodule` appends one path segment.
- `import` and `using` dependencies are resolved against project-assigned module names. Source-level `module` declarations
  have been removed; Kotlin projects transfer exact source paths and optional logical module names through the v2 project
  registry. The C23 symbol collector and Rust compiler-core session boundary both accept that external assignment.
- Missing import targets produce errors.
- Imported sources that are not listed in `addFiles` are added to the dependency graph and checked.

This layer lives under the HIR directory.

### HIR symbol collection

- The `xs check` flow runs HIR symbol collection after structural AST and macro validation.
- During project checking, direct sources and sources discovered through the import graph are collected into one shared HIR
  symbol table.
- Project metadata determines the module path. Namespace syntax is optional: one leading source-scoped namespace may
  establish a file base, brace-scoped namespaces may occur multiple times or nest, and their paths are restored when a
  block ends. There is no source-level `module` declaration.
- `public namespace` does not promote contained declarations; omitted declaration visibility remains `internal`.
- Top-level `fn`, `class`, `interface`, `enum`, `data`, and `macro_rules!` declarations are collected into the symbol table.
- `macro_rules!` rules use `matcher -> { expansion };`; the older `matcher: { expansion };` spelling is not accepted.
- `extern "ABI"` block functions are collected as function symbols, while `static` foreign globals are collected as
  `extern global` symbols. Contained declarations also default to `internal`; an outer visibility does not silently promote
  them.
- HIR CFFI validation runs after import resolution. Version 0 currently requires C ABI extern blocks to be written as
  `#[repr(C)] extern "C" { ... }`; this keeps the ABI/layout intent explicit before backend CFFI lowering exists.
- The same pass validates the first standard CFFI attribute shapes and scopes: block metadata such as `LinkLibrary` and
  `Header`, function metadata such as `LinkName` and unwind/thread markers, and static metadata such as `ThreadLocal`.
- `xs_hir_collect_symbols_expanded`, when given a declaration macro expansion set, collects synthetic declarations produced
  by `XS_SYNTAX_DECL_MACRO_CALL` reparse trees into the active HIR namespace of the macro call. Duplicate symbol checks use
  the same namespace rule as normal declarations.
- `xs_hir_collect_member_symbols` produces a separate HIR member symbol table for a class/interface owner symbol. V0 scope
  covers fields, field-like macro output, methods, constructors, destructors, and nested types.
- If methods repeat with the same name, the last declaration wins in lookup according to the X# method-merge rule; field and
  nested-member name conflicts produce diagnostics.
- Symbols carry short name, logical module name, namespace name, fully qualified name, visibility, source location, and
  source AST node.
- Duplicate top-level declarations with the same short name in the same namespace are errors.
- The same short name may be used under different namespaces.
- `import Module;` records that the module is usable through its qualified name. It does not place public symbols into the
  local import scope.
- Source syntax uses qualified paths: `Module::Name` and `Module::Type::Item` are namespace/type paths, while
  `value.member` remains value member access. Expression generic arguments use turbofish, for example
  `Factory::<Int>()`. HIR still stores canonical qualified names with `.` internally.
- `using Module::Name;` and `using Alias = Module::Name;` open one public top-level symbol into the local import scope.
- `using namespace Module;` opens public top-level symbols from that namespace into the local import scope.
- Non-public symbols are not opened through external module import.
- Qualified external names and types require a case-sensitive matching `import Module;` or `import Module::Namespace;`
  declaration unless they belong to the exact current logical module.
- Direct call targets in function bodies are resolved through same-namespace symbols and the import scope.
- Fully qualified call targets through another namespace/module resolve only to public symbols.
- Internal symbols resolve only inside their exact logical module. Private symbols additionally require the same namespace
  and source file.
- Calls where the first segment is a local parameter/variable are deferred to type checking.
- `value.Method()` calls on locals/parameters with explicit named type annotations are validated for member existence through
  the HIR member symbol table. At this stage the receiver type is resolved only for direct identifier receivers and named type
  annotations.
- After declaration macro expansion, the HIR member symbol table includes field-like variables and method declarations
  produced inside classes/interfaces in normal member declaration order. Macro-generated member names that conflict with
  original members produce diagnostics; only method-method same-name cases remain merge/overload candidates.
- `xs_hir_validate_name_uses_expanded`, when given a statement macro replacement set, traverses direct statement child lists
  through the `xs_macro_expand_child_statements` expanded view. This validates function/method call targets produced after
  macro expansion against the HIR symbol/import scope.

The Rust compiler-core path now performs exact parameter-type overload selection for top-level functions and `data`
methods. Instance `data` calls pass the receiver as an explicit target-independent HIR parameter; static methods use their
associated `Type::method` name. Deterministic hidden symbols keep overloads distinct through XHIR, XMIR, XLIL, and LLVM.
The C23 member table still provides early existence/visibility validation. Operator dispatch, generic constraint
membership, class/interface virtual dispatch, imported overload sets, and function values remain later stages.

### HIR type resolution bootstrap

- The `xs check` flow runs HIR type resolution after HIR import and name resolution.
- The current primitive type names are recognized: `Str`, `String`, `Bool`, `Byte`, `SByte`, `Char`, `Short`, `Long`,
  `Int`, `Integer`, `UShort`, `ULong`, `UInt`, `UInteger`, `SFloat`, `LFloat`, `Float`, and `Double`.
- `Bool` accepts `true`/`false` and explicitly contextual integer literals. It uses an unsigned 8-bit value
  representation; an unannotated integer literal still infers `Int`.
- `Byte` is an unsigned 8-bit primitive and `SByte` is a signed 8-bit primitive at the HIR level.
- `Char` is one Unicode scalar value represented as `u32`.
- Signed integer widths are `Short`/`Long`/`Int`/`Integer` = i16/i32/i64/i128; unsigned widths are
  `UShort`/`ULong`/`UInt`/`UInteger` = u16/u32/u64/u128. Floating widths are
  `SFloat`/`LFloat`/`Float`/`Double` = f16/f32/f64/f128. These widths are target-independent, and X# has no
  x86-32-specific primitive or native-width alias.
- `Str` is an immutable borrowed UTF-32 code-point view with an implicit static lifetime. String constants lower through
  MIR and XLIL as explicit `u32` code points.
- `String` is a distinct built-in owned, heap-backed UTF-32 string. It is not an alias or sugar for
  `Optional<Str>`, and no implicit borrow from `String` to `Str` is inserted.
- `Optional<T>` resolves as if the compiler had inserted `import optional; using namespace std::optional;`, making
  `std::optional::Optional<T>` available as `Optional<T>`. It is compiler-provided enum data with `Some: T` and
  payload-free `None` variants, made available through that implicit namespace using. The implemented `??`, `??=`,
  postfix `!`, and data-field `?.` slices use target-independent Optional aggregates in MIR. Coalescing and safe member
  access extract the discriminant, evaluate only the selected path, and merge through a MIR place. `??=` mutates only
  mutable local Optional bindings and leaves an existing `Some` untouched. Postfix `!` creates explicit success and
  failure blocks; the failure path becomes the ordinary MIR/XLIL panic terminator. Safe data-field access converts
  `Optional<Data>` to `Optional<FieldType>` without evaluating or extracting a payload on the `None` path. These paths
  reach XLIL, LLVM IR, object emission, and native `.xse` execution for supported payload types without introducing
  Optional-specific backend instructions.
  `Optional<T>` has explicit forced unboxing to `T` through postfix `!`; failed forced unboxing follows the compiler's
  panic path. `Optional<Str>` owns and boxes its
  string payload instead of carrying an optional borrowed/static `Str` reference. `T?` is source sugar for
  `Optional<T>`; an assigned non-optional value is wrapped in `Some`, while `nil` becomes `None`. Literal inference does
  not infer an Optional: `name := "Leitwolf";` infers `Str`. Optional method calls, chained safe access across arbitrary
  reference types, and full flow-sensitive Optional refinement remain later HIR work.
- The C23 HIR type resolver recognizes the standard wrapper type names `Optional<T>`, `std::optional::Optional<T>`,
  `std::result::Result<(), E>`, `std::result::Result<T, E>`, the special shorthand `Result<()>`, and the standard error type `Error`.
  The compiler treats the `std::result` namespace as implicitly usable, so `import result;` is optional for `Result<T, E>`,
  `Ok(...)`, and `Error(...)`. The only one-argument form is `Result<()>`, defaulting its error payload to `Error`;
  `Result<Int>` is rejected as incomplete. Rust compiler-core lowering preserves both payloads as
  `Type::Result { success, error }`, checks constructor payloads, and performs complete Result propagation lowering for the
  supported source-native expression subset.
- `Panic` is treated as an implicit standard import for the assertion and panic macro family. Those macros remain normal
  imported macros rather than built-ins; the macro validator treats the `Panic` module as always available. Panic is a
  macro-only module and HIR rejects `using namespace panic;`.
- `Stdio` is intentionally not prelude. `print!`, `println!`, `eprint!`, `eprintln!`, `format!`, and `std::fmt::*` require
  `import stdio;`. `format_args!` and `format_args_nl!` are compiler-special built-ins:
  they bypass `macro_rules!` resolution and cannot be declared or shadowed by user macros. `write!` and `writeln!` are
  built-in writer macros.
- Macros are not prelude entries. `#[MacroExport]` identifies a top-level module macro export; importing its module makes
  the macro available under its unqualified invocation name. Qualified macro invocation syntax does not exist. The
  standard-module registry and attribute placement checks are implemented; project-wide user macro export expansion
  across source files remains pending.
- The C23 HIR type resolver also recognizes the initial standard CFFI family: `std::cffi::CStr`, `std::cffi::CString`,
  `std::cffi::RawPtr<T>`, `std::cffi::NonNull<T>`, `std::cffi::Slice<T>`, `std::cffi::Handle<T>`, `std::cffi::Owned<T>`, `std::cffi::Borrowed<T>`,
  `std::cffi::Out<T>`, `std::cffi::DynamicLibrary`, `std::cffi::Symbol<T>`, `std::cffi::File`, and `std::cffi::VarArgs`.
- X# uses nominal typing. HIR type identity for user-defined types is based on name/symbol identity; identical structural
  shape does not imply compatibility.
- HIR primitive metadata carries XLIL type mappings for primitive types with documented runtime layout.
- `Str` is not mapped to an XLIL type yet because its runtime/ABI layout is incomplete.
- Type names inside functions, data, enum, class/interface members, and generic constraints are validated.
- Generic parameter names are recognized in their own declaration scope.
- User-defined `class`, `interface`, `enum`, and `data` types are resolved through the HIR symbol table and import scope.
- Fully qualified type uses through another namespace/module can resolve only to public type symbols.
- Internal type symbols resolve only inside their exact logical module. Private type symbols additionally require the
  same namespace and source file.
- Generic type uses must provide the same number of type arguments as the declaration’s generic parameter count.
- Because generic type erasure and default generic parameters are not supported, using generic types without arguments is an
  error.
- Duplicate generic parameter names in the same declaration scope are errors.
- Generic constraint types must resolve to interface symbols. For generic interface constraint uses, arity and type argument
  resolution are also performed during HIR type resolution.
- Generic parameters may carry multiple constraints. In a constraint list, `, Identifier :` starts a new generic parameter;
  otherwise the comma separates another constraint on the same parameter.
- `xs_hir_check_expression_types_with_macros` checks literal compatibility for variable initializers with explicit primitive
  type annotations and direct assignment literal RHS values to the same local. For now, integer, float, string, char, and
  bool literal kinds are matched against primitive target types. `return <literal>;` statements inside functions with
  explicit primitive return types are checked with the same literal compatibility logic. `None`, `new`, identifiers, calls,
  and other expression forms are deferred until general expression type inference is complete.
- The same HIR expression-check stage reports diagnostics for direct identifier assignment to `val`, `const`, and `static`
  immutable declarations inside local block/function scopes. Field, index, dereference, alias/borrow-based mutability rules
  are deferred to later borrow/type-check stages.
- `op` bodies are checked for the first referential-transparency slice. Calls, method calls, assignment, macro calls, `new`,
  `await`, result propagation, mutable borrow, and property accessors
  are rejected inside `op` until a fuller effect/purity model can classify them precisely.
- `xs_hir_resolve_types_expanded`, when given a statement macro replacement set, traverses direct statement child lists
  through the `xs_macro_expand_child_statements` expanded view. This validates type uses produced after macro expansion
  against the HIR symbol/import scope.

This C23 pre-check stage does not produce general expression type inference, overload selection, constraint
membership/compatibility checks, trait/interface compatibility, or ABI/layout decisions. The Rust compiler core later
checks interface membership for explicitly instantiated top-level generic functions; broader generic method and trait
compatibility remains incomplete.

The growing semantic-analysis and type-checking implementation now starts in the isolated Rust `xslang` crate instead of
adding new semantic rules to the old C23 HIR prototypes. The first checked Rust rule validates that `await` expressions occur
only inside async function bodies. `xslang` also carries the first Result propagation type rule: `Result<()>@` and
`Result<T, E>@` have success type `T`, require an enclosing Result return type with a compatible error channel, and remain
deferred at HIR-to-MIR lowering until error-return control-flow lowering exists. The first integration boundary is now
active in the C23 driver: the frontend flattens every macro-materialized AST into a versioned in-memory packet containing
fixed node records, a child-index table, and a text-byte arena. Rust `xslang` validates the ABI version, indices,
parent/child relationships, text ranges, and UTF-8 before creating an owned syntax tree in an opaque compiler-core session.
Each per-file session owns its imported tree. Project builds merge those trees into a program session before body lowering,
so signature collection runs across every selected source file while the C23 frontend retains source-specific diagnostics.
The first construction slice is active: module names and top-level function signatures are imported, parameters retain
their source spans, unit is represented explicitly, primitive type names are resolved, and other names remain nominal HIR
type references. Remaining function-body forms and complete semantic diagnostics still need to move onto this session model.
Supported single-block bodies now cross the next boundary as well: explicit returns and tail expressions containing
integer/boolean literals, local references, and the current arithmetic/comparison operator subset are built as Rust HIR,
type-checked, and lowered to target-independent Rust MIR. Unsupported body shapes remain deferred while their declarations
stay available to the session.
Explicitly typed and inferred local declarations, simple `=`, integer compound assignments, and prefix/postfix integer
updates are included in this body slice. Function parameters occupy the MIR parameter table and are immediately live values;
they no longer masquerade as uninitialized MIR locals. Function signatures are collected before body import, so direct calls
to functions in the same module resolve forward references, self recursion, and mutual recursion across selected source
files. Their argument and result types are recorded in typed HIR and lower through the existing target-independent MIR and
XLIL call models. Unit-returning calls carry an explicit HIR unit result and become result-free MIR/XLIL calls. A non-unit
call used as a semicolon-terminated expression is still evaluated, but its typed result is discarded. Top-level overloads
and instance/static `data` methods use exact parameter lists and deterministic hidden symbols. Generic calls, imported
overload sets, class/interface dispatch, operator calls, and function values remain later compiler-core work.
Top-level `data` and `class` declarations now enter the Rust declaration registry with nominal identity and typed fields.
Ordered `data` base lists retain their access and virtual-inheritance metadata in HIR/XHIR. Non-virtual `data` layouts
recursively place inherited fields before own fields in source base-list order; the same resolved field sequence drives
object checking, member paths, MIR places, scalar function parameters, aggregate registries, and native lowering. Multiple
and transitive data inheritance is executable; ambiguous inherited field names and invalid base graphs are rejected.
For the first executable aggregate slice, locally initialized `data` objects are recursively scalarized into
target-independent MIR storage places. Field reads, leaf assignments, nested aggregate replacement, and whole local copies
then lower through XLIL stack slots to LLVM. Non-recursive `data` parameters are flattened in declaration order to their
primitive leaves at the MIR/XLIL function boundary; calls accept either initialized places or object literals. Non-recursive
`data` return values use deterministic nominal aggregate registry entries and real MIR `aggregate`/`extract` statements.
Aggregate-returning calls can initialize local places, including nested `data` layouts. Overloaded constructors enforce
definite primitive-leaf initialization on every continuing path and return the completed aggregate. Constructor expressions
use the canonical `new Type(...)` form; the former `Type(...)` spelling is not treated as construction. Completed constructor
values may cross a `data` return boundary, be flattened directly into another function's scalar call ABI, or serve as the
receiver of a member projection without first being stored in a named source local. Non-mutating instance methods receive
the scalarized value, while static methods use the ordinary call ABI. Recursive by-value parameters and returns require a
future indirect ABI and are rejected explicitly. Escaping class objects, receiver mutation/reference ABI, class allocation,
operator dispatch, and a stable cross-module layout remain deferred.
Nominal member reads are not restricted to named local places. XHIR records an explicit owner, field name, result type,
and receiver expression for projections such as `make_point().position.x`. MIR resolves the inherited field order from the
nominal registry and emits target-independent aggregate extraction; XLIL and LLVM preserve the same nested value projection.
Assignment still requires an addressable local-rooted place.

The shared public C23 include tree now has an initial feature layer under `<xs/c23/*.h>`. `<xs/c23_features.h>` is its
umbrella header. The current `trait.h` and `impl.h` macros define object-safe vtable contracts, translation-unit-local
`impl ... for ...` bindings, erased trait objects, and checked C23 call syntax without compiler extensions. This is a small
foundation rather than a claim that every X# trait rule is already available to C callers.
The imported HIR body model now has explicit lexical blocks and distinguishes statement `if` from value-producing `if`.
Both forms require a `Bool` condition during Rust type checking. Statement branches lower to MIR basic blocks with an
explicit merge when control can continue. A directly returned `if` expression may lower each required value branch to its
own MIR return terminator. In all other value contexts, both branches write their typed tail values to compiler-owned MIR
storage and the merge block loads the selected result. This supports local initialization, assignment, call arguments, and
nested supported expressions without introducing a target-specific phi representation. The verified CFG lowers to XLIL
slots and `br_if` before entering LLVM.
`while`, post-test `do`/`while` sugar, `break`, and `continue` now cross the same compiler-core boundary. Rust HIR requires a `Bool` loop condition and
rejects loop jumps outside a loop. MIR lowering creates explicit header, body, and exit blocks; `continue` targets the
header and `break` targets the exit. The resulting target-independent CFG is verified and lowered to XLIL `br`/`br_if`
records before entering the existing backend path.
Statement `match` now crosses this boundary for checked `Long` and `Bool` selectors, literal patterns, and a final `else`
arm. HIR lowers each pattern to ordered MIR test/body blocks and a merge when an arm can continue. Source locals are stable
MIR storage locations: initializers and assignments emit `store.local`, reads emit `load.local`, and MIR-to-XLIL allocates
typed stack slots. This keeps mutations visible after conditionals, loops, and match arms before LLVM emits memory and branch
instructions.
Value-producing `match` expressions use the same ordered pattern CFG. Every arm must provide a tail value of the declared
result type. MIR writes those mutually exclusive values to compiler-owned target-independent storage, joins at a merge
block, and loads the result for its enclosing expression. XHIR v1 records the selector type, result type, arms, and value
blocks explicitly; LLVM-specific phi or ABI semantics do not leak into HIR or MIR.
For supported single-file and multi-file builds, this Rust path no longer stops at an internal MIR count. A program session
collects all function signatures before lowering any body, allowing same-module forward calls across files. It then
borrow-checks and optimizes each lowered function, lowers the combined module to XLIL v1 text, and exposes that text through
a borrowed C ABI view. The C23 driver parses it with the public `xs/lil.h` API, verifies the reconstructed module, and
reuses the established LLVM IR, object, and native `.xse` emission path. This is the only source-body compilation route;
unsupported source forms stop with a compiler diagnostic instead of entering a second fallback compiler. HIR and MIR
remain independent of LLVM.

The C23 HIR prototype mirrors the first parts of that rule: `@` is accepted inside functions returning
`Result<()>`/`Result<T, E>` and rejected elsewhere. When the operand is a direct same-file function call, the
callee must also return a Result type. Imported calls, method calls, and local function values remain later name-resolution
work; the Rust compiler-core route performs exact success/error compatibility checks for resolved same-module calls.

`@` is a surface-language sugar. In `xslang`, the Result desugar pass translates it into an explicit Result-match and
early-return intent model before MIR lowering. If a raw `ResultPropagation` expression reaches MIR lowering, that is treated
as a pipeline ordering error and is rejected instead of becoming a backend primitive.

For `xslang` propagation checking and desugaring, the single-argument `Result<()>` form uses standard `Error` as its error type.

The MIR lowerer has a separate `DesugaredFunction` entry point. Result-match intent from `@` lowers to ordinary
target-independent MIR:

- extract the one-bit Result discriminant;
- branch to a success edge and an error edge;
- extract the active payload in the selected edge;
- reconstruct the enclosing Result on the error edge and return immediately;
- continue with the success payload on the success edge.

There is no Result-specific XLIL opcode. Result values use a registered aggregate layout with a boolean tag, success
payload, and error payload. `aggregate`, `extract`, `br_if`, and `ret` carry the operation into LLVM. Inactive payload slots
receive a deterministic canonical value and are never observed on the opposite discriminant edge.

Source `match` accepts typed `Ok(binding)` and `Error(binding)` patterns. Those two variants together are exhaustive and do
not require a final `else` arm. A binding exists only within its arm block; spelling the payload as `else` discards it.
Duplicate Result variants, patterns on a non-Result selector, and payload-type disagreement are typed-HIR errors. MIR
extracts field 1 for `Ok` and field 2 for `Error`, then joins value-producing arms through compiler-owned storage.

XHIR text writing can emit desugared functions for inspection. The desugared form writes `result_match` records with explicit
success/error binding names and types. Surface XHIR match arms use explicit records such as
`arm result Ok binding value : Long`; this remains independent of LLVM spelling and ABI objects.

### Macro validation and scope resolution

- Macro matcher variables, repetition depths, and expansion variables are validated.
- Direct and indirect macro recursion in the same scope is an error.
- Macro definitions are collected for the relevant scope before expansion.
- Macro fragment/token matcher helpers are separated into `xs/sources/macro/fragment.c`; validation, expansion traversal, and
  future fragment reparse support must grow without bloating `xs/sources/macro/expansion.c`.
- Macro calls before textual definition in the same scope are accepted.
- Macros defined in an inner scope cannot be called from an outer scope.
- Calls are checked to resolve to a visible macro definition.
- `include!`, `format_args!`, `format_args_nl!`, `write!`, and `writeln!` are built-in macros. `format_args!` and
  `format_args_nl!` are compiler-special syntax rather than `macro_rules!` definitions. They use the same format string
  validation as the Stdio output macros where applicable.
- Imported `Stdio` macros are treated as external macros, not built-ins. The validator recognizes `print!`, `println!`,
  `eprint!`, `eprintln!`, and `format!` through `import stdio`. `println!()` and `eprintln!()`
  accept the newline-only form. Built-in `writeln!(destination)` accepts the destination-only newline form. Other
  formatting forms require a string literal format template and matching placeholder argument count. Debug, pretty-debug,
  hexadecimal, padding, and alignment specs such as `{:?}`, `{:#?}`, `{:08x}`, and `{:_>8}` are accepted by the validator.
- The formatting grammar and the `print!`, `println!`, `eprint!`, `eprintln!`, and `format!` expansion structures are
  versioned against Rust 1.57. Newer implicit-capture syntax is not part of this contract. `format_args_nl!` is the
  source-callable built-in newline-format variant used by line macros.
- Full token matcher rules and empty matcher rules can be matched.
- Single-token fragment matchers that can be validated precisely (`tt`, `ident`, `literal`, `lifetime`, and `vis`) are matched
  against call tokens.
- The macro expansion preparation API is `xs_macro_prepare_expansion`. It resolves calls in scope, counts calls that can be
  expanded without structural reparsing through single-token fragments or full-token matchers, produces a simple expansion
  token/substitution plan, and intentionally defers `meta` fragments.
- V0 support for `expr`, `stmt`, `block`, `ty`, `path`, `item`, and `pat` fragments captures the call-parentheses token
  sequence as a single expression/statement/block/type/path/item/pattern fragment when the matcher contains exactly one
  remaining fragment. `$name` use in expansion carries that token sequence to statement or declaration reparse.
- `xs_macro_expand_tokens` produces call span and expanded token lists for simple supported calls. A macro call must match
  exactly one rule; zero matches and multiple matches are validation errors. This output is not written back into the
  structural AST yet; it is an intermediate flow for later fragment-level reparse and AST replacement.
- `xs_macro_reparse_expansion_as_statement` reparses a supported expansion token list as a statement inside a synthetic
  function body. This is a bridge for fragment-level reparse and AST replacement, not final macro-call replacement.
- `xs_macro_reparse_result_statement` extracts the replacement statement node from the reparsed synthetic body. Parent-child
  AST replacement remains a later step.
- `xs_macro_expand_statements` reparses supported token expansions as statements and maps call span to replacement statement
  nodes inside `XsMacroStatementExpansionSet`. The set owns the synthetic reparse trees, so replacement nodes remain valid
  until the set is freed. This API only produces statement replacements for statement-context macro calls; nested macro calls
  inside other expressions remain token-level expansions.
- `xs_macro_statement_expansion_find` returns the synthetic replacement statement node for an `XS_SYNTAX_STMT_MACRO_CALL`.
  HIR consumers do not keep their own span mapping code for replacement lookup.
- `xs_macro_expand_declarations` reparses supported token expansions from declaration-context `XS_SYNTAX_DECL_MACRO_CALL`
  nodes as synthetic source files and stores call span, reparse tree ownership, and produced declaration count inside
  `XsMacroDeclarationExpansionSet`. `xs_macro_declaration_expansion_find` returns the synthetic declaration expansion record
  for a declaration macro call node.
- `xs_macro_expand_top_level_declarations` produces a structural expanded view for a top-level declaration list. The view
  preserves original declaration nodes, materializes `XS_SYNTAX_DECL_MACRO_CALL` nodes through synthetic declaration
  expansion records with matching call spans, and provides a testable expanded-AST bridge until parent-child AST rewrite is
  complete.
- `xs_macro_expand_child_declarations` provides the same expanded-view mechanism for the direct children of any parent
  declaration. HIR type resolution traverses function members and field-like variable declarations produced by declaration
  macro calls inside `class` and `interface` through this view.
- `xs_macro_expand_child_statements` produces a structural expanded view for direct statement children of nodes such as
  statement blocks. The view preserves original statement nodes and materializes `XS_SYNTAX_STMT_MACRO_CALL` nodes through
  synthetic replacement statement records with matching call spans in statement order. This API does not physically rewrite
  parent-child AST; it is an intermediate bridge shared by HIR/MIR passes that consume statement macro replacement.
- `xs_macro_materialize_expanded_tree` clones an expanded tree into a separate syntax tree, replacing macro-call
  declaration/statement nodes with replacements from declaration and statement expansion sets. It does not mutate the
  original AST; replacement text/span references in the materialized tree are tied to the expansion set lifetime because the
  expansion sets own the reparse source text.
- HIR symbol collection operates through the top-level expanded declaration view. Declaration macro calls must have one
  matching rule before their replacement declarations enter the normal declaration flow.
- The `xs check` flow runs macro validation, macro expansion preparation, and statement expansion set generation before HIR
  symbol collection. The driver keeps the replacement set alive for the compilation unit and passes it to HIR name-use and
  HIR type-resolution traversals. HIR name and type resolution traverse statement child lists through the expanded statement
  view, so each validated statement macro replacement enters the normal statement flow.

Declaration/item-context macro-call AST input, declaration reparse set generation, top-level/child declaration expanded views,
statement expanded view, separate expanded tree materialization, HIR symbol-collection integration, and class/interface
function/field-like member expansion wired into HIR type traversal all exist. Writing generated declaration/statement nodes
back as in-place parent-child replacements on the main AST and reclassifying field-like declarations as `XS_SYNTAX_CLASS_FIELD`
remain next steps.

`meta` fragment capture and full AST expansion are still incomplete. `expr`, `stmt`, `block`, `ty`, `path`, `item`, and
`pat` fragment support is currently limited to a single token sequence. Unsupported fragment matchers do not invent
semantics.

### MIR model

- `xs/mir.h` exposes a C23 API for modules, function declarations, function definitions, basic blocks, and terminators.
- MIR function definitions carry a basic block list.
- MIR function definitions carry a local table; local kind, type, mutability, and name are stored.
- The MIR place model starts with a root local plus a `field`/`deref`/`index` projection chain.
- MIR has an SSA value table and core `const.i64`, `const.bool`, signed i64 arithmetic/bitwise/shift/comparison
  instructions, `load`, and `store` instructions.
- Each basic block may currently have a `return`, `goto`, `branch_if`, `panic`, or `unreachable` terminator in the Rust MIR
  model. The C MIR model also distinguishes `panic` from structurally unreachable control flow.
- The MIR text writer deterministically writes declarations and functions with bodies.
- `xs/mir/borrow_checker.h` contains the first MIR validation/borrow-check skeleton.
- The borrow-checker skeleton validates mandatory terminators, return type compatibility, and `store` operations into
  immutable local roots.
- The borrow checker also validates instruction result/value ids, `load`/`store` place ids, `goto`/`branch_if` targets,
  `branch_if` condition liveness/type, i64 arithmetic/bitwise/shift operand type/id consistency, and i64 comparison
  i64-to-bool result consistency.
- `xs/mir/optimizer.h` contains the initial MIR optimization API.
- The CFG cleanup pass removes blocks unreachable from the entry block and rewrites remaining block ids plus `goto`/`branch`
  targets.
- Constant folding lowers fixed-width integer arithmetic, bitwise, and shift instructions with two matching constants to
  a constant result, and lowers fixed-width comparisons to a `const.bool` result. Arithmetic wraps to the declared width;
  division by zero, signed minimum divided by `-1`, and out-of-range shifts remain unchanged instead of being folded.
- Constant folding also lowers a MIR `branch_if` whose condition resolves to a known `const.bool` into a direct `goto`;
  CFG cleanup can then remove the dead target block.
- Source-native builds run C MIR borrow checking, constant folding, CFG cleanup, and a second borrow-check pass before
  lowering MIR to XLIL.
- Rust `xslang` also contains a target-independent MIR structural verifier for duplicate local/block ids, missing
  terminators, unknown local references, and unknown block targets. This verifier is separate from LLVM and runs before
  borrow-check-specific reasoning.
- Rust `xslang` optimizer exposes `optimize_verified_function`, which validates MIR before optimization and verifies the
  optimized MIR before returning it.
- Rust `xslang` XMIR text support can write and parse optimizer analysis records for optimization pass reports.
- Rust `xslang` XMIR text support can also write and parse structural verifier diagnostic analysis records.
- Rust `xslang` MIR and XLIL models now carry `add.i64`, `sub.i64`, `mul.i64`, `const.bool`, and `eq.i64`. XMIR and XLIL
  text parsers/writers round-trip them, the MIR verifier checks operand/result types, MIR → XLIL lowering emits matching
  XLIL instructions, and the Rust MIR optimizer folds arithmetic and equality comparisons when operands are known
  `const.i64` values.
- Rust `xslang` MIR and XLIL models also carry the first conditional control-flow primitive. MIR writes/parses
  `branch_if` records with `condition local N`, `then block A`, and `else block B`; XLIL writes/parses assembly-like
  `br_if %rN, bbA, bbB` terminators. Verifiers require a `bool` condition and existing target blocks. HIR lowering does not
  emit conditional control-flow yet. The MIR optimizer can fold same-block constant `branch_if` conditions to `goto`.
- Rust `xslang` lowers context-typed literals for the complete fixed-width integer family into MIR and XLIL records.
  Positive values are range-checked against their declared type; negative literal forms cover every signed width,
  including the exact i8/i16/i32/i64/i128 minimum values. XMIR keeps semantic decimal values while XLIL uses exact
  fixed-width hexadecimal bit patterns for the newly added widths.
- Fixed-width integer operations use one target-independent typed operation model across MIR and XLIL. Signedness controls
  division, remainder, right shift, and ordered comparisons; comparisons produce `bool`. The text readers, writers,
  verifiers, optimizer, MIR-to-XLIL lowering, public C23 XLIL model, and LLVM lowering share this contract.
- The same Rust bridge lowers `Long` division, remainder, bitwise AND/OR/XOR, and signed shifts to target-independent
  `div.i32`, `rem.i32`, `and.i32`, `or.i32`, `xor.i32`, `shl.i32`, and arithmetic `shr.i32` records. Their model,
  XHIR/XMIR/XLIL text readers and writers, verifiers, MIR-to-XLIL lowering, and native compiler-core bridge are tested
  together. `Int` keeps its i64 representation and is not silently narrowed to this i32 instruction family.
  Unsupported HIR expressions and primitive values whose runtime layout is not ready, such as `Str`, produce lowering
  diagnostics instead of inventing temporary backend semantics.

This stage does not yet produce complete statement/expression lowering, the full instruction set, async
state machine generation, region/loan/move analysis, drop-point validation, or a comprehensive MIR optimization pass set.

### X Platform boundary

- The former experimental XGC model no longer lives in `xslang`. Managed-heap work belongs to the independent X Platform
  runtime as XPG.
- XPI is a language-neutral register IL below language frontends. X# can lower `XLIL → XPI`; XPI itself does not inherit
  X# nominal types, strings, ownership rules, or source semantics.
- The current X# compiler remains on its ownership-oriented LLVM path. XPLR bytecode and XPJ are not implemented here.

### LLVM backend infrastructure

- A separate `xs_backend_llvm` library exists outside the frontend.
- LLVM context, target machine, target triple, and data layout are managed.
- A separate LLVM module is created for each codegen unit.
- Documented numeric primitive types map to LLVM types.
- Body-less function declarations and function signature lowering are supported.
- XLIL function declaration signatures lower through the XLIL type vocabulary rather than through HIR primitive types.
- The public `xs/lil.h` producer surface can build every implemented XLIL v1 registry/body record, verify the completed
  opaque model, emit caller-owned canonical text without exposing `FILE *` across an FFI boundary, parse it back, and
  inspect typed instruction payloads. Its optional insertion-point builder infers common operand and registry types, while
  stable opaque handles, an API-version query, exported-symbol annotations, and the installed shared `xs_lil` library
  form the FFI boundary for official bindings.
- LLVM optimization pipelines from `default<O0>` through `default<O3>` can be configured.
- LLVM module verification, LLVM IR text emission, and object file emission work.
- `xs build --xlil -file <input.xlil>` parses and verifies XLIL v1 text through `xs_lil_module_parse_text`, then lowers to
  LLVM IR, verifies and optimizes the LLVM module, and emits an object file and native `.xse` executable beside the input.
  Native direct XLIL requires exactly one defined
  `.func main : () -> i32`; its supported body subset includes `.param`, all fixed-width integer constants and operations,
  `const.f32`,
  `const.f64`, `const.bool`,
  `add.i32`, `sub.i32`, `mul.i32`, `div.i32`, `rem.i32`, `and.i32`, `or.i32`, `shl.i32`, `shr.i32`, `eq.i32`,
  `ne.i32`, `lt.i32`, `le.i32`, `gt.i32`, `ge.i32`, `not.bool`, signed i64 arithmetic/bitwise/shift/comparison
  instructions, typed `.slot`/`load`/`store`, `call`, `br`, `br_if`, `panic`, `ret`, and `ret %rN`.
- `xs build -file <input.xs>` and argument-free `xs build` use the same native path for supported compiler-core
  sessions. Context-typed literals, parameters, locals, direct calls, and returns preserve every fixed integer width;
  `main` remains `Long`. The broader expression slice includes `Long`/`Bool` mutable locals,
  unary `+`/`-`,
  arithmetic, bitwise including `^`, shift, short-circuit `&&`/`||`, and top-level `if` expressions. Logical expressions
  become MIR control-flow branches and XLIL `br_if` records; their right operand is not evaluated on the short path.
  Supported `:=` initializers infer types from expressions as well as literals. Prefix/postfix increment and decrement retain
  their distinct result values in typed HIR and lower through MIR storage loads/stores. Compound `/=`, `%=`, `&=`, `|=`,
  and `^=` assignments also use the Rust compiler-core path. The resulting XLIL reuses the native LLVM builder.
- Direct executable linking uses the configured Clang driver with LLD for the native Linux ELF target. A configured
  cross-target still receives LLVM IR and object artifacts, then stops before executable linking; runtime and external
  library linking remain unconfigured.
- `.xse` is the native executable artifact extension. The first output format under that extension is ELF; PE support comes
  later.
- The linker can be invoked without a shell, with argument policy left to the upper layer.

Details: [LLVM_BACKEND.md](LLVM_BACKEND.md)

### XLIL target

- `docs/XLIL.md` defines XLIL as the official low-level intermediate language for X#.
- `.xhir`, `.xmir`, and `.xlil` are the extensions for HIR, MIR, and XLIL code. They are human-readable text formats, not
  binary or opaque serialized compiler state.
- Their leading `version N` records are compiler input contracts for `xs build`; they select the supported XHIR/XMIR/XLIL
  text grammar version to parse. Canonical writers emit version `1`, while readers accept versions `0` and `1`; unknown
  later versions remain explicit errors.
- `.xhir` and `.xmir` are intended for direct developer inspection, code review, regression fixtures, and debugging. Their
  official record grammar is not fully documented yet, but future grammar changes must keep that human-readable property.
- `.xhir` and `.xmir` are not assembly-like. XHIR should expose resolved semantic structure; XMIR should expose structured
  control-flow, places, drops, regions, and analysis results. See [XHIR.md](XHIR.md) and [XMIR.md](XMIR.md).
- XLIL is the target-independent type/data vocabulary that HIR/MIR depend on.
- XLIL is designed to sit before LLVM IR and act as the common input point for backends.
- `.xlil` is always a text registry format; no binary XLIL format will be added.
- XLIL is not as high-level as CLR and not as low-level as assembly; it is an assembly-like but distinct
  target-independent mid/low-level registry language.
- XLIL is not bytecode or a virtual-machine format.
- The stable XLIL registry/generation C23 API target is documented as `#include <xs/lil.h>`.
- The XLIL AOT C23 API target is separated as `#include <xs/lil/aot.h>`; until concrete object/link behavior exists, it only
  marks the planned public surface.
- External frontends and tools can produce the supported XLIL v1 subset through the public C23 model and use the LLVM
  backend pipeline to produce native executables.
- Third-party languages can generate XLIL through `xs/lil.h`; XLIL AOT, HIR baseline JIT, and MIR performance JIT are planned
  as separate public headers: `xs/lil/aot.h`, `xs/hir/jit.h`, and `xs/mir/jit.h`.
- Direct file compilation entries are recognized as `xs build --output hir|mir|xlil -file <input>` and
  `xs build --hir|--mir|--xlil -file <input>`. The commands are not fully implemented yet.
- `xs/lil.h` contains target-independent core APIs for XLIL modules, verification, primitive types, text parsing/writing,
  read-only function/body inspection, function bodies, basic blocks, parameters, all fixed-width integer constants, exact-bit
  `const.f32`/`const.f64`, `const.bool`, UTF-16 `eq.str`/`ne.str`,
  typed stack slots and `load`/`store`, i32 arithmetic/bitwise/shift/comparison, i64 arithmetic/bitwise/shift/comparison,
  aggregate and fixed-array type registries, `aggregate`, `array`, `extract`, `extract.array`, `call`, `br`, `br_if`,
  and `return`.
- The XLIL text writer emits assembly-like registry records: `.xlil version 1`, `.xlil module`, `.extern`, `.func`,
  `.type %tN Name : (...)`, `.array %aN : T x N`, `.slot %sN:type`, `bbN.label:`, typed SSA instructions, composite
  construction/extraction,
  `store %rN, %sA`, `%rN:type = load %sA`, `br bbN`,
  `br_if %rN, bbA, bbB`, `panic`, `ret`, and `.end`.
- MIR parameters carry an explicit immutable local id plus XLIL-vocabulary type, allowing the first MIR → XLIL body bridge
  to bind them to XLIL `.param` values. The bridge lowers MIR
  parameter signatures, typed MIR fixed-width integer constants, `const.f32`, `const.f64`, `const.bool`, i64 arithmetic/bitwise/shift/comparison local
  statements,
  typed call statements whose arguments already have lowered XLIL values, UTF-16 `Str` equality/inequality,
  unconditional `goto`, conditional `branch_if`
  with an already-lowered `bool` condition, and matching local return values to XLIL signatures, `const`, arithmetic,
  compare, `call`, `br`, `br_if`, `panic`, and `ret %rN` records. C MIR `panic` lowers to the XLIL terminator; LLVM then
  emits `llvm.trap` and `unreachable`.
- C MIR root-local places lower to typed XLIL stack slots. Root-place loads and stores lower through XLIL and LLVM
  `alloca`/`load`/`store`; field, dereference, and index projections remain deferred until their layout rules are available.
- The C `xs/mono/plan.h` API still plans already-concrete MIR functions. Rust compiler-core lowering now also creates the
  transitively reachable explicit generic-function instances and feeds their stable symbols into the existing native
  pipeline. Generic type instantiation and inferred generic arguments remain incomplete.
- `xs/codegen/units.h` contains a target-independent codegen-unit planning API. MIR functions are split into module-path
  based codegen units by the default v0 policy. When produced from a mono plan, the unit name comes from the source module
  path and the function name comes from the stable monomorphized symbol.
- Rust `xslang` contains the first XHIR/XMIR structured text writer and header parser bootstrap. These outputs are
  intentionally separate from XLIL's assembly-like registry records.
- Rust `xslang` XHIR text support can write and parse type-check diagnostic analysis records.

XLIL instruction set, function body model, runtime/ABI layout, and MIR → XLIL body lowering are implemented incrementally.
Public XLIL behavior is documented in [XLIL.md](XLIL.md).

## Unfinished stages

The public roadmap is summarized in [TODO.md](TODO.md). Remaining work is implemented incrementally while keeping documented
syntax, public APIs, and the current architecture stable.

- Macro fragment matcher engine and AST macro expansion
- HIR method and operator resolution
- Type, function-call, generic, and trait/interface dependency edges beyond module/import
- Complete expression type inference and generic method constraint validation
- Send, Sync, mutability, and async/await validation
- Complete MIR statement/expression lowering and async state machine generation
- Borrow checker and drop-point validation
- MIR optimizations
- Monomorphization, codegen unit splitting, and incremental compilation cache
- XLIL function body data model and MIR → XLIL body lowering
- XLIL to LLVM IR function body lowering
- LLVM mapping for `Str`, whose runtime/ABI layout is not fully implemented
- Linking generated object files according to project targets
- End-to-end source-project `xs build` and `xs run`

Temporary language rules are not invented in the parser or LLVM backend for unfinished semantic stages.

## Build and test

```text
cmake --preset clang-debug
cmake --build --preset clang-debug
ctest --preset clang-debug
```

To check the example project:

```text
cd tests/fixtures/projects/native_call
../../../../build/clang-debug/xs check
```
