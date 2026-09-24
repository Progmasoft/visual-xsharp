// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

#include "Visual/XSharp/Analysis/ControlFlow.hpp"

namespace Visual::XSharp::Analysis
{
    // A forward analysis propagates changed output facts to successors. A
    // backward analysis propagates changed input facts to predecessors. Keeping
    // direction in the scheduler avoids every dataflow pass rebuilding the same
    // edge maps and accidentally choosing a presentation-order iteration.
    enum class WorklistDirection : std::uint8_t
    {
        Forward,
        Backward
    };

    struct WorklistStatistics final
    {
        std::size_t blockEvaluations{};
        std::size_t changeNotifications{};
        std::size_t scheduledBlocks{};
        std::size_t peakPendingBlocks{};

        [[nodiscard]] auto
        operator==(const WorklistStatistics &) const -> bool = default;
    };

    class DataflowWorklist final
    {
    public:
        DataflowWorklist(const ControlFlowResult &controlFlow,
                         WorklistDirection direction);

        // Next removes one pending block. Every reachable block is initially
        // scheduled exactly once in a direction-friendly order: reverse
        // postorder for forward problems and postorder for backward problems.
        [[nodiscard]] auto
        Next() -> std::optional<ControlFlowBlockId>;

        // NotifyChanged schedules the blocks whose input may be affected by a
        // changed transfer result. Duplicate pending identities are coalesced.
        void
        NotifyChanged(ControlFlowBlockId block);

        // Explicit scheduling is useful for analyses with a non-standard seed.
        // Unknown and unreachable block identities are ignored conservatively.
        void
        Schedule(ControlFlowBlockId block);

        [[nodiscard]] auto
        Empty() const noexcept -> bool;

        [[nodiscard]] auto
        Statistics() const noexcept -> const WorklistStatistics &;

    private:
        struct Implementation;
        std::unique_ptr<Implementation> implementation_;

    public:
        DataflowWorklist(const DataflowWorklist &) = delete;
        auto
        operator=(const DataflowWorklist &) -> DataflowWorklist & = delete;

        DataflowWorklist(DataflowWorklist &&other) noexcept;
        auto
        operator=(DataflowWorklist &&other) noexcept -> DataflowWorklist &;

        ~DataflowWorklist();
    };
} // namespace Visual::XSharp::Analysis
