<!-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com> -->
<!-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0 -->

# Visual X# specification examples

The `.vxs` files below this directory are topic-oriented language design examples. Each numbered fragment is independent and
includes context that explains the intended rule, whether the fragment is valid or invalid, and which surrounding declarations
may be omitted for brevity.

These files are not concatenated programs. Repeated declarations, incomplete context, and deliberately rejected examples are
intentional. A file may therefore be useful as a design reference without being directly compilable as one application.

The directory records current language intent; it is not an implementation-completeness claim. The production compiler is
still moving to the Haskell-through-CorePrep and C++20 Xpp/Xmm architecture, so some examples describe behavior that has not
yet reached the production `vxs` route.

When a language rule is implemented, it should gain a focused compiler test in addition to its explanatory example here.
Compiler behavior must not be inferred from historical fixtures when it conflicts with the current examples.

See [the specification guide](../docs/SPECIFICATION.md) for the topic map and maintenance rules.

## Catalog

### Language

- [Declarations](Language/Decls.vxs) and [attributes](Language/Attributes.vxs)
- [Operators](Language/Operators.vxs), [iteration](Language/Iteration.vxs), and [unsafe behavior](Language/Unsafe.vxs)
- [String](Language/String.vxs), [Optional](Language/Optional.vxs), and [exceptions](Language/Exceptions.vxs)

### Standard library

- Core: [core system](StandardLibrary/Core/CoreSystem.vxs) and [collections](StandardLibrary/Core/Collections.vxs)
- I/O: [console](StandardLibrary/IO/ConsoleIO.vxs), [files](StandardLibrary/IO/FileIO.vxs), and
  [commands](StandardLibrary/IO/Command.vxs)
- Text: [JSON](StandardLibrary/Text/Text.Json.vxs) and [XML](StandardLibrary/Text/Text.Xml.vxs)
- Runtime services: [threading](StandardLibrary/Concurrency/Threading.vxs), [date](StandardLibrary/Temporal/Date.vxs), and
  [time](StandardLibrary/Temporal/Time.vxs)
- Query: [BLINQ](StandardLibrary/Query/BLINQ.vxs)

### Libraries and interop

- Databases: [MariaDB](Libraries/Databases/MariaDB.vxs), [PostgreSQL](Libraries/Databases/PostgreSQL.vxs), and
  [SQLite](Libraries/Databases/Sqlite3.vxs)
- Native boundaries: [FFI](Interop/FFI.vxs) and [inline assembly](Interop/InlineAssembly.vxs)
