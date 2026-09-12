// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1
#pragma once

#include <vector>

#include "Visual/XSharp/Xmm/Verifier.hpp"

namespace Visual::XSharp::Xmm
{
    // Ownership verification is deliberately separate from structural Xmm
    // verification. The pass consumes already explicit AARC operations and reasons
    // about their control-flow lifetime without changing the IR.
    [[nodiscard]] auto
    VerifyOwnership(const ::visual_xsharp::xmm::Function &function)
        -> std::vector<VerificationIssue>;
} // namespace Visual::XSharp::Xmm
