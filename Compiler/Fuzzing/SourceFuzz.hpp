// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1
#pragma once

#include <cstdint>
#include <span>

namespace Visual::XSharp::Fuzzing
{
    // These routes deliberately share production Haskell entry points. The
    // syntax stages stop after the lexer or parser; source compilation goes
    // through verified Core, Xpp, Xmm and LLVM lowering.
    void
    ExerciseLexer(std::span<const std::uint8_t> input);
    void
    ExerciseParser(std::span<const std::uint8_t> input);
    void
    ExerciseSourceToLlvm(std::span<const std::uint8_t> input);
    void
    ExerciseDifferentialOracle(std::span<const std::uint8_t> input);
} // namespace Visual::XSharp::Fuzzing
