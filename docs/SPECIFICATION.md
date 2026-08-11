# Specification guide

## Purpose

`Spec/` contains 24 topic-oriented `.vxs` example suites. They explain current language design intent using independent,
numbered fragments.

The suites cover:

- core declarations, attributes, operators, strings, optionals, exceptions, and unsafe behavior;
- collections, iteration, threading, dates, times, console I/O, file I/O, and command execution;
- JSON, XML, BLINQ, PostgreSQL, MariaDB, and SQLite APIs; and
- FFI and inline assembly boundaries.

## How to read an example

Each example should identify:

- the rule being demonstrated;
- whether the fragment is valid or invalid;
- whether declarations are omitted for context; and
- the expected compiler or runtime behavior.

Examples in one file are not concatenated. Repeated names and intentionally invalid fragments are normal.

## Design versus implementation

The examples are a language-design catalog, not a statement that every feature is implemented. The compiler test suite is the
source of truth for current implementation coverage. A design example should gain a focused compiler test when its behavior is
implemented.

## Public language rules

Current cross-cutting rules include:

- source files use `.vxs`;
- project executables use `.vxse`;
- runtime `String` is a sequence of Unicode code points rather than a UTF-8 byte container;
- project entry is a namespace-qualified class with `public static void Main()`;
- top-level runtime functions are not permitted;
- public intermediate artifact names are `.core`, `.xpp`, and `.xmm`; and
- normal compilation does not write intermediate representations unless emission is explicitly requested.

## Maintenance

- Public prose is written in en-US English.
- Correct spelling is required even when an earlier draft treated a misspelling as an API name.
- New examples must not expose repository-internal planning paths.
- Historical fixtures must not override the current language design.
- Ambiguous or contradictory rules must be resolved before implementation; they must not be silently guessed by compiler code.
