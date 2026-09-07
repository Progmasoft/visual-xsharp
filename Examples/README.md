<!--
SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1
-->

# Visual X# comparative programs

Each directory contains the same complete program in four languages:

- Visual X#, written against the normative `Spec/` language and standard-library contracts;
- C# 13;
- C++20;
- Java 21.

The examples are comparative source material, not a declaration that every forward-looking Visual X# feature is already
implemented by the current compiler. C# and Java are example languages only. They are not compiler-development
prerequisites and must not be added to the Visual X# compiler toolchain requirements.

Every program is intentionally self-contained. The C++, C#, and Java variants use their own standard libraries rather
than imitating Visual X# APIs mechanically.

| Program | Main Visual X# surface |
| --- | --- |
| `HelloWorld` | namespace, class entry point, `String`, console output |
| `FizzBuzz` | closed ranges, `for (:)`, mandatory `if` parentheses |
| `Fibonacci` | lazy `System.Utils.Enumerable<T>` generator and `yield` |
| `WordFrequency` | `[T]` dynamic arrays and `[K to V]` dictionaries |
| `BankAccount` | AARC class, constructor labels, fields, mutation |
| `ShapeAreas` | data classes, inheritance, type patterns, `match` |
| `GenericStack` | templates and the `System.Array<T>` operation surface |
| `FileRoundTrip` | `Path`, `Files`, checked `IOException` propagation |
| `ConcurrentMessages` | spawned threads, lambdas, MPSC channel |
| `ExceptionRecovery` | `throw`, declared throwable flow, ordered `catch` |
