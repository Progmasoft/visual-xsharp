// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "Visual/XSharp/Core/CorePrep.hpp"
#include <string>
#include <vector>

namespace visual_xsharp::core
{
struct VerificationIssue final
{
    std::string code;
    std::string message;
    SymbolId function{};
    BlockId block{};
};

[[nodiscard]] auto verify(const CorePrepModule &module) -> std::vector<VerificationIssue>;
} // namespace visual_xsharp::core
