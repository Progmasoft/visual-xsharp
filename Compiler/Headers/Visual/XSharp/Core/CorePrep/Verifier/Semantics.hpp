// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
#pragma once

#include "Visual/XSharp/Core/CorePrep/Verifier.hpp"

namespace visual_xsharp::core
{
    [[nodiscard]] auto
    verify_semantics(const CorePrepModule &module) -> std::vector<VerificationIssue>;
} // namespace visual_xsharp::core
