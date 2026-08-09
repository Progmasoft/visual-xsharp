<!--
SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
SPDX-License-Identifier: MPL-2.0
-->

# Visual X# specification examples

The `Spec/` tree is the public language-example area for Visual X#. It is intentionally example-driven: files show surface syntax,
semantic intent, valid and invalid shapes, and larger complete-language sketches.

The compiler is not expected to compile every example today. Examples may describe the completed language, but documented
syntax takes priority over ad-hoc implementation shortcuts.

## Language stability

The documented Visual X# syntax and semantics are frozen. Compiler work implements that contract rather than redesigning it.
Clarifications and defect fixes must preserve source meaning; intermediate-format and backend evolution remains
independently versioned through XHIR, XMIR, and XLIL headers.

## How to read `Spec/`

- `Spec/TypeSystem.xs` covers primitive names, nominal typing, `Optional<T>`, and standard wrapper behavior.
- `Spec/Variables.xs`, `Spec/Functions.xs`, and `Spec/ControlFlow.xs` cover everyday local/function/control-flow syntax.
- `Spec/Macros.xs`, `Spec/Attrs.xs`, `Spec/Stdio.xs`, and `Spec/Panic.xs` cover macro behavior, attributes, and imported
  macro modules.
- `Spec/Data.xs`, `Spec/Enum.xs`, `Spec/Classes.xs`, and `Spec/Generics.xs` cover declaration forms.
- `Spec/Modules.xs`, `Spec/SearchModule.xs`, and `Spec/Tuples.xs` cover project-assigned modules and built-in tuple forms.
- `Spec/FS.xs`, `Spec/Http.xs`, `Spec/Process.xs`, `Spec/Sync.xs`, `Spec/Thread.xs`, and `Spec/CFFI.xs` describe
  standard-library-facing examples.
- `Spec/Programs/` contains larger one-file programs written as if the completed language and standard library existed.

## Current example conventions

- Macro names are snake_case and are not under `STD`.
- Standard modules use `std::<module>` names, such as `std::fs`. Built-in collections use language type syntax and do not
  require a standard-module import: `[T]`, `[T; N]`, and `[K: V]`. A square-bracket initializer makes `[T]` an array; a
  brace initializer makes it a built-in set. Without an initializer, `[T]` is an array. No public `HashSet<T>` type exists.
  Fixed-array value properties use snake_case where needed: `count`, `capacity`, `is_empty`, `start_index`, `end_index`,
  `first`, and `last`.
- `::` separates modules, namespaces, types, associated items, and
  constructors; `.` is reserved for value member access and method calls. Generic arguments in expression paths use
  turbofish, for example `Some::<Str>("value")`.
- `.xs` has no `module` keyword. The project runtime bundled with `xs` evaluates `xs.project.kts` plus optional
  `xs.module.kts` metadata and passes
  logical module membership to the native compiler. Module names are case-sensitive: `name("Math")` is imported as
  `import Math;`, not `import math;`. `add(...)` accepts concrete paths or globs, and `submodule` creates a qualified
  child module. `PascalCase` source directories and `snake_case.xs` files are canonical but not mandatory.
- Namespace declarations are optional. A file may have one leading source-scoped `namespace path;`, any number of
  brace-scoped `namespace path { ... }` declarations, or both in that order. Brace-scoped namespaces may nest.
- Instance members use `self` as the receiver name inside constructors, methods, and property accessors.
- Constructor calls always use `new Type(...)`. `Type::new(...)` is not constructor syntax; `::` remains available for
  ordinary associated/static members.
- Property bodies should use explicit backing fields such as `self._age`; referencing the property itself as `self.age`
  inside its own accessor is recursive and rejected.
- Visual X# evaluation is strict/eager. The language aims for referential transparency where practical, but standard-library APIs
  are not globally guaranteed to be referentially transparent.
- `fn Name(...) -> Type` declares an ordinary function. `op Name(...) -> Type` declares a referentially transparent
  operation for pure computation; operations are not just aliases for functions, and non-transparent operation bodies are
  compile-time errors.
- `import Module;` makes a module usable through its qualified name. It does not place the module's public symbols in
  local scope. For example, `import fs;` keeps file APIs qualified as `std::fs::File::open(...)`; users may add
  `using namespace std::fs;` when they want `File::open(...)`. Use `using Module::Name;`, `using Alias = Module::Name;`,
  or `using namespace Module;` when short names are desired. `using Module::*;` is invalid; both
  `using namespace Module;` and `using namespace Module::*;` are valid. The macro-only `panic` module cannot be opened
  with `using namespace`.
- `include!`, `format_args!`, `format_args_nl!`, `write!`, and `writeln!` are built-in macros. `format_args!` and
  `format_args_nl!` are compiler-special and cannot be declared or shadowed through `macro_rules!`.
- `print!`, `println!`, `eprint!`, `eprintln!`, `format!`, and the `std::fmt::*` API come from `Stdio`.
- Attribute delimiter syntax is built in: `#[...]` applies to the following declaration/member, and `#![...]` applies to the
  enclosing file/module form. Official Visual X# attributes live under `std::attrs::*`; the compiler brings those names into
  attribute scope automatically, so `import attrs;` is optional.
- CFFI is explicit. The standard CFFI module is not automatically imported or placed in scope; source uses `import cffi;`
  before C foreign-function declarations and the canonical helper types under `std::cffi::*`.
- `Optional<T>` is available as if the compiler had inserted `import optional; using namespace std::optional;`, making
  `std::optional::Optional<T>` available as `Optional<T>`.
- `Str` is an immutable borrowed UTF-32 code-point view with an implicit static lifetime. `String` is a distinct
  built-in owned, heap-backed UTF-32 string; it is not `Optional<Str>` sugar and does not implicitly borrow as `Str`.
  `Optional<Str>` and `Optional<String>` are distinct types, while `:=` string-literal inference produces `Str`.
- Visual X# uses `else` for placeholder/default positions. Type and lifetime placeholders are written as `Type<else, Foo>` and
  `&'else T`; `_` placeholders are not canonical Visual X# syntax.
- Statement/expression separation follows Rust: `expression;` evaluates and discards the value, while the final expression
  in a block may omit `;` and becomes the block value. Function tail expressions are desugared as implicit returns when the
  function return type expects a value.
- Parentheses around `if`, `else if`, `while`, `do ... while`, `for`, and `match` control values are optional. Both surface
  forms normalize to the same structural AST; omitted parentheses do not change precedence or evaluation order.
- Positional tuples use `(T, U)`/`(left, right)` and compile-time `.0` member access. Named tuples use
  `(name: T, age: U)`/`(name: value, age: value)` and named member access. A trailing comma distinguishes a one-element
  tuple from a grouped expression.
- `Optional<T>` is compiler-provided enum data with `Some: T` and payload-free `None` variants; the implicit namespace
  using makes both constructors available in source.
- `Result` is a special standard namespace: `import result;` is optional, and the compiler behaves as if
  `using namespace std::result;` existed for `Result<()>`, `Result<T, E>`, `Ok(...)`, and `Error(...)`. `Result<T, E>` is
  compiler-provided enum data and both payload types are unrestricted. Most other `std::*` modules are not automatically
  placed in local scope. The only single-argument form is `Result<()>`, whose error payload defaults to standard `Error`;
  forms such as `Result<Int>` are incomplete. Result is not a default function return: a function must declare it when its body uses `@`, `Ok(...)`, or
  `Error(...)`.
- `Panic` is also an implicit standard import for assertion and panic macros. `assert!`, `assert_eq!`, `assert_ne!`,
  `debug_assert!`, `debug_assert_eq!`, and `panic!` are available without an explicit import, but they are still library
  macros rather than compiler built-ins. Panic is macro-only, so `using namespace panic;` is invalid.
- `Stdio` is not prelude and is not implicit. Its `print!`, `println!`, `eprint!`, `eprintln!`, and `format!` macros and
  `std::fmt::*` API require `import stdio;`. A `using namespace` declaration does not import a module. The writer and
  formatting-argument built-ins do not require Stdio.
- Macros are not prelude entries. A top-level macro marked `#[MacroExport]` enters an imported module's macro export
  registry and is invoked by its unqualified name; Visual X# has no qualified macro-call syntax.
- Recoverable failures use `Result<()>`/`Result<T, E>` plus postfix `@` propagation. Exception declarations and
  control-flow syntax are not part of Visual X#.

## Program examples

Each file in `Spec/Programs/` contains one example program. They are design fixtures for syntax, APIs, and semantic intent,
not compatibility promises for the current partial compiler implementation.

The current set intentionally covers different language surfaces:

- CLI/file workflows: `TodoCli.xs`, `FileBackup.xs`, `StaticSiteReport.xs`, `LogAggregator.xs`
- Data-heavy services: `BankLedger.xs`, `InventoryService.xs`, `CsvAnalytics.xs`
- Network and async sketches: `ChatServer.xs`, `HttpHealthMonitor.xs`, `SensorStream.xs`, `JobQueue.xs`
- Package/tooling flows: `PackageResolver.xs`, `PluginPipeline.xs`
