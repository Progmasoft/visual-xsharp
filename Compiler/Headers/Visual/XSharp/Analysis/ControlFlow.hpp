// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1
#pragma once

#include <cstdint>
#include <vector>

namespace Visual::XSharp::Analysis
{
    using ControlFlowBlockId = std::uint32_t;

    struct ControlFlowBlock final
    {
        ControlFlowBlockId id{};
        std::vector<ControlFlowBlockId> successors;
    };

    struct ControlFlowGraph final
    {
        ControlFlowBlockId entry{};
        std::vector<ControlFlowBlock> blocks;
    };

    enum class ControlFlowIssueKind : std::uint8_t
    {
        DuplicateBlock,
        MissingEntry,
        MissingTarget
    };

    struct ControlFlowIssue final
    {
        ControlFlowIssueKind kind{ ControlFlowIssueKind::MissingEntry };
        ControlFlowBlockId block{};
        ControlFlowBlockId target{};

        [[nodiscard]] auto
        operator==(const ControlFlowIssue &) const -> bool = default;
    };

    struct ControlFlowBlockFacts final
    {
        ControlFlowBlockId block{};
        bool reachable{};
        std::vector<ControlFlowBlockId> predecessors;
        std::vector<ControlFlowBlockId> successors;

        [[nodiscard]] auto
        operator==(const ControlFlowBlockFacts &) const -> bool = default;
    };

    struct ControlFlowResult final
    {
        std::vector<ControlFlowIssue> issues;
        std::vector<ControlFlowBlockId> preorder;
        std::vector<ControlFlowBlockId> reversePostorder;
        std::vector<ControlFlowBlockFacts> facts;

        [[nodiscard]] auto
        valid() const -> bool
        {
            return issues.empty();
        }
    };

    // AnalyzeControlFlow treats block ids and explicit edges as semantic. The
    // input vector is presentation only: traversal always follows each block's
    // declared successor order, and observable fact tables are sorted by id.
    [[nodiscard]] auto
    AnalyzeControlFlow(const ControlFlowGraph &graph) -> ControlFlowResult;
} // namespace Visual::XSharp::Analysis
