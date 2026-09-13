// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "Visual/XSharp/Analysis/ControlFlow.hpp"

namespace Visual::XSharp::Analysis::Liveness
{
    using BlockId = ControlFlowBlockId;
    using StorageId = std::uint64_t;

    // Access is deliberately stage-neutral. Xpp symbolic storage and Xmm
    // virtual registers both fit StorageId, while the adapters retain their
    // own instruction/opcode policy for deciding whether a dead write is safe
    // to erase.
    struct Access final
    {
        std::size_t instruction{};
        bool terminator{};
        std::vector<StorageId> reads;
        std::optional<StorageId> write;
        bool removable{};

        [[nodiscard]] auto
        operator==(const Access &) const -> bool = default;
    };

    struct Block final
    {
        BlockId id{};
        std::vector<BlockId> successors;
        std::vector<Access> accesses;

        [[nodiscard]] auto
        operator==(const Block &) const -> bool = default;
    };

    struct Function final
    {
        BlockId entry{};
        std::vector<Block> blocks;
    };

    struct AccessFacts final
    {
        std::size_t instruction{};
        bool terminator{};
        bool retained{ true };
        std::vector<StorageId> liveBefore;
        std::vector<StorageId> liveAfter;

        [[nodiscard]] auto
        operator==(const AccessFacts &) const -> bool = default;
    };

    struct BlockFacts final
    {
        BlockId block{};
        bool reachable{};
        std::vector<StorageId> liveOnEntry;
        std::vector<StorageId> liveOnExit;
        std::vector<AccessFacts> accesses;

        [[nodiscard]] auto
        operator==(const BlockFacts &) const -> bool = default;
    };

    struct Result final
    {
        std::vector<ControlFlowIssue> issues;
        std::vector<BlockFacts> facts;

        [[nodiscard]] auto
        valid() const -> bool
        {
            return issues.empty();
        }
    };

    // Analyze computes the least backward liveness fixed point. A removable
    // write whose destination is not live is excluded together with its reads;
    // this makes a whole dead producer chain disappear in one analysis instead
    // of requiring an instruction-count number of optimizer iterations.
    [[nodiscard]] auto
    Analyze(const Function &function) -> Result;
} // namespace Visual::XSharp::Analysis::Liveness
