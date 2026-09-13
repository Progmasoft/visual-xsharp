// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1
#pragma once

#include <cstdint>
#include <vector>

#include "Visual/XSharp/Analysis/Dominance.hpp"

namespace Visual::XSharp::Analysis
{
    struct ControlDependenceEdge final
    {
        ControlFlowBlockId controller{};
        ControlFlowBlockId successor{};
        ControlFlowBlockId dependent{};

        [[nodiscard]] auto
        operator==(const ControlDependenceEdge &) const -> bool = default;
    };

    struct ControlDependenceBlockFacts final
    {
        ControlFlowBlockId block{};
        std::vector<ControlFlowBlockId> controllers;
        std::vector<ControlFlowBlockId> dependents;

        [[nodiscard]] auto
        operator==(const ControlDependenceBlockFacts &) const -> bool = default;
    };

    struct ControlDependenceResult final
    {
        DominanceResult structure;
        std::vector<ControlDependenceEdge> edges;
        std::vector<ControlDependenceBlockFacts> facts;
        bool available{};

        [[nodiscard]] auto
        validForTransformation() const -> bool
        {
            return available && structure.validForTransformation();
        }
    };

    // AnalyzeControlDependence records the controlling CFG edge as well as the
    // controller block. Edge identity matters when true and false arms later
    // receive different predicates or profile weights.
    [[nodiscard]] auto
    AnalyzeControlDependence(const ControlFlowGraph &graph) -> ControlDependenceResult;

    [[nodiscard]] auto
    ControlDependenceFactsFor(
        const ControlDependenceResult &result,
        ControlFlowBlockId block) -> const ControlDependenceBlockFacts *;
} // namespace Visual::XSharp::Analysis
