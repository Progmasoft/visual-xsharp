// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1
#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "Visual/XSharp/Xpp/IR.hpp"

namespace Visual::XSharp::Xpp
{
    /// One Xpp structural or symbolic validation finding.
    struct VerificationIssue final
    {
        /// Stable machine-readable issue identifier.
        std::string code;
        /// Human-readable explanation of the invalid construct.
        std::string message;
        /// Function identity containing the problem.
        ::visual_xsharp::xpp::SymbolId function{};
        /// Block identity containing the problem, when available.
        ::visual_xsharp::xpp::BlockId block{};
        /// Instruction ordinal for instruction-level issues.
        std::size_t instruction{};
    };

    /// Validate Xpp before storage assignment and Xmm lowering.
    /// @param module Optimizer output or other candidate Xpp program.
    /// @return Ordered verifier issues; empty means the Xpp module is valid.
    [[nodiscard]] auto
    Verify(const ::visual_xsharp::xpp::Module &module)
        -> std::vector<VerificationIssue>;
} // namespace Visual::XSharp::Xpp
