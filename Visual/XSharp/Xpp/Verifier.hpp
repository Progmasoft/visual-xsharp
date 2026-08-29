// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
#pragma once

#include "Visual/XSharp/Xpp/IR.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace Visual::XSharp::Xpp
{
struct VerificationIssue final
{
    std::string code;
    std::string message;
    ::visual_xsharp::xpp::SymbolId function{};
    ::visual_xsharp::xpp::BlockId block{};
    std::size_t instruction{};
};

// Verify owns the Xpp boundary: optimizer output must remain structurally and
// symbolically valid before Xmm assigns storage or virtual registers.
[[nodiscard]] auto Verify(const ::visual_xsharp::xpp::Module &module) -> std::vector<VerificationIssue>;
} // namespace Visual::XSharp::Xpp
