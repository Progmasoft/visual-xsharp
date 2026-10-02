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
    // four cycling selectors above happen to select. The two leaves are the
    // literals 0 and 4, so a form that tests its generated expression sees
    // both a false and a true value.
    constexpr std::uint8_t kModes = 9U;
    constexpr std::uint8_t kLimits = 12U;
    constexpr std::array<std::uint8_t, 2U> leaves{ 0U, 4U };
    for (const auto leaf : leaves)
    {
        for (std::uint8_t mode = 0U; mode < kModes; ++mode)
        {
            for (std::uint8_t limit = 0U; limit < kLimits; ++limit)
            {
                const std::array<std::uint8_t, 3U> seed{ leaf, mode, limit };
                llvm::errs() << "Differential smoke: leaf "
                             << static_cast<unsigned>(leaf) << " mode "
                             << static_cast<unsigned>(mode) << " limit "
                             << static_cast<unsigned>(limit) << '\n';
                Visual::XSharp::Fuzzing::ExerciseDifferentialOracle(seed);
            }
        }
    }
    return 0;
}
