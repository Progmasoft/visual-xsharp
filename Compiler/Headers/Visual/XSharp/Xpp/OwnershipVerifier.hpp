// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1
#pragma once

#include <vector>

#include "Visual/XSharp/Xpp/Verifier.hpp"

namespace Visual::XSharp::Xpp
{
    // The module-level entry point lets the adapter distinguish direct function
    // symbols from register-backed closure values before running generic ownership
    // dataflow for each function body.
    [[nodiscard]] auto
    VerifyOwnership(const ::visual_xsharp::xpp::Module &module)
        -> std::vector<VerificationIssue>;
} // namespace Visual::XSharp::Xpp
