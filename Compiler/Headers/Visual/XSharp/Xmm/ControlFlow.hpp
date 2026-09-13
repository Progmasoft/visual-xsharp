// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1
#pragma once

#include "Visual/XSharp/Analysis/Dominance.hpp"
#include "Visual/XSharp/Xmm/IR.hpp"

namespace visual_xsharp::xmm
{
    // Xmm preserves Xpp block identity, but it owns its terminator enum. This
    // adapter makes that boundary explicit and keeps structural facts shared.
    [[nodiscard]] auto
    AnalyzeControlStructure(const Function &function) -> Visual::XSharp::Analysis::DominanceResult;
} // namespace visual_xsharp::xmm
