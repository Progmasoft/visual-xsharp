// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace Visual::XSharp::Analysis
{
    using StorageId = std::uint64_t;
    using BlockId = std::uint32_t;

    // An access point represents one instruction or the block terminator. Reads
    // happen before the optional write, matching the execution order of Xpp and
    // Xmm instructions. The original instruction index is retained so clients
    // can report a stage-specific diagnostic without reconstructing locations.
    struct AccessPoint final
    {
        std::size_t instruction{};
        bool terminator{};
        std::vector<StorageId> reads;
        std::optional<StorageId> write;
    };

    struct Block final
    {
        BlockId id{};
        std::vector<BlockId> successors;
        std::vector<AccessPoint> accesses;
    };

    struct Function final
    {
        BlockId entry{};
        std::vector<StorageId> declarations;
        std::vector<StorageId> initiallyInitialized;
        std::vector<Block> blocks;
    };

    enum class IssueKind : std::uint8_t
    {
        DuplicateBlock,
        MissingEntry,
        MissingTarget,
        DuplicateDeclaration,
        UnknownInitialStorage,
        UnknownReadStorage,
        UnknownWriteStorage,
        ReadBeforeInitialization
    };

    struct Issue final
    {
        IssueKind kind{ IssueKind::MissingEntry };
        BlockId block{};
        std::size_t instruction{};
        bool terminator{};
        StorageId storage{};
        BlockId target{};

        [[nodiscard]] auto
        operator==(const Issue &) const -> bool = default;
    };

    // Facts are intentionally observable. Stage verifier tests can assert the
    // actual fixed-point boundary instead of inferring it from one diagnostic,
    // and later ownership analyses can reuse the same control-flow contract.
    struct BlockFacts final
    {
        BlockId block{};
        bool reachable{};
        std::vector<StorageId> initializedOnEntry;
        std::vector<StorageId> initializedOnExit;

        [[nodiscard]] auto
        operator==(const BlockFacts &) const -> bool = default;
    };

    struct Result final
    {
        std::vector<Issue> issues;
        std::vector<BlockFacts> facts;

        [[nodiscard]] auto
        valid() const -> bool
        {
            return issues.empty();
        }
    };

    // Analyze computes a forward must-initialization fixed point. A storage is
    // initialized on block entry only when every reachable predecessor has
    // initialized it. Block vector order is never used as an execution order.
    [[nodiscard]] auto
    Analyze(const Function &function) -> Result;
} // namespace Visual::XSharp::Analysis
