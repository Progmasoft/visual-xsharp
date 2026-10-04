// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

#include <array>
#include <cstdint>
#include <llvm/Support/raw_ostream.h>
#include <string_view>

#include "BranchingExecutionCases.hpp"
#include "ExpressionExecutionCases.hpp"
#include "LeavingExecutionCases.hpp"
#include "SourceFuzz.hpp"
#include "Visual/XSharp/Support/CompilerStack.hpp"

// The executable regressions with hand-written results: every program of the
// expression, branching and leaving tables runs through CorePrep, Xpp, Xmm,
// LLVM and the ORC JIT, unoptimized and optimized, and must return its expected
// value from both. They are a program of their own, beside `source_fuzz_smoke`,
// because each of the two is a deterministic check under one process
// watchdog: together their work is about twice what either does alone, and a
// watchdog is meant to end a run that never finishes, not to bound the size
// of a test table.

namespace
{
    // Programs that own closures while control leaves through a block used
    // as a value. The ownership-flow verifiers of Xpp and Xmm run on every
    // program compiled here, so a path that left without releasing what it
    // owns, or released it twice, is rejected. The two CorePrep lowerings
    // are compared on them as well. They are compiled and verified, not
    // run: the JIT of this harness does not link lifted closures.
    constexpr std::array<std::string_view, 7U> kOwnershipCases{ {
        // An initializer that never completes, after a closure was created.
        "namespace Fuzz; class Program { "
        "public static int Step(_ int n) { return n > 0 ? 1 + Step(n - 1) "
        ": 0; } "
        "public static int Evaluate() { int a = Step(4); "
        "auto held = [kept = a] \\ -> kept; "
        "int r = if (held() > 3) { return held(); } else { return 0; }; "
        "return r; } }",
        // A do/while whose condition never completes, reached after the
        // body and after a continue, with a closure created in the body.
        "namespace Fuzz; class Program { "
        "public static int Step(_ int n) { return n > 0 ? 1 + Step(n - 1) "
        ": 0; } "
        "public static int Evaluate() { int a = Step(4); "
        "auto held = [kept = a] \\ -> kept; int n = 0; "
        "do { n += held(); auto inner = [seen = n] \\ -> seen + 1; "
        "if (inner() < 3) { continue; } } "
        "while (if (n > 6) { return n; } else { return 0 - n; }); "
        "return 0; } }",
        // Break and continue out of value blocks in nested loops, with
        // closures created in both loops.
        "namespace Fuzz; class Program { "
        "public static int Step(_ int n) { return n > 0 ? 1 + Step(n - 1) "
        ": 0; } "
        "public static int Evaluate() { int a = Step(4); int t = 0; "
        "for (int i = 0; i < a; i += 1) { "
        "auto step = [by = i] \\ -> by + 1; int j = 0; "
        "while (j < 3) { j += 1; auto pick = [at = j] \\ -> at; "
        "t += if (pick() == 2) { break; } else { step() }; } "
        "t += match (i) { 1 -> { continue; }, int n -> step() + n }; } "
        "return t; } }",
        // A value carried out of a value block to a loop expression, a
        // break in a loop condition and a continue in a loop update, each
        // with a closure alive at the transfer.
        "namespace Fuzz; class Program { "
        "public static int Step(_ int n) { return n > 0 ? 1 + Step(n - 1) "
        ": 0; } "
        "public static int Evaluate() { int a = Step(4); int t = 0; "
        "int found = while (true) { t += 1; "
        "auto seen = [at = t] \\ -> at * 2; "
        "int q = if (seen() > a) { break seen(); } else { 0 }; t += q; }; "
        "auto keep = [of = found] \\ -> of; int n = 0; "
        "while (if (n > keep()) { break; } else { true }) { n += 1; } "
        "for (int i = 0; i < 4; i += if (i == 1) { continue; } else { 1 }) "
        "{ auto tick = [by = i] \\ -> by; n += tick(); if (i == 1) { i += 2; "
        "} } return found + n; } }",
        // A return out of a value block inside a loop expression, with
        // closures created in the loop.
        "namespace Fuzz; class Program { "
        "public static int Step(_ int n) { return n > 0 ? 1 + Step(n - 1) "
        ": 0; } "
        "public static int Evaluate() { int a = Step(4); int t = 0; "
        "int r = while (true) { t += 1; auto seen = [at = t] \\ -> at; "
        "int q = if (seen() > a) { return seen() * 10; } else { seen() }; "
        "if (q == 3) { break q; } }; return r; } }",
        // Callables whose result type is inferred from returns that stand
        // in a value block and in a loop expression.
        "namespace Fuzz; class Program { "
        "public static int Step(_ int n) { return n > 0 ? 1 + Step(n - 1) "
        ": 0; } "
        "public static int Evaluate() { int a = Step(4); "
        "auto pick = \\(int v) -> { int q = if (v > 0) { return 1; } "
        "else { 2 }; return q; }; "
        "auto scan = [limit = a] \\(int v) -> { int q = while (true) { "
        "if (v > limit) { return 7; } break 2; }; return q + v; }; "
        "return pick(a) + scan(a); } }",
        // A callable every path of which returns from a value block, and
        // one whose returns are a different type from its creator's.
        "namespace Fuzz; class Program { "
        "public static int Step(_ int n) { return n > 0 ? 1 + Step(n - 1) "
        ": 0; } "
        "public static int Evaluate() { int a = Step(4); "
        "auto twice = \\(int v) -> { int q = if (v > 0) { return v * 2; } "
        "else { return 0 - v; }; return q; }; "
        "auto positive = \\(int w) -> { bool b = if (w > 0) { return true; "
        "} else { false }; return b; }; "
        "int q = if (positive(a)) { return twice(a); } else { 20 }; "
        "return q; } }",
    } };

    int
    Smoke()
    {
        // Hand-written results for assignments and increments used as values
        // and for loops used as expressions.
        Visual::XSharp::Fuzzing::ExerciseExpressionCases();
        // Hand-written results for match, if expressions and guard.
        Visual::XSharp::Fuzzing::ExerciseBranchingCases();
        // Hand-written results for expressions that leave instead of
        // yielding a value.
        Visual::XSharp::Fuzzing::ExerciseLeavingCases();
        for (const auto text : kOwnershipCases)
        {
            llvm::errs() << "Ownership verification: " << text << '\n';
            // The bytes of the text are the input, as a fuzz target gets it.
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
            const auto *const bytes
                = reinterpret_cast<const std::uint8_t *>(text.data());
            Visual::XSharp::Fuzzing::ExerciseAcceptedSource(
                { bytes, text.size() });
        }
        return 0;
    }
} // namespace

int
main()
{
    // The programs include ones nested up to the frontend's limits, which
    // only compile on the stack the compiler runs on in `vxs`.
    return Visual::XSharp::Support::RunOnCompilerStack([] {
        return Smoke();
    });
}
