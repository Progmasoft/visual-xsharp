// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1
#pragma once

#include <cstdint>
#include <span>

namespace Visual::XSharp::Fuzzing
{
    // These routes deliberately share production Haskell entry points. The
    // syntax stages stop after the lexer or parser; source compilation goes
    // through verified Core, Xpp, Xmm and LLVM lowering. Every accepted
    // source is also lowered to CorePrep by both the frontend and the native
    // adapter, and the two results must be structurally equal.
    void
    ExerciseLexer(std::span<const std::uint8_t> input);
    void
    ExerciseParser(std::span<const std::uint8_t> input);
    void
    ExerciseSourceToLlvm(std::span<const std::uint8_t> input);
    void
    ExerciseDifferentialOracle(std::span<const std::uint8_t> input);
    /// Like ExerciseSourceToLlvm, but the source is known to be valid: a
    /// frontend rejection is a failure. Deterministic checks use this so a
    /// program that silently stopped compiling cannot pass as "rejected".
    void
    ExerciseAcceptedSource(std::span<const std::uint8_t> input);
} // namespace Visual::XSharp::Fuzzing
