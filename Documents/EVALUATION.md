<!-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com> -->
<!-- SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1 -->

# Evaluation by need

Visual X# is a lazy language with call-by-need evaluation. A value is computed
when it is first needed and at most once, and a value that is never needed is
never computed. Effects are not lazy: they happen where they are written. The
source has no notation for either; the compiler tells the two apart. The rules
are in `Spec/Language/Evaluation.vxs`.

This document says how much of that the compiler implements and how. It is an
implementation reference, not the definition of the language.

## What is implemented

The value of a local binding is computed by need when all of the following
hold:

- its type is `bool`, a numeric type or an enum;
- the binding is never assigned after it is declared;
- no closure captures it;
- its initializer is built from names, literals, operators, tests,
  conditionals and calls, with no store, no transfer of control and no block;
- the binding is in a method, not in the body of a callable.

Such a value is computed by the first read that is reached, and by no later
one. A binding whose initializer is never read computes nothing, so a division
by zero or a call that never returns in it does nothing.

Everything else is computed where it is written, as before. That is correct
for an expression with an effect and a restriction of the implementation for
the rest; see "What is pending".

## How a binding is deferred

The frontend lowers a deferred binding without any new form of Core. The
binding declares three kinds of local:

- the local itself, with a neutral value;
- a flag that is false until the value has been computed;
- a copy of each variable the initializer reads that is assigned anywhere in
  the function, taken where the binding stands.

Every read of the local is lowered to a test of the flag that, when the flag
is false, evaluates the initializer, stores the value and sets the flag,
followed by a read of the local. The initializer is lowered at each read, over
the copies, so it means what its variables held at the binding however late it
runs. The stages after the frontend see ordinary locals, stores and branches.

The body of a method is lowered twice. The first lowering evaluates every
binding in place and is kept only for what it shows: which locals are assigned
after their binding and which a closure captures. The second lowering defers.

## Where laziness cannot be observed

Deferring a value costs a flag and a test at each read. The compiler computes
a value where its binding stands, with neither, in two cases in which no
program can tell:

- the initializer cannot fail and cannot run without end, and reads no value
  that is itself deferred: it does not call and does not divide;
- the statement after the binding is certain to read the value: the
  expression that statement evaluates first reads it outside the right side of
  `&&` and `||` and outside the branches of a conditional.

In the second case one thing differs: when the value and an operand evaluated
before it in that statement both fail, the value fails first. A program that
fails either way is not told which of two failures it meets.

The second case assumes that a call needs its arguments. That is true of the
compiler today and not of the language, so the case has to be narrowed when
arguments are passed by need.

## What is pending

These are parts of the language the compiler does not implement yet. None is a
restriction of the language.

| Pending | Today |
| --- | --- |
| arguments passed by need | an argument is computed at the call, before the method runs |
| results returned by need | a returned value is computed before the method returns |
| values of other types by need: `String`, callables, objects | computed where they are written |
| a variable that is assigned again | the binding and every assignment are computed in place |
| a binding that a closure captures, and bindings in the body of a callable | computed in place |
| a value that is discarded, as in `_ = value;` | computed |
| a thunk as a value of its own in Core, Xpp and Xmm, with runtime support | a flag and a slot in the frame of one function |
| effects other than stores and transfers of control | none exist in the implemented subset: it has no input, output or shared state |

## Verification

`LazyEvaluationTests.hs` observes laziness through what tells a computed value
from one that was not: a division by zero, a call that never returns, and the
number of steps a program takes in the reference Core evaluator, which is how
"at most once" is checked. `source_execution_smoke` runs the same programs
through LLVM in both pipeline modes, where a division by zero that was
computed would end the process and a call that never returns would never end.
