// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

#include <array>
#include <cstdint>
#include <llvm/Support/raw_ostream.h>
#include <string>
#include <string_view>

#include "BranchingExecutionCases.hpp"
#include "SourceFuzz.hpp"

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
        struct Case final
        {
            bool flag;
            bool other;
            int left;
            int right;
            std::int64_t expected;
            std::string_view body;
        };

        constexpr std::array<Case, 83U> kCases{ {
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

        [[nodiscard]] auto
        Truth(bool value) -> std::string
        {
            // `Id` is recursive, so the optimizer cannot fold the arguments
            // away and the body really executes on run-time values.
            return value ? "Id(1) > 0" : "Id(0) > 0";
        }

        [[nodiscard]] auto
        Program(const Case &entry) -> std::string
        {
            return "namespace Fuzz;\n"
                   "class Program {\n"
                   "    public static int Id(_ int n) { return n > 0 ? 1 + "
                   "Id(n - 1) : 0; }\n"
                   "    public static int Twice(_ int value) { return value + "
                   "value; }\n"
                   "    public static int Run(_ bool flag, _ bool other, _ int "
                   "left, _ int right) {\n        "
                   + std::string(entry.body)
                   + "\n    }\n"
                     "    public static int Evaluate() { return Run("
                   + Truth(entry.flag) + ", " + Truth(entry.other) + ", Id("
                   + std::to_string(entry.left) + "), Id("
                   + std::to_string(entry.right) + ")); }\n}\n";
        }
        // A match with far more arms than the lowering nests in one group.
        // Arm `index` yields `index * 3 + 1` and the catch-all yields 0.
        [[nodiscard]] auto
        WideMatchBody(int arms) -> std::string
        {
            std::string body = "return match (left) { ";
            for (int index = 0; index < arms; ++index)
                body += std::to_string(index) + " -> "
                        + std::to_string(index * 3 + 1) + ", ";
            return body + "_ -> 0 };";
        }

        // The same table written as an `else if` chain, which reaches Core
        // as one level of nesting per link.
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
    } // namespace

    void
    ExerciseBranchingCases()
    {
        for (const auto &entry : kCases)
        {
            llvm::errs() << "Branching execution: " << entry.body << '\n';
            ExerciseExpectedValue(Program(entry), entry.expected);
        }
        // The subjects select the first arm of the second group and the
        // catch-all; every run compiles the whole match again, so there are
        // only two. A
        // lowering that nested one level per arm would overflow the stack
        // of the stages after Core long before this many arms.
        constexpr int kWideArms = 200;
        constexpr std::array<int, 2U> subjects{ 16, 200 };
        const auto wide = WideMatchBody(kWideArms);
        for (const auto subject : subjects)
        {
            const Case entry{ false,
                              false,
                              subject,
                              0,
                              subject < kWideArms ? subject * 3 + 1 : 0,
                              wide };
            llvm::errs() << "Branching execution: match of " << kWideArms
                         << " arms on " << subject << '\n';
            ExerciseExpectedValue(Program(entry), entry.expected);
        }
        // An `else if` chain of 300 links. The native wire reader, the Core
        // verifier and the CorePrep adapter walk a chain in a loop; when
        // they recursed, 150 links overflowed the stack. Both CorePrep
        // lowerings are compared on it as on every other program here.
        constexpr int kChainLinks = 300;
        constexpr std::array<int, 2U> selected{ 299, 300 };
        const auto chain = ElseIfChainBody(kChainLinks);
        for (const auto subject : selected)
        {
            const Case entry{ false,
                              false,
                              subject,
                              0,
                              subject < kChainLinks ? subject * 3 + 1 : 0,
                              chain };
            llvm::errs() << "Branching execution: else-if chain of "
                         << kChainLinks << " links on " << subject << '\n';
            ExerciseExpectedValue(Program(entry), entry.expected);
        }
        // Programs at the nesting limits of the frontend: a statement at
        // level 256 and an expression at level 1024. They compile only on
        // the compiler stack, which the smoke program runs on like `vxs`.
        {
            constexpr int kLevels = 255;
            std::string nested = "int total = 0; ";
            for (int index = 0; index < kLevels; ++index)
                nested += "if (left > " + std::to_string(index) + ") { ";
            nested += "total += 1; ";
            for (int index = 0; index < kLevels; ++index)
                nested += "} ";
            nested += "return total;";
            constexpr std::array<int, 1U> arguments{ kLevels };
            for (const auto argument : arguments)
            {
                const Case entry{
                    false, false, argument, 0, argument == kLevels ? 1 : 0,
                    nested
                };
                llvm::errs() << "Branching execution: " << kLevels
                             << " nested if statements on " << argument << '\n';
                ExerciseExpectedValue(Program(entry), entry.expected);
            }
            constexpr int kOperands = 1024;
            std::string sum = "return left";
            for (int index = 1; index < kOperands; ++index)
                sum += " + left";
            sum += ";";
            const Case entry{ false, false, 3, 0, 3 * kOperands, sum };
            llvm::errs() << "Branching execution: sum of " << kOperands
                         << " operands\n";
            ExerciseExpectedValue(Program(entry), entry.expected);
        }
    }
} // namespace Visual::XSharp::Fuzzing
