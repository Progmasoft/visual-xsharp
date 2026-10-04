// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

#include <array>
#include <string_view>

#include "ExecutionCases.hpp"
#include "LeavingExecutionCases.hpp"

// Executable regressions for expressions that leave instead of yielding a
// value: `return`, `break` and `continue` out of blocks used as values, in
// loop bodies, in the condition and the update clause of a loop, and in
// loops used as expressions. Each case is a method body, the arguments it
// runs on and the value it must return. The expected values are written by
// hand from the language rules: what precedes the transfer is evaluated
// once and in order, what follows it is not evaluated, `break` and
// `continue` target the nearest loop, a `break` in the condition of a loop
// leaves that loop, and a `continue` in an update clause ends the update.
// The same table is checked against a reference evaluator, as
// `leavingCases` of `BranchingEvaluationCases.hs` in the frontend test
// suite; here every case runs through CorePrep, Xpp, Xmm, LLVM and the ORC
// JIT, unoptimized and optimized.

namespace Visual::XSharp::Fuzzing
{
    namespace
    {
        constexpr std::array<ExecutionCase, 115U> kCases{ {
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
              1,
              0,
              101,
              "int t = 0; int n = 0; do { n += 1; if (n == left) { continue; } "
              "t += 10; } while (if ((t += 1) > 100) { return 1; } else { "
              "return t * 100 + n; }); return 99;" },
            { false,
              false,
              5,
              0,
              1101,
              "int t = 0; int n = 0; do { n += 1; if (n == left) { continue; } "
              "t += 10; } while (if ((t += 1) > 100) { return 1; } else { "
              "return t * 100 + n; }); return 99;" },
            { false,
              false,
              1,
              0,
              1000,
              "int t = 0; do { if (left > 0) { break; } t += 5; } while (if "
              "((t += 1) > 0) { return t; } else { return 0; }); return t + "
              "1000;" },
            { false,
              false,
              0,
              0,
              6,
              "int t = 0; do { if (left > 0) { break; } t += 5; } while (if "
              "((t += 1) > 0) { return t; } else { return 0; }); return t + "
              "1000;" },
            { false,
              false,
              0,
              0,
              10,
              "int t = 0; for (int i = 0; i < 3; i += 1) { do { t += 1; } "
              "while (if (t > left) { return t * 10 + i; } else { return t; "
              "}); } return 77;" },
            { false,
              false,
              5,
              0,
              1,
              "int t = 0; for (int i = 0; i < 3; i += 1) { do { t += 1; } "
              "while (if (t > left) { return t * 10 + i; } else { return t; "
              "}); } return 77;" },
            { false,
              false,
              0,
              0,
              1,
              "int t = 0; int i = 0; while (i < 3) { i += 1; int j = 0; do { j "
              "+= 1; if (j < 3) { continue; } t += j; } while (if (i > left) { "
              "return t * 10 + i; } else { j < 4 }); } return t;" },
            { false,
              false,
              9,
              0,
              21,
              "int t = 0; int i = 0; while (i < 3) { i += 1; int j = 0; do { j "
              "+= 1; if (j < 3) { continue; } t += j; } while (if (i > left) { "
              "return t * 10 + i; } else { j < 4 }); } return t;" },
            { false,
              false,
              0,
              0,
              55,
              "int t = 0; int i = 0; while (i < 5) { i += 1; int j = 0; while "
              "(j < 3) { j += 1; t += if (j == 2) { break; } else { 1 }; } } "
              "return t * 10 + i;" },
            { false,
              false,
              0,
              0,
              105,
              "int t = 0; int i = 0; while (i < 5) { i += 1; int j = 0; while "
              "(j < 3) { j += 1; t += if (j == 2) { continue; } else { 1 }; } "
              "} return t * 10 + i;" },
            { false,
              false,
              3,
              0,
              4,
              "int t = 0; while (t < 10) { t += 1; int q = while (true) { "
              "break t; }; if (q > left) { break; } } return t;" },
            { false,
              false,
              20,
              0,
              10,
              "int t = 0; while (t < 10) { t += 1; int q = while (true) { "
              "break t; }; if (q > left) { break; } } return t;" },
            { false,
              false,
              1,
              0,
              10,
              "int t = 0; bool b = (t += 1) > 0 && (if (left > 0) { return t * "
              "10; } else { return t * 100; }); return 7;" },
            { false,
              false,
              0,
              0,
              100,
              "int t = 0; bool b = (t += 1) > 0 && (if (left > 0) { return t * "
              "10; } else { return t * 100; }); return 7;" },
            { false,
              false,
              1,
              0,
              1,
              "int t = 0; bool b = (t += 1) > 5 && (if (left > 0) { return t * "
              "10; } else { return t * 100; }); return t + (b ? 1000 : 0);" },
            { false,
              false,
              0,
              0,
              1,
              "int t = 0; bool b = (t += 1) > 5 && (if (left > 0) { return t * "
              "10; } else { return t * 100; }); return t + (b ? 1000 : 0);" },
            { false,
              false,
              3,
              0,
              4,
              "int t = 0; bool b = (t += left) > 0 || (if (flag) { return t * "
              "10; } else { return 50 + t; }); return b ? t + 1 : 7;" },
            { true,
              false,
              0,
              0,
              0,
              "int t = 0; bool b = (t += left) > 0 || (if (flag) { return t * "
              "10; } else { return 50 + t; }); return b ? t + 1 : 7;" },
            { false,
              false,
              0,
              0,
              50,
              "int t = 0; bool b = (t += left) > 0 || (if (flag) { return t * "
              "10; } else { return 50 + t; }); return b ? t + 1 : 7;" },
            { false,
              false,
              4,
              0,
              12,
              "int t = 0; int q = (t += left) ? t * 2 : (if (flag) { return 0 "
              "- 1; } else { return 0 - 2; }); return q + t;" },
            { true,
              false,
              0,
              0,
              -1,
              "int t = 0; int q = (t += left) ? t * 2 : (if (flag) { return 0 "
              "- 1; } else { return 0 - 2; }); return q + t;" },
            { false,
              false,
              0,
              0,
              -2,
              "int t = 0; int q = (t += left) ? t * 2 : (if (flag) { return 0 "
              "- 1; } else { return 0 - 2; }); return q + t;" },
            { true,
              false,
              0,
              0,
              10,
              "int t = 0; int r = (t += 1) + (if (flag) { return t * 10; } "
              "else { return t * 100; }) + (t += 50); return r;" },
            { false,
              false,
              0,
              0,
              100,
              "int t = 0; int r = (t += 1) + (if (flag) { return t * 10; } "
              "else { return t * 100; }) + (t += 50); return r;" },
            { true,
              false,
              0,
              0,
              3,
              "int t = 0; return Twice(Twice(t += 3) + (if (flag) { return t; "
              "} else { return t * 2; }));" },
            { false,
              false,
              0,
              0,
              6,
              "int t = 0; return Twice(Twice(t += 3) + (if (flag) { return t; "
              "} else { return t * 2; }));" },
            { true,
              false,
              4,
              0,
              5,
              "int t = left; t = Twice(t += 1) + (if (flag) { return t; } else "
              "{ return t + 100; }); return 0 - 1;" },
            { false,
              false,
              4,
              0,
              105,
              "int t = left; t = Twice(t += 1) + (if (flag) { return t; } else "
              "{ return t + 100; }); return 0 - 1;" },
            { false,
              false,
              0,
              0,
              11,
              "int t = 0; int r = match (left) { 0 if (t += 1) > 5 -> { return "
              "1; }, 0 -> { return 10 + t; }, _ -> { return 20 + t; } }; "
              "return r;" },
            { false,
              false,
              3,
              0,
              20,
              "int t = 0; int r = match (left) { 0 if (t += 1) > 5 -> { return "
              "1; }, 0 -> { return 10 + t; }, _ -> { return 20 + t; } }; "
              "return r;" },
            { true,
              false,
              0,
              0,
              1,
              "int r = match (left) { _ if flag -> { return 1; }, _ -> 5 }; "
              "return r + 1;" },
            { false,
              false,
              0,
              0,
              6,
              "int r = match (left) { _ if flag -> { return 1; }, _ -> 5 }; "
              "return r + 1;" },
            { false,
              false,
              3,
              0,
              31,
              "int n = 0; while (if (n >= left) { break; } else { true }) { n "
              "+= 1; } return n * 10 + 1;" },
            { false,
              false,
              0,
              0,
              1,
              "int n = 0; while (if (n >= left) { break; } else { true }) { n "
              "+= 1; } return n * 10 + 1;" },
            { false,
              false,
              2,
              0,
              302,
              "int c = 0; int n = 0; while (if ((c += 1) > left) { break; } "
              "else { true }) { n += 1; } return c * 100 + n;" },
            { false,
              false,
              0,
              0,
              100,
              "int c = 0; int n = 0; while (if ((c += 1) > left) { break; } "
              "else { true }) { n += 1; } return c * 100 + n;" },
            { false,
              false,
              3,
              0,
              3,
              "int n = 0; do { n += 1; } while (if (n >= left) { break; } else "
              "{ true }); return n;" },
            { false,
              false,
              0,
              0,
              1,
              "int n = 0; do { n += 1; } while (if (n >= left) { break; } else "
              "{ true }); return n;" },
            { false,
              false,
              2,
              0,
              3,
              "int t = 0; for (int i = 0; if (i > left) { break; } else { i < "
              "10 }; i += 1) { t += i; } return t;" },
            { false,
              false,
              20,
              0,
              45,
              "int t = 0; for (int i = 0; if (i > left) { break; } else { i < "
              "10 }; i += 1) { t += i; } return t;" },
            { false,
              false,
              0,
              0,
              36,
              "int t = 0; for (int i = 0; i < 3; i += 1) { int j = 0; while "
              "(if (j == 2) { break; } else { true }) { j += 1; t += 1; } t += "
              "10; } return t;" },
            { false,
              false,
              0,
              0,
              74,
              "int t = 0; int i = 0; while (i < 4) { i += 1; int j = 0; while "
              "(if (j >= i) { break; } else { true }) { j += 1; if (j == 2) { "
              "continue; } t += 1; } } return t * 10 + i;" },
            { false,
              false,
              2,
              0,
              71,
              "int t = 0; int skips = 0; for (int i = 0; i < 6; i += if (i == "
              "left && skips == 0) { skips += 1; continue; } else { 1 }) { t "
              "+= 1; } return t * 10 + skips;" },
            { false,
              false,
              9,
              0,
              60,
              "int t = 0; int skips = 0; for (int i = 0; i < 6; i += if (i == "
              "left && skips == 0) { skips += 1; continue; } else { 1 }) { t "
              "+= 1; } return t * 10 + skips;" },
            { false,
              false,
              2,
              0,
              304,
              "int t = 0; int u = 0; for (int i = 0; i < 4; i += 1, u += if (i "
              "== left) { continue; } else { 10 }) { t += 1; } return u * 10 + "
              "t;" },
            { false,
              false,
              9,
              0,
              404,
              "int t = 0; int u = 0; for (int i = 0; i < 4; i += 1, u += if (i "
              "== left) { continue; } else { 10 }) { t += 1; } return u * 10 + "
              "t;" },
            { false,
              false,
              1,
              0,
              315,
              "int t = 0; for (int i = 0; i < 3; i += 1) { int c = 0; for (int "
              "j = 0; j < 4; j += if (c == 0 && j == left) { c += 1; continue; "
              "} else { 1 }) { t += 1; } t += c * 100; } return t;" },
            { false,
              false,
              7,
              0,
              12,
              "int t = 0; for (int i = 0; i < 3; i += 1) { int c = 0; for (int "
              "j = 0; j < 4; j += if (c == 0 && j == left) { c += 1; continue; "
              "} else { 1 }) { t += 1; } t += c * 100; } return t;" },
            { true,
              false,
              4,
              0,
              5,
              "int r = while (true) { int q = if (flag) { break left + 1; } "
              "else { 2 }; break q; }; return r;" },
            { false,
              false,
              4,
              0,
              2,
              "int r = while (true) { int q = if (flag) { break left + 1; } "
              "else { 2 }; break q; }; return r;" },
            { false,
              false,
              0,
              0,
              100,
              "int r = while (true) { int q = match (left) { 0 -> { break 100; "
              "}, int n -> n * 2 }; break q; }; return r;" },
            { false,
              false,
              4,
              0,
              8,
              "int r = while (true) { int q = match (left) { 0 -> { break 100; "
              "}, int n -> n * 2 }; break q; }; return r;" },
            { false,
              false,
              2,
              0,
              63,
              "int t = 0; int r = for (int i = 0; ; i += 1) { int inner = "
              "while (true) { t += 1; int q = if (t > left) { break t * 2; } "
              "else { 0 }; t += q; }; if (inner > 0) { break inner + i; } }; "
              "return r * 10 + t;" },
            { false,
              false,
              0,
              0,
              21,
              "int t = 0; int r = for (int i = 0; ; i += 1) { int inner = "
              "while (true) { t += 1; int q = if (t > left) { break t * 2; } "
              "else { 0 }; t += q; }; if (inner > 0) { break inner + i; } }; "
              "return r * 10 + t;" },
            { false,
              false,
              5,
              0,
              2211,
              "int t = 0; int r = Twice(while (true) { t += 1; int q = (t += "
              "10) + (if (t > left) { break t; } else { 1 }); t += q; });  "
              "return r * 100 + t;" },
            { false,
              false,
              30,
              0,
              6834,
              "int t = 0; int r = Twice(while (true) { t += 1; int q = (t += "
              "10) + (if (t > left) { break t; } else { 1 }); t += q; });  "
              "return r * 100 + t;" },
            { true,
              false,
              0,
              0,
              77,
              "int r = while (true) { int q = if (flag) { return 77; } else { "
              "2 }; break q + left; }; return r;" },
            { false,
              false,
              3,
              0,
              5,
              "int r = while (true) { int q = if (flag) { return 77; } else { "
              "2 }; break q + left; }; return r;" },
            { true,
              false,
              0,
              0,
              1,
              "int r = while (true) { if (flag) { return 1; } break 2; }; "
              "return r + 10;" },
            { false,
              false,
              0,
              0,
              12,
              "int r = while (true) { if (flag) { return 1; } break 2; }; "
              "return r + 10;" },
            { false,
              false,
              6,
              0,
              6,
              "int r = while (true) { return left; }; return r + 1;" },
            { false,
              false,
              1,
              0,
              11,
              "int t = 0; int r = while (true) { t += 1; int q = for (int i = "
              "0; ; i += 1) { if (i + t > left) { return i * 10 + t; } if (i "
              "== 2) { break i; } }; if (t == 3) { break q; } }; return 0 - "
              "r;" },
            { false,
              false,
              2,
              0,
              21,
              "int t = 0; int r = while (true) { t += 1; int q = for (int i = "
              "0; ; i += 1) { if (i + t > left) { return i * 10 + t; } if (i "
              "== 2) { break i; } }; if (t == 3) { break q; } }; return 0 - "
              "r;" },
            { false,
              false,
              99,
              0,
              -2,
              "int t = 0; int r = while (true) { t += 1; int q = for (int i = "
              "0; ; i += 1) { if (i + t > left) { return i * 10 + t; } if (i "
              "== 2) { break i; } }; if (t == 3) { break q; } }; return 0 - "
              "r;" },
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
        } };

        // The methods a body may call besides `Run` itself.
        constexpr std::string_view kHelpers
            = "    public static int Twice(_ int value) { return value + "
              "value; }\n";
    } // namespace

    void
    ExerciseLeavingCases()
    {
        ExerciseExecutionCases("Leaving execution", kCases, kHelpers);
    }
} // namespace Visual::XSharp::Fuzzing
