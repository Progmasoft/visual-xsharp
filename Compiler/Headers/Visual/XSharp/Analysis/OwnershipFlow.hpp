// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Visual::XSharp::Analysis::OwnershipFlow
{
    using BlockId = std::uint32_t;
    // Xpp symbols are 64-bit while Xmm registers are 32-bit. The common analysis
    // uses the wider identity so adapters never truncate a valid source symbol.
    using HandleId = std::uint64_t;

    // Xpp and Xmm intentionally erase source-level weak/unowned syntax from the
    // value type. HandleKind restores the runtime representation distinction for
    // dataflow without leaking AARC details into the general Type model.
    enum class HandleKind : std::uint8_t
    {
        Strong,
        Weak,
        Unowned
    };

    enum class ActionKind : std::uint8_t
    {
        // Observe requires a live handle but leaves its ownership token intact.
        Observe,

        // Consume models a release. Every reachable execution must arrive with
        // exactly one live token of the requested kind.
        Consume,

        // Define replaces the abstract state for the destination. It is used by
        // ordinary reference-producing operations as well as ownership conversions.
        Define,

        // Forget removes a destination from ownership tracking when a stage reuses
        // storage for a value that does not participate in AARC.
        Forget
    };

    struct Action final
    {
        ActionKind kind{ ActionKind::Observe };
        HandleId handle{};
        HandleKind expected{ HandleKind::Strong };
        std::size_t instruction{};
        bool terminator{};

        [[nodiscard]] auto
        operator==(const Action &) const -> bool = default;
    };

    struct Block final
    {
        BlockId id{};
        std::vector<BlockId> successors;
        std::vector<Action> actions;

        [[nodiscard]] auto
        operator==(const Block &) const -> bool = default;
    };

    struct InitialHandle final
    {
        HandleId handle{};
        HandleKind kind{ HandleKind::Strong };

        [[nodiscard]] auto
        operator==(const InitialHandle &) const -> bool = default;
    };

    struct Function final
    {
        BlockId entry{};
        std::vector<InitialHandle> initialHandles;
        std::vector<Block> blocks;

        [[nodiscard]] auto
        operator==(const Function &) const -> bool = default;
    };

    // A state mask is exposed in facts because a join may deliberately retain
    // several possibilities. This makes diagnostics inspectable and prevents a
    // verifier from silently choosing one predecessor's ownership state.
    enum StateBit : std::uint8_t
    {
        kAbsent = 1U << 0U,
        kStrong = 1U << 1U,
        kWeak = 1U << 2U,
        kUnowned = 1U << 3U,
        kConsumed = 1U << 4U
    };

    using StateMask = std::uint8_t;

    struct HandleFact final
    {
        HandleId handle{};
        StateMask states{ kAbsent };

        [[nodiscard]] auto
        operator==(const HandleFact &) const -> bool = default;
    };

    struct BlockFacts final
    {
        BlockId block{};
        bool reachable{};
        std::vector<HandleFact> incoming;
        std::vector<HandleFact> outgoing;

        [[nodiscard]] auto
        operator==(const BlockFacts &) const -> bool = default;
    };

    enum class IssueKind : std::uint8_t
    {
        DuplicateBlock,
        MissingEntry,
        InvalidTarget,
        InvalidInitialHandle,
        ConflictingInitialKind,
        InvalidActionHandle,
        UseAfterConsume,
        HandleKindMismatch,
        PathStateMismatch
    };

    struct Issue final
    {
        IssueKind kind{ IssueKind::InvalidActionHandle };
        BlockId block{};
        std::size_t instruction{};
        bool terminator{};
        HandleId handle{};
        HandleKind expected{ HandleKind::Strong };
        StateMask actual{ kAbsent };

        [[nodiscard]] auto
        operator==(const Issue &) const -> bool = default;
    };

    struct Result final
    {
        std::vector<BlockFacts> facts;
        std::vector<Issue> issues;

        [[nodiscard]] auto
        operator==(const Result &) const -> bool = default;
    };

    [[nodiscard]] auto
    StateFor(HandleKind kind) noexcept -> StateMask;

    [[nodiscard]] auto
    Contains(StateMask states, StateBit state) noexcept -> bool;

    [[nodiscard]] auto
    IsExactly(StateMask states, HandleKind kind) noexcept -> bool;

    // Analyze performs a forward may-state fixed point. Unlike definite
    // initialization, ownership joins use union: every state that can arrive is
    // relevant because one consumed predecessor is enough to make a later use unsafe.
    [[nodiscard]] auto
    Analyze(const Function &function) -> Result;
} // namespace Visual::XSharp::Analysis::OwnershipFlow
