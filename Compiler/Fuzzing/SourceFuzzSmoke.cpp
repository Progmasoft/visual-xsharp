// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

#include <array>
#include <cstdint>
#include <llvm/Support/raw_ostream.h>
#include <string_view>

#include "SourceFuzz.hpp"

int
main()
{
    constexpr std::string_view sourceText
        = "namespace Demo; public class Program { public static long "
          "Evaluate() { return 5; } }";
    const auto source = std::span<const std::uint8_t>(
        reinterpret_cast<const std::uint8_t *>(sourceText.data()),
        sourceText.size());
    constexpr std::array<std::uint8_t, 8U> expressionSeed{ 1U, 2U, 3U, 4U,
                                                           5U, 6U, 7U, 8U };
    Visual::XSharp::Fuzzing::ExerciseLexer(source);
    Visual::XSharp::Fuzzing::ExerciseParser(source);
    const std::span<const std::uint8_t> emptySource;
    Visual::XSharp::Fuzzing::ExerciseSourceToLlvm(emptySource);
    Visual::XSharp::Fuzzing::ExerciseSourceToLlvm(source);
    // Control-flow shapes whose frontend and native CorePrep lowerings must
    // agree. They combine forms the generated programs below keep separate:
    // loops inside loops, short-circuit operators as loop conditions, and
    // several functions sharing one module-wide symbol numbering.
    constexpr std::array<std::string_view, 4U> accepted{
        "namespace Parity; class Program { public static int Evaluate() { "
        "int total = 0; for (int outer = 0; outer < 4; outer++) { "
        "if (outer == 2) { continue; } int inner = 0; "
        "while (inner < 3) { if (inner == 1) { inner++; continue; } "
        "total = total + outer + inner; inner++; } } return total; } }",
        "namespace Parity; class Program { public static int Evaluate() { "
        "int index = 0; int total = 0; "
        "while (index < 9 && (index < 4 || total < 20)) { "
        "total = total + index; index++; } "
        "do { total++; } while (total < 40 && index \\= 0); "
        "return total; } }",
        "namespace Parity; class Program { "
        "public static bool Down(_ int n) { return n == 0 || Down(n - 1); } "
        "public static int Pick(_ int n) { if (Down(n) && n < 6) { "
        "return n; } return 0 - 1; } "
        "public static int Evaluate() { return Pick(3) + Pick(8); } }",
        "namespace Parity; class First { public static long Evaluate() { "
        "long result = 8; if (result > 4) { result = result - 2; } "
        "return result; } } class Second { public static long Other() { "
        "long value = 3; for (int step = 0; step < 2; step++) { "
        "value = value * 2; } return value; } }",
    };
    for (const auto text : accepted)
        Visual::XSharp::Fuzzing::ExerciseAcceptedSource(
            std::span<const std::uint8_t>(
                reinterpret_cast<const std::uint8_t *>(text.data()),
                text.size()));
    llvm::errs() << "Differential smoke: mixed seed\n";
    Visual::XSharp::Fuzzing::ExerciseDifferentialOracle(expressionSeed);
    llvm::errs() << "Differential smoke: empty seed\n";
    Visual::XSharp::Fuzzing::ExerciseDifferentialOracle(emptySource);
    // Constant seeds force leaves, full-depth addition, subtraction and
    // multiplication. Exercise the independent oracle before a mutation
    // campaign so a missing generated-code route cannot appear as success.
    constexpr std::array<std::uint8_t, 4U> selectors{ 252U, 253U, 254U, 255U };
    for (const auto selector : selectors)
    {
        const std::array<std::uint8_t, 1U> seed{ selector };
        llvm::errs() << "Differential smoke: selector "
                     << static_cast<unsigned>(selector) << '\n';
        Visual::XSharp::Fuzzing::ExerciseDifferentialOracle(seed);
    }
    // A leaf selector followed by an explicit mode and limit byte reaches
    // every generated control-flow shape at every trip count, including the
    // zero-trip, continue and break paths, instead of only the shapes the
    // four cycling selectors above happen to select.
    constexpr std::uint8_t kModes = 6U;
    constexpr std::uint8_t kLimits = 12U;
    for (std::uint8_t mode = 0U; mode < kModes; ++mode)
    {
        for (std::uint8_t limit = 0U; limit < kLimits; ++limit)
        {
            const std::array<std::uint8_t, 3U> seed{ 0U, mode, limit };
            llvm::errs() << "Differential smoke: mode "
                         << static_cast<unsigned>(mode) << " limit "
                         << static_cast<unsigned>(limit) << '\n';
            Visual::XSharp::Fuzzing::ExerciseDifferentialOracle(seed);
        }
    }
    return 0;
}
