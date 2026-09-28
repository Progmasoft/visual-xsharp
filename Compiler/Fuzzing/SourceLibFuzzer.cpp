// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <span>

#include "SourceFuzz.hpp"

#ifndef VXS_SOURCE_FUZZ_STAGE
#    error VXS_SOURCE_FUZZ_STAGE must select one dedicated fuzz target
#endif

extern "C" int
LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    const std::span<const std::uint8_t> input(data, size);
    try
    {
#if VXS_SOURCE_FUZZ_STAGE == 0
        Visual::XSharp::Fuzzing::ExerciseLexer(input);
#elif VXS_SOURCE_FUZZ_STAGE == 1
        Visual::XSharp::Fuzzing::ExerciseParser(input);
#elif VXS_SOURCE_FUZZ_STAGE == 2
        Visual::XSharp::Fuzzing::ExerciseSourceToLlvm(input);
        Visual::XSharp::Fuzzing::ExerciseDifferentialOracle(input);
#else
#    error Unsupported VXS_SOURCE_FUZZ_STAGE
#endif
        return 0;
    }
    catch (const std::exception &exception)
    {
        // A C++ exception must never unwind through libFuzzer's C ABI. Print
        // the oracle diagnostic before turning it into a sanitizer-visible
        // failure that preserves the exact reproducing input.
        std::fputs("Visual X# source fuzz failure: ", stderr);
        std::fputs(exception.what(), stderr);
        std::fputc('\n', stderr);
        std::fflush(stderr);
        std::abort();
    }
}
