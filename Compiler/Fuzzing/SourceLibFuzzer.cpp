// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

#include <cstddef>
#include <cstdint>
#include <span>

#include "SourceFuzz.hpp"

#ifndef VXS_SOURCE_FUZZ_STAGE
#    error VXS_SOURCE_FUZZ_STAGE must select one dedicated fuzz target
#endif

extern "C" int
LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    const std::span<const std::uint8_t> input(data, size);
#if VXS_SOURCE_FUZZ_STAGE == 0
    Visual::XSharp::Fuzzing::ExerciseLexer(input);
#elif VXS_SOURCE_FUZZ_STAGE == 1
    Visual::XSharp::Fuzzing::ExerciseParser(input);
#elif VXS_SOURCE_FUZZ_STAGE == 2
    // Arbitrary source and generated arithmetic have independent corpora and
    // time budgets. An invalid source mutation should not pay for two JITs.
    Visual::XSharp::Fuzzing::ExerciseSourceToLlvm(input);
#elif VXS_SOURCE_FUZZ_STAGE == 3
    Visual::XSharp::Fuzzing::ExerciseDifferentialOracle(input);
#else
#    error Unsupported VXS_SOURCE_FUZZ_STAGE
#endif
    // Oracle invariant failures terminate directly. No exception can unwind
    // through the C ABI, and libFuzzer retains the reproducing input.
    return 0;
}
