// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

#include <array>
#include <cstdint>
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
    Visual::XSharp::Fuzzing::ExerciseDifferentialOracle(expressionSeed);
    return 0;
}
