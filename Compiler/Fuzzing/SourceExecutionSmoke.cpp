// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

#include <array>
#include <cstdint>
#include <llvm/Support/raw_ostream.h>
#include <span>
#include <string_view>

#include "BranchingExecutionCases.hpp"
#include "ExecutionCases.hpp"
#include "ExpressionExecutionCases.hpp"
#include "LeavingExecutionCases.hpp"
#include "SourceFuzz.hpp"
#include "Visual/XSharp/Runtime/AARC.hpp"
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
    // are compared on them as well. These are compiled and verified; the
    // table of closure cases below runs closures.
    constexpr std::array<std::string_view, 11U> kOwnershipCases{ {
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
        // A continue in a loop condition and a break in a loop update,
        // each with a closure alive at the transfer.
        "namespace Fuzz; class Program { "
        "public static int Step(_ int n) { return n > 0 ? 1 + Step(n - 1) "
        ": 0; } "
        "public static int Evaluate() { int a = Step(4); int c = 0; "
        "auto limit = [of = a] \\ -> of; int n = 0; "
        "while (if ((c += 1) < limit()) { continue; } else { n < 2 }) { "
        "auto tick = [by = n] \\ -> by + 1; n = tick(); } "
        "for (int i = 0; i < 9; i += if (i == limit()) { break; } else { 1 "
        "}) { auto add = [by = i] \\(int w) -> w + by; n = add(n); } "
        "return c * 100 + n; } }",
        // A callable created inside a callable: the inner one reads a
        // parameter of the outer one and a local of the method, which the
        // outer one must capture for it and nothing else.
        "namespace Fuzz; class Program { "
        "public static int Step(_ int n) { return n > 0 ? 1 + Step(n - 1) "
        ": 0; } "
        "public static int Evaluate() { int k = Step(4); "
        "auto outer = \\(int v) -> { auto inner = \\(int w) -> w + k + v; "
        "return inner(v) * 2; }; return outer(k); } }",
        // Three levels, with explicit and implicit captures mixed.
        "namespace Fuzz; class Program { "
        "public static int Step(_ int n) { return n > 0 ? 1 + Step(n - 1) "
        ": 0; } "
        "public static int Evaluate() { int k = Step(4); "
        "auto a = [k] \\(int v) -> { auto b = \\(int w) -> { "
        "auto c = [k, v, w] \\(int x) -> x + w * 10 + v * 100 + k * 1000; "
        "return c(1); }; return b(2); }; return a(3); } }",
        // An inner callable that outlives the call that created it.
        "namespace Fuzz; class Program { "
        "public static int Step(_ int n) { return n > 0 ? 1 + Step(n - 1) "
        ": 0; } "
        "public static int Evaluate() { int k = Step(4); "
        "auto make = \\(int v) -> { auto inner = \\(int w) -> w + v; "
        "return inner; }; auto f = make(k); auto g = make(k + 2); "
        "return f(2) * 100 + g(2); } }",
    } };

    // Methods whose return type is inferred, called from the bodies below.
    // A chain is declared against the order of its inference, and two
    // methods return each other.
    constexpr std::string_view kInferredHelpers
        = "    public static auto Twice(_ int value) { return value + "
          "value; }\n"
          "    public static auto Factorial(_ int value) { if (value <= 1) "
          "{ return 1; } return value * Factorial(value - 1); }\n"
          "    public static auto First(_ int v) { return Second(v) + 1; "
          "}\n"
          "    public static auto Second(_ int v) { return Third(v) + 10; "
          "}\n"
          "    public static auto Third(_ int v) { return v * 2; }\n"
          "    public static auto Even(_ int v) { if (v == 0) { return "
          "true; } return Odd(v - 1); }\n"
          "    public static auto Odd(_ int v) { if (v == 0) { return "
          "false; } return Even(v - 1); }\n"
          "    public static auto Pick(_ int v) { int q = if (v > 0) { "
          "return v * 3; } else { 5 }; return q + 1; }\n"
          "    public static auto Scan(_ int v) { int q = while (true) { "
          "if (v > 3) { return 7; } break 2; }; return q + v; }\n";

    // Closures that are created and called. Each expected value is worked
    // out by hand from the capture rules: a capture initializer is
    // evaluated once, where the closure is created; a closure created
    // inside another reads the names around both through the outer one;
    // and a closure keeps what it captured after the call that created it
    // has returned.
    constexpr auto kClosureCases = std::to_array<
        Visual::XSharp::Fuzzing::ExecutionCase>({
        { false,
          false,
          3,
          0,
          8,
          "auto outer = \\(int v) -> { auto inner = \\(int w) -> w + 1; "
          "return inner(v) * 2; }; return outer(left);" },
        { false,
          false,
          3,
          3,
          18,
          "int k = left; auto outer = \\(int v) -> { auto inner = "
          "\\(int w) -> w + k + v; return inner(v) * 2; }; "
          "return outer(right);" },
        { false,
          false,
          5,
          1,
          14,
          "int k = left; auto outer = \\(int v) -> { auto inner = "
          "\\(int w) -> w + k + v; return inner(v) * 2; }; "
          "return outer(right);" },
        { false,
          false,
          3,
          3,
          9,
          "int k = left; auto outer = [k] \\(int v) -> { auto inner = "
          "[k, v] \\(int w) -> w + k + v; return inner(v); }; "
          "return outer(right);" },
        { false,
          false,
          4,
          0,
          4321,
          "int k = left; auto a = \\(int v) -> { auto b = \\(int w) -> { "
          "auto c = \\(int x) -> x + w * 10 + v * 100 + k * 1000; "
          "return c(1); }; return b(2); }; return a(3);" },
        { false,
          false,
          5,
          7,
          709,
          "auto make = \\(int v) -> { auto inner = \\(int w) -> w + v; "
          "return inner; }; auto f = make(left); auto g = make(right); "
          "return f(2) * 100 + g(2);" },
        { false,
          false,
          1,
          0,
          111,
          "int k = left; auto held = [kept = k] \\ -> kept; k += 10; "
          "return held() * 100 + k;" },
        { false,
          false,
          3,
          0,
          12,
          "auto pick = \\(int v) -> { int q = if (v > 0) { return 1; } "
          "else { 2 }; return q; }; "
          "return pick(left) * 10 + pick(0 - left);" },
        { false,
          false,
          1,
          0,
          307,
          "auto scan = \\(int v) -> { int q = while (true) { "
          "if (v > 3) { return 7; } break 2; }; return q + v; }; "
          "return scan(left) * 100 + scan(left + 3);" },
        { false,
          false,
          3,
          0,
          6,
          "int n = 0; int t = 0; while (if (n >= left) { break; } else { "
          "true }) { auto step = [by = n] \\ -> by + 1; n = step(); "
          "t += n; } return t;" },
        { false,
          false,
          3,
          0,
          6,
          "int t = 0; for (int i = 0; i < 10; i += if (i == left) { "
          "break; } else { 1 }) { auto add = [by = i] \\(int w) -> w + "
          "by; t = add(t); } return t;" },
        // A closure passed to a method, made by one and returned by one.
        { false, false, 4, 0, 12, "return Apply(\\(int w) -> w * 3, left);" },
        { false,
          false,
          5,
          2,
          711,
          "auto add = Adder(left); auto ten = Adder(10); "
          "return add(right) * 100 + ten(1);" },
        { false,
          false,
          3,
          0,
          8,
          "auto f = \\(int w) -> w + 1; auto g = Pass(f); "
          "return g(left) + f(left);" },
        { false,
          false,
          5,
          1,
          6,
          "auto a = \\(int w) -> w + 1; auto b = \\(int w) -> w * 2; "
          "auto c = Choose(left > right, a, b); return c(left);" },
        { false,
          false,
          5,
          9,
          10,
          "auto a = \\(int w) -> w + 1; auto b = \\(int w) -> w * 2; "
          "auto c = Choose(left > right, a, b); return c(left);" },
        // A method named where a value is expected.
        { false,
          false,
          3,
          4,
          14,
          "auto f = Plain; return Apply(f, left) + Apply(Plain, right);" },
        // A closure variable assigned again, in a loop and from itself.
        { false,
          false,
          3,
          0,
          103,
          "auto f = Adder(0); for (int i = 1; i <= left; i += 1) { "
          "f = Adder(i); } return f(100);" },
        { false,
          false,
          0,
          0,
          100,
          "auto f = Adder(0); for (int i = 1; i <= left; i += 1) { "
          "f = Adder(i); } return f(100);" },
        { false,
          false,
          5,
          0,
          7,
          "auto f = Adder(1); f = Compose2(f); return f(left);" },
        // Closures that exist on one branch only.
        { false,
          false,
          4,
          1,
          5,
          "int r = 0; if (left > right) { auto f = Adder(left); "
          "r = f(1); } else { auto g = Adder(right); "
          "auto h = Compose2(g); r = h(1); } return r;" },
        { false,
          false,
          1,
          3,
          7,
          "int r = 0; if (left > right) { auto f = Adder(left); "
          "r = f(1); } else { auto g = Adder(right); "
          "auto h = Compose2(g); r = h(1); } return r;" },
        // A closure result that nothing receives.
        { false,
          false,
          5,
          0,
          7,
          "_ = Adder(left); auto f = Adder(2); _ = Adder(3); "
          "return f(left);" },
        // A closure kept alive only by the closure that captured it.
        { false,
          false,
          5,
          1,
          12,
          "auto inner = Adder(left); auto outer = \\(int w) -> "
          "inner(w) * 2; return outer(right);" },
        // A closure made in every pass of a loop that returns from
        // its middle.
        { false,
          false,
          4,
          0,
          103,
          "for (int i = 0; i < 5; i += 1) { auto f = Adder(i); "
          "if (f(left) > 6) { return f(100); } } return 0;" },
        { false,
          false,
          0,
          0,
          0,
          "for (int i = 0; i < 5; i += 1) { auto f = Adder(i); "
          "if (f(left) > 6) { return f(100); } } return 0;" },
    });

    // Methods that take, make and return closures. A parameter is borrowed
    // and a result is owned, so `Pass` and `Choose` must hand out a
    // reference of their own, and the closure of `Compose2` must keep the
    // closure it was given.
    constexpr std::string_view kClosureHelpers
        = "    public static int Apply(_ (int) -> int f, _ int v) { return "
          "f(v); }\n"
          "    public static (int) -> int Adder(_ int n) { return \\(int w) "
          "-> w + n; }\n"
          "    public static (int) -> int Pass(_ (int) -> int f) { return "
          "f; }\n"
          "    public static (int) -> int Choose(_ bool c, _ (int) -> int a, "
          "_ (int) -> int b) { if (c) { return a; } return b; }\n"
          "    public static (int) -> int Compose2(_ (int) -> int f) { "
          "return \\(int w) -> f(f(w)); }\n"
          "    public static int Plain(_ int value) { return value + value; "
          "}\n";

    // Written here by hand, apart from the generated tables and from the
    // frontend tests: the expected values are worked out from the methods
    // above, so these runs do not share an expectation with any other
    // table.
    constexpr auto kInferredCases = std::to_array<
        Visual::XSharp::Fuzzing::ExecutionCase>({
        { false, false, 3, 4, 30, "return Twice(left) + Factorial(right);" },
        { false, false, 0, 1, 1, "return Twice(left) + Factorial(right);" },
        { false, false, 4, 0, 19, "return First(left);" },
        { false,
          false,
          4,
          0,
          12,
          "return (Even(left) ? 10 : 20) + (Odd(left) ? 1 : 2);" },
        { false,
          false,
          3,
          0,
          21,
          "return (Even(left) ? 10 : 20) + (Odd(left) ? 1 : 2);" },
        { false, false, 2, 0, 66, "return Pick(left) * 10 + Pick(0 - left);" },
        { false,
          false,
          1,
          0,
          307,
          "return Scan(left) * 100 + Scan(left + 3);" },
        { false,
          false,
          3,
          0,
          6,
          "int x = Twice(left); bool b = Even(x); return b ? x : 0 - x;" },
    });

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
        // Hand-written results for calls of methods whose return type is
        // inferred.
        Visual::XSharp::Fuzzing::ExerciseExecutionCases(
            "Inferred return execution",
            kInferredCases,
            kInferredHelpers);
        // The JIT finds the runtime of the closure cases in this process.
        // The call also keeps the runtime in the program where the linker
        // would otherwise leave an unreferenced library out.
        if (vxs_aarc_abi_version() != VXS_AARC_ABI_VERSION)
        {
            llvm::errs() << "the linked AARC runtime has another ABI version\n";
            return 1;
        }
        // Hand-written results for closures: created, called, nested and
        // returned, through LLVM and the runtime that owns them.
        // Each case is a program of its own, and the runtime must hold no
        // more allocations after it than before: a closure that is not
        // released, or a capture its destructor does not release, is a
        // failure here on every platform, with the program that leaked.
        for (const auto &closureCase : kClosureCases)
        {
            const auto before
                = Visual::XSharp::Runtime::Aarc::LiveAllocations();
            Visual::XSharp::Fuzzing::ExerciseExecutionCases(
                "Closure execution",
                std::span(&closureCase, 1U),
                kClosureHelpers);
            const auto after = Visual::XSharp::Runtime::Aarc::LiveAllocations();
            if (after != before)
            {
                llvm::errs()
                    << "closure case left " << (after - before)
                    << " AARC allocation(s) behind: " << closureCase.body
                    << '\n';
                return 1;
            }
        }
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
        const auto result = Smoke();
        // What the compiler stack held for all of the above, the programs
        // at the nesting limits among them. The line is how the figure is
        // read on platforms where nobody measures by hand; zero means the
        // platform does not report it.
        llvm::errs() << "compiler stack committed: "
                     << Visual::XSharp::Support::CommittedStackBytes() / 1024U
                     << " KiB\n";
        return result;
    });
}
