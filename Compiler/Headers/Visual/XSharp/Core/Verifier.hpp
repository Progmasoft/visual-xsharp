// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
#pragma once

#include <string>
#include <vector>

#include "Visual/XSharp/Core/IR.hpp"

namespace Visual::XSharp::Core
{
    struct VerificationIssue final
    {
        std::string code;
        std::string message;
        SymbolId function{};
        SymbolId symbol{};
    };

    // Verify applies the same semantic boundary as Visual.XSharp.Core.Verifier in the
    // Haskell frontend. A decoded document is only structurally valid; no CorePrep or
    // backend stage may consume it until this verifier returns an empty issue list.
    [[nodiscard]] auto
    Verify(const Module &module) -> std::vector<VerificationIssue>;
} // namespace Visual::XSharp::Core
