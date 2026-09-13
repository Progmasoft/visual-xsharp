// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "Visual/XSharp/Analysis/ControlFlow.hpp"

namespace Visual::XSharp::Analysis
{
    // Dominance is the common structural contract used by target-independent
    // native optimizations. Keeping it independent from Xpp and Xmm prevents
    // either IR from quietly acquiring different loop or join semantics.
    struct DominanceBlockFacts final
    {
        ControlFlowBlockId block{};
        bool reachable{};
        std::optional<ControlFlowBlockId> immediateDominator;
        std::optional<ControlFlowBlockId> immediatePostDominator;
        std::vector<ControlFlowBlockId> dominators;
        std::vector<ControlFlowBlockId> postDominators;
        std::vector<ControlFlowBlockId> dominanceFrontier;
        std::vector<ControlFlowBlockId> postDominanceFrontier;
        std::vector<ControlFlowBlockId> loopHeaders;
        std::size_t loopDepth{};

        [[nodiscard]] auto
        operator==(const DominanceBlockFacts &) const -> bool = default;
    };

    struct NaturalLoop final
    {
        ControlFlowBlockId header{};
        ControlFlowBlockId latch{};
        std::vector<ControlFlowBlockId> members;
        std::vector<ControlFlowBlockId> exits;

        [[nodiscard]] auto
        operator==(const NaturalLoop &) const -> bool = default;
    };

    // An irreducible region is a cyclic SCC without one member that dominates
    // the complete region. Such a graph remains legal IR, but transformations
    // which assume a natural-loop header must explicitly decline it.
    struct IrreducibleRegion final
    {
        std::vector<ControlFlowBlockId> members;
        std::vector<ControlFlowBlockId> entries;

        [[nodiscard]] auto
        operator==(const IrreducibleRegion &) const -> bool = default;
    };

    struct DominanceResult final
    {
        ControlFlowResult controlFlow;
        std::vector<DominanceBlockFacts> facts;
        std::vector<NaturalLoop> naturalLoops;
        std::vector<IrreducibleRegion> irreducibleRegions;
        std::vector<ControlFlowBlockId> exits;
        bool hasPostDominance{};

        // Structural analysis can still describe malformed CFG input, but a
        // transforming pass must not consume partial facts as proof.
        [[nodiscard]] auto
        validForTransformation() const -> bool
        {
            return controlFlow.valid();
        }
    };

    [[nodiscard]] auto
    AnalyzeDominance(const ControlFlowGraph &graph) -> DominanceResult;

    [[nodiscard]] auto
    Dominates(
        const DominanceResult &result,
        ControlFlowBlockId dominator,
        ControlFlowBlockId block) -> bool;

    [[nodiscard]] auto
    StrictlyDominates(
        const DominanceResult &result,
        ControlFlowBlockId dominator,
        ControlFlowBlockId block) -> bool;

    [[nodiscard]] auto
    PostDominates(
        const DominanceResult &result,
        ControlFlowBlockId postDominator,
        ControlFlowBlockId block) -> bool;

    [[nodiscard]] auto
    StrictlyPostDominates(
        const DominanceResult &result,
        ControlFlowBlockId postDominator,
        ControlFlowBlockId block) -> bool;

    [[nodiscard]] auto
    FactsFor(
        const DominanceResult &result,
        ControlFlowBlockId block) -> const DominanceBlockFacts *;
} // namespace Visual::XSharp::Analysis
