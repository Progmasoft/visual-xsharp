// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1
#pragma once

#include "Visual/XSharp/Analysis/Dominance.hpp"
#include "Visual/XSharp/Xpp/IR.hpp"

namespace visual_xsharp::xpp
{
    // Build the canonical native structural analysis for one verified Xpp
    // function. Optimizers consume this adapter instead of duplicating the
    // terminator-to-edge mapping and accidentally drifting from verifiers.
    [[nodiscard]] auto
    AnalyzeControlStructure(const Function &function) -> Visual::XSharp::Analysis::DominanceResult;
} // namespace visual_xsharp::xpp
