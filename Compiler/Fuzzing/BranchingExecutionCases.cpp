// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "BranchingExecutionCases.hpp"
#include "ExecutionCases.hpp"

// Executable regressions for `match`, for `if` used as an expression and for
// `guard`. Each case is a method body, the arguments it runs on and the value
// it must return. The expected values are written by hand from the language
// rules: the subjects of a match are evaluated once and left to right, the
// arms are tested in order, a guard runs only when the patterns of its arm
// accept, exactly one body runs, and the block of a guard runs only when its
// condition is false. The same table is checked against a reference
// evaluator in `BranchingTests.hs` of the frontend test suite; here every
// case runs through CorePrep, Xpp, Xmm, LLVM and the ORC JIT, unoptimized
// and optimized.

namespace Visual::XSharp::Fuzzing
{
    namespace
    {
        constexpr std::array<ExecutionCase, 148U> kCases{ {
            { false,
              false,
              1,
              0,
              10,
              "return match (left) { 1 -> 10, 2 -> 20, _ -> 30 };" },
            { false,
              false,
              2,
              0,
              20,
              "return match (left) { 1 -> 10, 2 -> 20, _ -> 30 };" },
            { false,
              false,
              5,
              0,
              30,
              "return match (left) { 1 -> 10, 2 -> 20, _ -> 30 };" },
            { false,
              false,
              1,
              0,
              10,
              "int v = 0 - left; return match (v) { -1 -> 10, -2 -> 20, 0 -> "
              "5, _ -> 7 };" },
            { false,
              false,
              2,
              0,
              20,
              "int v = 0 - left; return match (v) { -1 -> 10, -2 -> 20, 0 -> "
              "5, _ -> 7 };" },
            { false,
              false,
              0,
              0,
              5,
              "int v = 0 - left; return match (v) { -1 -> 10, -2 -> 20, 0 -> "
              "5, _ -> 7 };" },
            { false,
              false,
              3,
              0,
              7,
              "int v = 0 - left; return match (v) { -1 -> 10, -2 -> 20, 0 -> "
              "5, _ -> 7 };" },
            { false,
              false,
              1,
              0,
              1,
              "int v = 0 - left - 9223372036854775807; return match (v) { "
              "-9223372036854775808 -> 1, -9223372036854775807 -> 2, _ -> 0 "
              "};" },
            { false,
              false,
              0,
              0,
              2,
              "int v = 0 - left - 9223372036854775807; return match (v) { "
              "-9223372036854775808 -> 1, -9223372036854775807 -> 2, _ -> 0 "
              "};" },
            { true,
              false,
              3,
              2,
              1,
              "int v = right - left; return match (v), (flag) { (-1), (true) "
              "-> 1, (-1), (_) -> 2, (_), (_) -> 3 };" },
            { false,
              false,
              3,
              2,
              2,
              "int v = right - left; return match (v), (flag) { (-1), (true) "
              "-> 1, (-1), (_) -> 2, (_), (_) -> 3 };" },
            { true,
              false,
              2,
              2,
              3,
              "int v = right - left; return match (v), (flag) { (-1), (true) "
              "-> 1, (-1), (_) -> 2, (_), (_) -> 3 };" },
            { true,
              false,
              0,
              0,
              1,
              "int r = if (flag) { return 1; } else { return 2; }; return r + "
              "50;" },
            { false,
              false,
              0,
              0,
              2,
              "int r = if (flag) { return 1; } else { return 2; }; return r + "
              "50;" },
            { false,
              false,
              0,
              0,
              10,
              "int r = match (left) { 0 -> { return 10; }, _ -> { return 20; } "
              "}; return r + 1;" },
            { false,
              false,
              5,
              0,
              20,
              "int r = match (left) { 0 -> { return 10; }, _ -> { return 20; } "
              "}; return r + 1;" },
            { true,
              false,
              0,
              0,
              7,
              "return Twice(if (flag) { return 7; } else { return 9; });" },
            { false,
              false,
              0,
              0,
              9,
              "return Twice(if (flag) { return 7; } else { return 9; });" },
            { true,
              false,
              0,
              0,
              1,
              "int r = 1 + (if (flag) { return 1; } else { return 2; }); "
              "return r;" },
            { false,
              false,
              0,
              0,
              2,
              "int r = 1 + (if (flag) { return 1; } else { return 2; }); "
              "return r;" },
            { false,
              false,
              2,
              0,
              3,
              "int t = 0; int i = 0; while (i < 5) { i += 1; int q = if (i > "
              "left) { break; } else { continue; }; t += q; } return t * 10 + "
              "i;" },
            { false,
              false,
              9,
              0,
              5,
              "int t = 0; int i = 0; while (i < 5) { i += 1; int q = if (i > "
              "left) { break; } else { continue; }; t += q; } return t * 10 + "
              "i;" },
            { true,
              false,
              5,
              0,
              1,
              "bool b = flag && (if (left > 0) { return 1; } else { return 2; "
              "}); return b ? 3 : 4;" },
            { true,
              false,
              0,
              0,
              2,
              "bool b = flag && (if (left > 0) { return 1; } else { return 2; "
              "}); return b ? 3 : 4;" },
            { false,
              false,
              5,
              0,
              4,
              "bool b = flag && (if (left > 0) { return 1; } else { return 2; "
              "}); return b ? 3 : 4;" },
            { true,
              false,
              5,
              0,
              3,
              "bool b = flag || (if (left > 0) { return 1; } else { return 2; "
              "}); return b ? 3 : 4;" },
            { false,
              false,
              5,
              0,
              1,
              "bool b = flag || (if (left > 0) { return 1; } else { return 2; "
              "}); return b ? 3 : 4;" },
            { false,
              false,
              0,
              0,
              2,
              "bool b = flag || (if (left > 0) { return 1; } else { return 2; "
              "}); return b ? 3 : 4;" },
            { false,
              false,
              5,
              0,
              5,
              "int n = left ?: (if (flag) { return 100; } else { return 200; "
              "}); return n;" },
            { true,
              false,
              0,
              0,
              100,
              "int n = left ?: (if (flag) { return 100; } else { return 200; "
              "}); return n;" },
            { false,
              false,
              0,
              0,
              200,
              "int n = left ?: (if (flag) { return 100; } else { return 200; "
              "}); return n;" },
            { false,
              false,
              3,
              0,
              5,
              "int r = if (left > 0) { 5 } else { if (flag) { return 1; } else "
              "{ return 2; } }; return r;" },
            { true,
              false,
              0,
              0,
              1,
              "int r = if (left > 0) { 5 } else { if (flag) { return 1; } else "
              "{ return 2; } }; return r;" },
            { false,
              false,
              0,
              0,
              2,
              "int r = if (left > 0) { 5 } else { if (flag) { return 1; } else "
              "{ return 2; } }; return r;" },
            { false,
              false,
              5,
              0,
              6,
              "int n = left; do { n += 1; } while (if (n > 3) { return n; } "
              "else { return 0 - n; }); return 99;" },
            { false,
              false,
              1,
              0,
              -2,
              "int n = left; do { n += 1; } while (if (n > 3) { return n; } "
              "else { return 0 - n; }); return 99;" },
            { false,
              false,
              9,
              0,
              100,
              "int r = if (left > 5) { return 100; } else { left * 2 }; return "
              "r + 1;" },
            { false,
              false,
              3,
              0,
              7,
              "int r = if (left > 5) { return 100; } else { left * 2 }; return "
              "r + 1;" },
            { false,
              false,
              0,
              0,
              50,
              "int r = match (left) { 0 -> { return 50; }, int n -> n + 1 }; "
              "return r * 2;" },
            { false,
              false,
              4,
              0,
              10,
              "int r = match (left) { 0 -> { return 50; }, int n -> n + 1 }; "
              "return r * 2;" },
            { true,
              false,
              4,
              0,
              7,
              "return Twice(if (flag) { return 7; } else { left });" },
            { false,
              false,
              4,
              0,
              8,
              "return Twice(if (flag) { return 7; } else { left });" },
            { false,
              false,
              3,
              0,
              604,
              "int t = 0; int i = 0; while (i < 10) { i += 1; t += if (i > "
              "left) { break; } else { i }; } return t * 100 + i;" },
            { false,
              false,
              0,
              0,
              1,
              "int t = 0; int i = 0; while (i < 10) { i += 1; t += if (i > "
              "left) { break; } else { i }; } return t * 100 + i;" },
            { false,
              false,
              4,
              0,
              9,
              "int t = 0; for (int i = 0; i < 6; i += 1) { t += match (i) { 2 "
              "-> { continue; }, int n -> { if (n == left) { continue; } else "
              "{ n } } }; } return t;" },
            { false,
              false,
              9,
              0,
              13,
              "int t = 0; for (int i = 0; i < 6; i += 1) { t += match (i) { 2 "
              "-> { continue; }, int n -> { if (n == left) { continue; } else "
              "{ n } } }; } return t;" },
            { true,
              false,
              0,
              0,
              1,
              "guard (left > 0) else { match (flag) { true -> { return 1; }, "
              "false -> { return 2; } } } return left + 10;" },
            { false,
              false,
              0,
              0,
              2,
              "guard (left > 0) else { match (flag) { true -> { return 1; }, "
              "false -> { return 2; } } } return left + 10;" },
            { false,
              false,
              5,
              0,
              15,
              "guard (left > 0) else { match (flag) { true -> { return 1; }, "
              "false -> { return 2; } } } return left + 10;" },
            { false,
              false,
              4,
              0,
              8,
              "int t = 0; int i = 0; while (i < left) { i += 1; guard (i \\= "
              "2) else { continue; } t += i; } return t;" },
            { false,
              false,
              1,
              0,
              1,
              "int t = 0; int i = 0; while (i < left) { i += 1; guard (i \\= "
              "2) else { continue; } t += i; } return t;" },
            { false,
              false,
              1,
              0,
              10,
              "int r = 0; match (left) { 1 -> { r = 10; }, 2 -> { r = 20; } } "
              "return r;" },
            { false,
              false,
              2,
              0,
              20,
              "int r = 0; match (left) { 1 -> { r = 10; }, 2 -> { r = 20; } } "
              "return r;" },
            { false,
              false,
              7,
              0,
              0,
              "int r = 0; match (left) { 1 -> { r = 10; }, 2 -> { r = 20; } } "
              "return r;" },
            { false,
              false,
              20,
              0,
              40,
              "return match (left) { int n if n > 10 -> n * 2, int n if n > 5 "
              "-> n + 1, _ -> 0 };" },
            { false,
              false,
              7,
              0,
              8,
              "return match (left) { int n if n > 10 -> n * 2, int n if n > 5 "
              "-> n + 1, _ -> 0 };" },
            { false,
              false,
              3,
              0,
              0,
              "return match (left) { int n if n > 10 -> n * 2, int n if n > 5 "
              "-> n + 1, _ -> 0 };" },
            { false,
              false,
              1,
              1,
              11,
              "return match (left), (right) { (1), (1) -> 11, (1), (_) -> 10, "
              "(_), (1) -> 1, (_), (_) -> 0 };" },
            { false,
              false,
              1,
              5,
              10,
              "return match (left), (right) { (1), (1) -> 11, (1), (_) -> 10, "
              "(_), (1) -> 1, (_), (_) -> 0 };" },
            { false,
              false,
              4,
              1,
              1,
              "return match (left), (right) { (1), (1) -> 11, (1), (_) -> 10, "
              "(_), (1) -> 1, (_), (_) -> 0 };" },
            { false,
              false,
              4,
              4,
              0,
              "return match (left), (right) { (1), (1) -> 11, (1), (_) -> 10, "
              "(_), (1) -> 1, (_), (_) -> 0 };" },
            { true,
              false,
              0,
              0,
              1,
              "return match (flag) { true -> 1, false -> 2 };" },
            { false,
              false,
              0,
              0,
              2,
              "return match (flag) { true -> 1, false -> 2 };" },
            { true,
              true,
              0,
              0,
              1,
              "return match (flag), (other) { (true), (_) -> 1, (false), "
              "(true) -> 2, (false), (false) -> 3 };" },
            { true,
              false,
              0,
              0,
              1,
              "return match (flag), (other) { (true), (_) -> 1, (false), "
              "(true) -> 2, (false), (false) -> 3 };" },
            { false,
              true,
              0,
              0,
              2,
              "return match (flag), (other) { (true), (_) -> 1, (false), "
              "(true) -> 2, (false), (false) -> 3 };" },
            { false,
              false,
              0,
              0,
              3,
              "return match (flag), (other) { (true), (_) -> 1, (false), "
              "(true) -> 2, (false), (false) -> 3 };" },
            { true,
              true,
              0,
              0,
              3,
              "return match (flag), (other) { (true), (true) -> 3, (true), (_) "
              "-> 2, (_), (true) -> 1, (_), (_) -> 0 };" },
            { true,
              false,
              0,
              0,
              2,
              "return match (flag), (other) { (true), (true) -> 3, (true), (_) "
              "-> 2, (_), (true) -> 1, (_), (_) -> 0 };" },
            { false,
              true,
              0,
              0,
              1,
              "return match (flag), (other) { (true), (true) -> 3, (true), (_) "
              "-> 2, (_), (true) -> 1, (_), (_) -> 0 };" },
            { false,
              false,
              0,
              0,
              0,
              "return match (flag), (other) { (true), (true) -> 3, (true), (_) "
              "-> 2, (_), (true) -> 1, (_), (_) -> 0 };" },
            { false,
              false,
              0,
              0,
              101,
              "int n = left; int r = match (n += 1) { 1 -> 100, 2 -> 200, _ -> "
              "300 }; return r + n;" },
            { false,
              false,
              1,
              0,
              202,
              "int n = left; int r = match (n += 1) { 1 -> 100, 2 -> 200, _ -> "
              "300 }; return r + n;" },
            { false,
              false,
              5,
              0,
              306,
              "int n = left; int r = match (n += 1) { 1 -> 100, 2 -> 200, _ -> "
              "300 }; return r + n;" },
            { false,
              false,
              1,
              0,
              1,
              "int n = left; return match (n += 1), (n * 10) { (2), (20) -> 1, "
              "(_), (_) -> 0 };" },
            { false,
              false,
              2,
              0,
              0,
              "int n = left; return match (n += 1), (n * 10) { (2), (20) -> 1, "
              "(_), (_) -> 0 };" },
            { false,
              false,
              1,
              0,
              1001,
              "int calls = 0; int r = match (left) { 1 if (calls += 1) > 0 -> "
              "10, 2 if (calls += 10) > 0 -> 20, _ -> 30 }; return r * 100 + "
              "calls;" },
            { false,
              false,
              2,
              0,
              2010,
              "int calls = 0; int r = match (left) { 1 if (calls += 1) > 0 -> "
              "10, 2 if (calls += 10) > 0 -> 20, _ -> 30 }; return r * 100 + "
              "calls;" },
            { false,
              false,
              3,
              0,
              3000,
              "int calls = 0; int r = match (left) { 1 if (calls += 1) > 0 -> "
              "10, 2 if (calls += 10) > 0 -> 20, _ -> 30 }; return r * 100 + "
              "calls;" },
            { true,
              false,
              1,
              0,
              1,
              "return match (left) { 1 if flag -> 1, 1 -> 2, _ -> 3 };" },
            { false,
              false,
              1,
              0,
              2,
              "return match (left) { 1 if flag -> 1, 1 -> 2, _ -> 3 };" },
            { true,
              false,
              9,
              0,
              3,
              "return match (left) { 1 if flag -> 1, 1 -> 2, _ -> 3 };" },
            { false,
              false,
              1,
              5,
              1,
              "return match (left) { 1 if right -> 1, _ -> 0 };" },
            { false,
              false,
              1,
              0,
              0,
              "return match (left) { 1 if right -> 1, _ -> 0 };" },
            { false,
              false,
              2,
              5,
              0,
              "return match (left) { 1 if right -> 1, _ -> 0 };" },
            { false,
              false,
              1,
              0,
              1001,
              "int n = 0; int r = match (left) { 1 -> (n += 1), 2 -> (n += "
              "10), _ -> (n += 100) }; return r * 1000 + n;" },
            { false,
              false,
              2,
              0,
              10010,
              "int n = 0; int r = match (left) { 1 -> (n += 1), 2 -> (n += "
              "10), _ -> (n += 100) }; return r * 1000 + n;" },
            { false,
              false,
              3,
              0,
              100100,
              "int n = 0; int r = match (left) { 1 -> (n += 1), 2 -> (n += "
              "10), _ -> (n += 100) }; return r * 1000 + n;" },
            { false,
              false,
              2,
              0,
              1,
              "return match (Twice(left)) { 4 -> 1, 6 -> 2, _ -> 0 };" },
            { false,
              false,
              3,
              0,
              2,
              "return match (Twice(left)) { 4 -> 1, 6 -> 2, _ -> 0 };" },
            { false,
              false,
              4,
              0,
              0,
              "return match (Twice(left)) { 4 -> 1, 6 -> 2, _ -> 0 };" },
            { false,
              false,
              0,
              0,
              1,
              "long wide = 5; return match (wide) { 5 -> 1, _ -> 0 };" },
            { false,
              false,
              1,
              0,
              0,
              "long wide = match (left) { 1 -> 10, _ -> 20 }; return wide > 15 "
              "? 1 : 0;" },
            { false,
              false,
              2,
              0,
              1,
              "long wide = match (left) { 1 -> 10, _ -> 20 }; return wide > 15 "
              "? 1 : 0;" },
            { false,
              false,
              1,
              0,
              5,
              "int r = 0; match (left) { 1 -> r = 5, _ -> r = Twice(left) } "
              "return r;" },
            { false,
              false,
              4,
              0,
              8,
              "int r = 0; match (left) { 1 -> r = 5, _ -> r = Twice(left) } "
              "return r;" },
            { true,
              false,
              1,
              0,
              5,
              "return match (left) { 1 -> if (flag) { 5 } else { 6 }, _ -> "
              "match (right) { 0 -> 7, _ -> 8 } };" },
            { false,
              false,
              1,
              0,
              6,
              "return match (left) { 1 -> if (flag) { 5 } else { 6 }, _ -> "
              "match (right) { 0 -> 7, _ -> 8 } };" },
            { false,
              false,
              2,
              0,
              7,
              "return match (left) { 1 -> if (flag) { 5 } else { 6 }, _ -> "
              "match (right) { 0 -> 7, _ -> 8 } };" },
            { false,
              false,
              2,
              3,
              8,
              "return match (left) { 1 -> if (flag) { 5 } else { 6 }, _ -> "
              "match (right) { 0 -> 7, _ -> 8 } };" },
            { false,
              false,
              1,
              4,
              9,
              "return match (left) { 1 -> { int t = right * 2; t + 1 }, _ -> { "
              "int t = right * 3; t - 1 } };" },
            { false,
              false,
              2,
              4,
              11,
              "return match (left) { 1 -> { int t = right * 2; t + 1 }, _ -> { "
              "int t = right * 3; t - 1 } };" },
            { false,
              false,
              10,
              0,
              8,
              "int total = 0; for (int i = 0; i < left; i++) { match (i) { 2 "
              "-> { continue; }, 5 -> { break; }, _ -> { total += i; } } } "
              "return total;" },
            { false,
              false,
              3,
              0,
              1,
              "int total = 0; for (int i = 0; i < left; i++) { match (i) { 2 "
              "-> { continue; }, 5 -> { break; }, _ -> { total += i; } } } "
              "return total;" },
            { false,
              false,
              0,
              0,
              0,
              "int total = 0; for (int i = 0; i < left; i++) { match (i) { 2 "
              "-> { continue; }, 5 -> { break; }, _ -> { total += i; } } } "
              "return total;" },
            { false,
              false,
              0,
              0,
              40,
              "int n = 0; int r = while (true) { n += 1; match (n) { 4 -> { "
              "break n * 10; }, _ -> { } } }; return r;" },
            { false,
              false,
              1,
              0,
              100,
              "match (left) { 1 -> { return 100; }, _ -> { } } return 5;" },
            { false,
              false,
              2,
              0,
              5,
              "match (left) { 1 -> { return 100; }, _ -> { } } return 5;" },
            { false, false, 1, 0, 5, "match (left) { } return 5;" },
            { false,
              false,
              3,
              5,
              5,
              "int r = if (left > right) { left } else { right }; return r;" },
            { false,
              false,
              9,
              2,
              9,
              "int r = if (left > right) { left } else { right }; return r;" },
            { true,
              false,
              4,
              0,
              9,
              "int r = if (flag) { int t = left * 2; t + 1 } else { int t = "
              "right * 3; t - 1 }; return r;" },
            { false,
              false,
              0,
              5,
              14,
              "int r = if (flag) { int t = left * 2; t + 1 } else { int t = "
              "right * 3; t - 1 }; return r;" },
            { true,
              false,
              0,
              0,
              10001,
              "int n = 0; int r = if (flag) { n += 1; 10 } else { n += 100; 20 "
              "}; return r * 1000 + n;" },
            { false,
              false,
              0,
              0,
              20100,
              "int n = 0; int r = if (flag) { n += 1; 10 } else { n += 100; 20 "
              "}; return r * 1000 + n;" },
            { true,
              true,
              1,
              2,
              101,
              "return (if (flag) { left } else { right }) + (if (other) { 100 "
              "} else { 200 });" },
            { false,
              false,
              1,
              2,
              202,
              "return (if (flag) { left } else { right }) + (if (other) { 100 "
              "} else { 200 });" },
            { false,
              false,
              3,
              0,
              6,
              "guard (left > 0) else { return 0 - 1; } return left * 2;" },
            { false,
              false,
              0,
              0,
              -1,
              "guard (left > 0) else { return 0 - 1; } return left * 2;" },
            { false,
              false,
              6,
              0,
              6,
              "int total = 0; for (int i = 0; i < left; i++) { guard (i % 2 == "
              "0) else { continue; } total += i; } return total;" },
            { false,
              false,
              1,
              0,
              0,
              "int total = 0; for (int i = 0; i < left; i++) { guard (i % 2 == "
              "0) else { continue; } total += i; } return total;" },
            { false,
              false,
              4,
              0,
              4,
              "int n = 0; while (true) { guard (n < left) else { break; } n += "
              "1; } return n;" },
            { false,
              false,
              0,
              0,
              0,
              "int n = 0; while (true) { guard (n < left) else { break; } n += "
              "1; } return n;" },
            { false,
              false,
              2,
              3,
              13,
              "int total = 0; { int part = left * 2; total += part; } { int "
              "part = right * 3; total += part; } return total;" },
            { false,
              false,
              0,
              0,
              0,
              "int total = 0; { int part = left * 2; total += part; } { int "
              "part = right * 3; total += part; } return total;" },
            { false,
              false,
              3,
              0,
              40,
              "int n = 0; while (true) { { n += 1; if (n > left) { break; } } "
              "} { { return n * 10; } }" },
            { false,
              false,
              0,
              0,
              10,
              "int n = 0; while (true) { { n += 1; if (n > left) { break; } } "
              "} { { return n * 10; } }" },
            { false,
              false,
              4,
              0,
              10,
              "int total = 0; for (int i = 0; i < left; i++) { { if (i == 1) { "
              "continue; } } { int step = i * 2; total += step; } } return "
              "total;" },
            { false,
              false,
              1,
              0,
              0,
              "int total = 0; for (int i = 0; i < left; i++) { { if (i == 1) { "
              "continue; } } { int step = i * 2; total += step; } } return "
              "total;" },
            { false,
              false,
              1,
              0,
              10,
              "return match (left) { 1 -> 10 2 -> 20 _ -> 30 };" },
            { false,
              false,
              2,
              0,
              20,
              "return match (left) { 1 -> 10 2 -> 20 _ -> 30 };" },
            { false,
              false,
              9,
              0,
              30,
              "return match (left) { 1 -> 10 2 -> 20 _ -> 30 };" },
            { false,
              false,
              5,
              0,
              10,
              "return match (left) { int value if value > 2 -> { value = value "
              "* 2; value }, int value -> { value += 1; value } };" },
            { false,
              false,
              1,
              0,
              2,
              "return match (left) { int value if value > 2 -> { value = value "
              "* 2; value }, int value -> { value += 1; value } };" },
            { false,
              false,
              3,
              0,
              2,
              "int r = 0; match (left) { int value if (value = 7) > 9 -> { r = "
              "1; }, 3 -> { r = 2; }, _ -> { r = 3; } } return r;" },
            { false,
              false,
              4,
              0,
              3,
              "int r = 0; match (left) { int value if (value = 7) > 9 -> { r = "
              "1; }, 3 -> { r = 2; }, _ -> { r = 3; } } return r;" },
            { true,
              true,
              0,
              0,
              1,
              "int r = if (flag) { if (other) { 1 } else { 2 } } else { match "
              "(left) { 1 -> 10, _ -> 20 } }; return r;" },
            { true,
              false,
              0,
              0,
              2,
              "int r = if (flag) { if (other) { 1 } else { 2 } } else { match "
              "(left) { 1 -> 10, _ -> 20 } }; return r;" },
            { false,
              false,
              1,
              0,
              10,
              "int r = if (flag) { if (other) { 1 } else { 2 } } else { match "
              "(left) { 1 -> 10, _ -> 20 } }; return r;" },
            { false,
              false,
              2,
              0,
              20,
              "int r = if (flag) { if (other) { 1 } else { 2 } } else { match "
              "(left) { 1 -> 10, _ -> 20 } }; return r;" },
            { true,
              true,
              1,
              0,
              11,
              "int n = 0; int r = if (flag) { if (other) { n += 1; } else { n "
              "+= 2; } match (left) { 1 -> { n += 10; } } n } else { 0 }; "
              "return r;" },
            { true,
              false,
              2,
              0,
              2,
              "int n = 0; int r = if (flag) { if (other) { n += 1; } else { n "
              "+= 2; } match (left) { 1 -> { n += 10; } } n } else { 0 }; "
              "return r;" },
            { false,
              false,
              1,
              0,
              0,
              "int n = 0; int r = if (flag) { if (other) { n += 1; } else { n "
              "+= 2; } match (left) { 1 -> { n += 10; } } n } else { 0 }; "
              "return r;" },
            { false,
              false,
              1,
              2,
              12,
              "match (left) { 1 -> { match (right) { 2 -> { return 12; } } }, "
              "_ -> { } } return 5;" },
            { false,
              false,
              1,
              3,
              5,
              "match (left) { 1 -> { match (right) { 2 -> { return 12; } } }, "
              "_ -> { } } return 5;" },
            { false,
              false,
              2,
              2,
              5,
              "match (left) { 1 -> { match (right) { 2 -> { return 12; } } }, "
              "_ -> { } } return 5;" },
            { false,
              false,
              5,
              0,
              6,
              "int n = left; guard ((n += 1) > 3) else { return n * 10; } "
              "return n;" },
            { false,
              false,
              1,
              0,
              20,
              "int n = left; guard ((n += 1) > 3) else { return n * 10; } "
              "return n;" },
        } };

        // The methods a body may call besides `Run` itself.
        constexpr std::string_view kHelpers
            = "    public static int Twice(_ int value) { return value + "
              "value; }\n";

        // A match with many arms. Arm `index` yields `index * 3 + 1` and the
        // catch-all yields 0.
        [[nodiscard]] auto
        WideMatchBody(int arms) -> std::string
        {
            std::string body = "return match (left) { ";
            for (int index = 0; index < arms; ++index)
                body += std::to_string(index) + " -> "
                        + std::to_string(index * 3 + 1) + ", ";
            return body + "_ -> 0 };";
        }

        // The same table written as an `else if` chain.
        [[nodiscard]] auto
        ElseIfChainBody(int links) -> std::string
        {
            std::string body;
            for (int index = 0; index < links; ++index)
                body += std::string(index == 0 ? "if" : " else if")
                        + " (left == " + std::to_string(index) + ") { return "
                        + std::to_string(index * 3 + 1) + "; }";
            return body + " return 0;";
        }

        // `if (left > 0) { if (left > 1) { ... total += 1; } }`
        [[nodiscard]] auto
        NestedIfBody(int levels) -> std::string
        {
            std::string body = "int total = 0; ";
            for (int index = 0; index < levels; ++index)
                body += "if (left > " + std::to_string(index) + ") { ";
            body += "total += 1; ";
            for (int index = 0; index < levels; ++index)
                body += "} ";
            return body + "return total;";
        }

        // `left + left + ... + left`
        [[nodiscard]] auto
        SumBody(int operands) -> std::string
        {
            std::string body = "return left";
            for (int index = 1; index < operands; ++index)
                body += " + left";
            return body + ";";
        }

        // The value of arm or link `index` in the tables above.
        [[nodiscard]] constexpr auto
        TableValue(int index) -> std::int64_t
        {
            return std::int64_t{ index } * 3 + 1;
        }
    } // namespace

    void
    ExerciseBranchingCases()
    {
        ExerciseExecutionCases("Branching execution", kCases, kHelpers);

        // Programs whose size is the point. Every body is compiled once and
        // all of its runs are checked by that one program.
        std::vector<ExecutionCase> large;

        // The subjects select the first arm, arms around a multiple of
        // sixteen, an arm in the middle, the last arm and the catch-all. A
        // lowering that nested one level per arm would overflow the stack
        // of the stages after Core long before this many arms.
        constexpr int kWideArms = 200;
        const auto wide = WideMatchBody(kWideArms);
        for (const auto subject : { 0, 15, 16, 17, 150, 199, 200 })
            large.push_back({ false,
                              false,
                              subject,
                              0,
                              subject < kWideArms ? TableValue(subject) : 0,
                              wide });

        // An `else if` chain of 300 links. The native wire reader, the Core
        // verifier and the CorePrep adapter walk a chain in a loop; when
        // they recursed, 150 links overflowed the stack. Both CorePrep
        // lowerings are compared on it as on every other program here.
        constexpr int kChainLinks = 300;
        const auto chain = ElseIfChainBody(kChainLinks);
        for (const auto subject : { 0, 149, 150, 299, 300 })
            large.push_back({ false,
                              false,
                              subject,
                              0,
                              subject < kChainLinks ? TableValue(subject) : 0,
                              chain });

        // Programs at the nesting limits of the frontend: a statement at
        // level 256 and an expression at level 1024. They compile only on
        // the compiler stack, which the smoke program runs on like `vxs`.
        constexpr int kLevels = 255;
        const auto nested = NestedIfBody(kLevels);
        for (const auto argument : { kLevels, kLevels - 1 })
            large.push_back({ false,
                              false,
                              argument,
                              0,
                              argument == kLevels ? 1 : 0,
                              nested });
        constexpr int kOperands = 1024;
        const auto sum = SumBody(kOperands);
        large.push_back(
            { false, false, 3, 0, std::int64_t{ 3 } * kOperands, sum });

        ExerciseExecutionCases("Branching execution", large, kHelpers);
    }
} // namespace Visual::XSharp::Fuzzing
