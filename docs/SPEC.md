<!--
SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
SPDX-License-Identifier: MPL-2.0
-->

# Visual X# specification examples

The `Spec/` directory is the public, example-driven language specification for Visual X#. Its `.vxs` files describe surface
syntax, semantic behavior, standard-library contracts, and both accepted and rejected forms.

These suites describe the intended language. The current compiler may not implement every fragment yet. Implementation work
must follow the documented behavior instead of treating an implementation shortcut as a language rule.

## Reading a suite

Each file contains numbered, independently readable fragments. Every fragment has an English explanation of its purpose.
Additional annotations identify two important cases:

- `Expected result` distinguishes intentionally valid and invalid forms.
- `Context note` says that an ellipsis stands for declarations omitted from the focused example.

A topic suite is not necessarily one compilable translation unit. Imports, helper declarations, or surrounding types may be
shown in another fragment when repeating them would hide the rule being demonstrated.

## Topic map

The suites are grouped by the language surface they explain:

- Core language: `CoreSystem.vxs`, `Decls.vxs`, `Operators.vxs`, `Optional.vxs`, and `Unsafe.vxs`.
- Metadata and interoperability: `Attributes.vxs`, `FFI.vxs`, and `InlineAssembly.vxs`.
- Control and execution: `Command.vxs`, `Exceptions.vxs`, `Iteration.vxs`, and `Threading.vxs`.
- Values and collections: `Collections.vxs`, `String.vxs`, and `BLINQ.vxs`.
- Console, files, and structured text: `ConsoleIO.vxs`, `FileIO.vxs`, `Text.Json.vxs`, and `Text.Xml.vxs`.
- Date and time: `Date.vxs` and `Time.vxs`.
- Databases: `MariaDB.vxs`, `PostgreSQL.vxs`, and `Sqlite3.vxs`.

The canonical list and short reading instructions are also available in [`Spec/README.md`](../Spec/README.md).

## Change discipline

Public examples use the `.vxs` source extension. Native executable output uses `.vxse`. A specification change should update
the affected suite first, keep API names stable unless the language contract explicitly changes them, and add focused tests
when compiler support is implemented.
