// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

#include <algorithm>
#include <deque>
#include <ranges>
#include <unordered_map>
#include <unordered_set>

#include "Visual/XSharp/Analysis/OwnershipFlow.hpp"

namespace Visual::XSharp::Analysis::OwnershipFlow
{
    namespace
    {
        using BlockMap = std::unordered_map<BlockId, const Block *>;
        using PredecessorMap = std::unordered_map<BlockId, std::vector<BlockId>>;
        using StateMap = std::unordered_map<HandleId, StateMask>;
        using FactMap = std::unordered_map<BlockId, StateMap>;

        [[nodiscard]] auto
        LiveMask() noexcept -> StateMask
        {
            return static_cast<StateMask>(kStrong | kWeak | kUnowned);
        }

        [[nodiscard]] auto
        State(const StateMap &states, HandleId handle) noexcept -> StateMask
        {
            if (const auto found = states.find(handle); found != states.end())
                return found->second;
            return kAbsent;
        }

        [[nodiscard]] auto
        CatalogBlocks(const Function &function, std::vector<Issue> &issues) -> BlockMap
        {
            BlockMap blocks;
            blocks.reserve(function.blocks.size());
            for (const auto &block : function.blocks)
            {
                if (!blocks.emplace(block.id, &block).second)
                    issues.push_back(
                        { IssueKind::DuplicateBlock,
                          block.id,
                          0U,
                          false,
                          0U,
                          HandleKind::Strong,
                          kAbsent });
            }
            if (!blocks.contains(function.entry))
                issues.push_back(
                    { IssueKind::MissingEntry,
                      function.entry,
                      0U,
                      false,
                      0U,
                      HandleKind::Strong,
                      kAbsent });
            return blocks;
        }

        [[nodiscard]] auto
        BuildPredecessors(
            const Function &function,
            const BlockMap &blocks,
            std::vector<Issue> &issues) -> PredecessorMap
        {
            PredecessorMap predecessors;
            predecessors.reserve(blocks.size());
            for (const auto &[id, block] : blocks)
            {
                static_cast<void>(block);
                predecessors.emplace(id, std::vector<BlockId>{});
            }

            // Iterate the source vector to keep diagnostics deterministic even though
            // lookup and the fixed point use hash maps internally.
            for (const auto &block : function.blocks)
            {
                if (blocks.at(block.id) != &block)
                    continue;
                for (const auto successor : block.successors)
                {
                    const auto target = predecessors.find(successor);
                    if (target == predecessors.end())
                    {
                        issues.push_back(
                            { IssueKind::InvalidTarget,
                              block.id,
                              block.actions.size(),
                              true,
                              0U,
                              HandleKind::Strong,
                              kAbsent });
                        continue;
                    }
                    target->second.push_back(block.id);
                }
            }
            return predecessors;
        }

        [[nodiscard]] auto
        ReachableBlocks(const Function &function, const BlockMap &blocks)
            -> std::unordered_set<BlockId>
        {
            std::unordered_set<BlockId> reachable;
            if (!blocks.contains(function.entry))
                return reachable;

            std::vector<BlockId> pending{ function.entry };
            while (!pending.empty())
            {
                const auto blockId = pending.back();
                pending.pop_back();
                if (!reachable.insert(blockId).second)
                    continue;
                for (const auto successor : blocks.at(blockId)->successors)
                    if (blocks.contains(successor))
                        pending.push_back(successor);
            }
            return reachable;
        }

        [[nodiscard]] auto
        HandleUniverse(const Function &function) -> std::vector<HandleId>
        {
            std::vector<HandleId> handles;
            handles.reserve(function.initialHandles.size());
            for (const auto &initial : function.initialHandles)
                if (initial.handle != 0U)
                    handles.push_back(initial.handle);
            for (const auto &block : function.blocks)
                for (const auto &action : block.actions)
                    if (action.handle != 0U)
                        handles.push_back(action.handle);
            std::ranges::sort(handles);
            handles.erase(std::unique(handles.begin(), handles.end()), handles.end());
            return handles;
        }

        [[nodiscard]] auto
        EmptyState(const std::vector<HandleId> &handles, StateMask value = kAbsent) -> StateMap
        {
            StateMap states;
            states.reserve(handles.size());
            for (const auto handle : handles)
                states.emplace(handle, value);
            return states;
        }

        [[nodiscard]] auto
        InitialState(
            const Function &function,
            const std::vector<HandleId> &handles,
            std::vector<Issue> &issues) -> StateMap
        {
            auto states = EmptyState(handles);
            for (const auto &initial : function.initialHandles)
            {
                if (initial.handle == 0U)
                {
                    issues.push_back(
                        { IssueKind::InvalidInitialHandle,
                          function.entry,
                          0U,
                          false,
                          0U,
                          initial.kind,
                          kAbsent });
                    continue;
                }

                const auto expected = StateFor(initial.kind);
                auto &actual = states.at(initial.handle);
                if (actual != kAbsent && actual != expected)
                    issues.push_back(
                        { IssueKind::ConflictingInitialKind,
                          function.entry,
                          0U,
                          false,
                          initial.handle,
                          initial.kind,
                          actual });
                actual = static_cast<StateMask>((actual == kAbsent ? 0U : actual) | expected);
            }
            return states;
        }

        void
        Apply(const Action &action, StateMap &states)
        {
            if (action.handle == 0U)
                return;

            auto &actual = states[action.handle];
            switch (action.kind)
            {
                case ActionKind::Observe:
                    break;
                case ActionKind::Consume:
                {
                    const auto expected = StateFor(action.expected);
                    if ((actual & expected) != 0U)
                    {
                        actual = static_cast<StateMask>(actual & ~expected);
                        actual = static_cast<StateMask>(actual | kConsumed);
                    }
                    break;
                }
                case ActionKind::Define:
                    actual = StateFor(action.expected);
                    break;
                case ActionKind::Forget:
                    actual = kAbsent;
                    break;
            }
        }

        [[nodiscard]] auto
        Transfer(const Block &block, StateMap incoming) -> StateMap
        {
            for (const auto &action : block.actions)
                Apply(action, incoming);
            return incoming;
        }

        [[nodiscard]] auto
        JoinPredecessors(
            BlockId block,
            const Function &function,
            const PredecessorMap &predecessors,
            const std::unordered_set<BlockId> &reachable,
            const std::vector<HandleId> &handles,
            const StateMap &initial,
            const FactMap &outgoing) -> StateMap
        {
            if (block == function.entry)
                return initial;

            // Zero is the lattice bottom while the fixed point is forming. Once a
            // reachable predecessor contributes, each handle carries an explicit
            // Absent bit when that path has not produced an ownership token.
            auto joined = EmptyState(handles, 0U);
            bool hasPredecessor = false;
            if (const auto found = predecessors.find(block); found != predecessors.end())
            {
                for (const auto predecessor : found->second)
                {
                    if (!reachable.contains(predecessor))
                        continue;
                    const auto facts = outgoing.find(predecessor);
                    if (facts == outgoing.end())
                        continue;
                    hasPredecessor = true;
                    for (const auto handle : handles)
                        joined[handle] = static_cast<StateMask>(
                            joined[handle] | State(facts->second, handle));
                }
            }
            return hasPredecessor ? joined : EmptyState(handles);
        }

        void
        ComputeFixedPoint(
            const Function &function,
            const BlockMap &blocks,
            const PredecessorMap &predecessors,
            const std::unordered_set<BlockId> &reachable,
            const std::vector<HandleId> &handles,
            const StateMap &initial,
            FactMap &incoming,
            FactMap &outgoing)
        {
            for (const auto block : reachable)
            {
                incoming.emplace(block, EmptyState(handles, 0U));
                outgoing.emplace(block, EmptyState(handles, 0U));
            }

            bool changed = true;
            while (changed)
            {
                changed = false;
                for (const auto &sourceBlock : function.blocks)
                {
                    const auto blockId = sourceBlock.id;
                    if (!reachable.contains(blockId) || blocks.at(blockId) != &sourceBlock)
                        continue;
                    auto nextIncoming = JoinPredecessors(
                        blockId,
                        function,
                        predecessors,
                        reachable,
                        handles,
                        initial,
                        outgoing);
                    auto nextOutgoing = Transfer(sourceBlock, nextIncoming);
                    if (incoming.at(blockId) != nextIncoming || outgoing.at(blockId) != nextOutgoing)
                    {
                        incoming[blockId] = std::move(nextIncoming);
                        outgoing[blockId] = std::move(nextOutgoing);
                        changed = true;
                    }
                }
            }
        }

        [[nodiscard]] auto
        HasOtherLiveKind(StateMask actual, StateMask expected) noexcept -> bool
        {
            return (actual & LiveMask() & ~expected) != 0U;
        }

        void
        ValidateRequirement(
            const Action &action,
            BlockId block,
            StateMask actual,
            std::vector<Issue> &issues)
        {
            const auto expected = StateFor(action.expected);
            const auto hasExpected = (actual & expected) != 0U;
            const auto hasConsumed = Contains(actual, kConsumed);
            const auto hasAbsent = Contains(actual, kAbsent);
            const auto hasOtherKind = HasOtherLiveKind(actual, expected);

            if (hasExpected && (hasConsumed || hasAbsent || hasOtherKind))
            {
                // Some paths satisfy the requirement and others do not. Reporting a
                // join mismatch is more useful than arbitrarily calling this a use
                // after release or a kind error.
                issues.push_back(
                    { IssueKind::PathStateMismatch,
                      block,
                      action.instruction,
                      action.terminator,
                      action.handle,
                      action.expected,
                      actual });
            }
            else if (!hasExpected && hasConsumed)
                issues.push_back(
                    { IssueKind::UseAfterConsume,
                      block,
                      action.instruction,
                      action.terminator,
                      action.handle,
                      action.expected,
                      actual });
            else if (!hasExpected && hasOtherKind)
                issues.push_back(
                    { IssueKind::HandleKindMismatch,
                      block,
                      action.instruction,
                      action.terminator,
                      action.handle,
                      action.expected,
                      actual });
            // Absent-only inputs are left to definite-initialization. Keeping the
            // analyses orthogonal avoids two diagnostics for the same missing value.
        }

        void
        ValidateActions(
            const Function &function,
            const BlockMap &blocks,
            const std::unordered_set<BlockId> &reachable,
            const FactMap &incoming,
            std::vector<Issue> &issues)
        {
            for (const auto &block : function.blocks)
            {
                if (!reachable.contains(block.id) || blocks.at(block.id) != &block)
                    continue;
                auto state = incoming.at(block.id);
                for (const auto &action : block.actions)
                {
                    if (action.handle == 0U)
                    {
                        issues.push_back(
                            { IssueKind::InvalidActionHandle,
                              block.id,
                              action.instruction,
                              action.terminator,
                              0U,
                              action.expected,
                              kAbsent });
                        continue;
                    }
                    if (action.kind == ActionKind::Observe || action.kind == ActionKind::Consume)
                        ValidateRequirement(action, block.id, State(state, action.handle), issues);
                    Apply(action, state);
                }
            }
        }

        [[nodiscard]] auto
        SortedFacts(const StateMap &states) -> std::vector<HandleFact>
        {
            std::vector<HandleFact> facts;
            facts.reserve(states.size());
            for (const auto &[handle, state] : states)
                facts.push_back(
                    { handle,
                      state == 0U ? static_cast<StateMask>(kAbsent) : state });
            std::ranges::sort(facts, {}, &HandleFact::handle);
            return facts;
        }

        [[nodiscard]] auto
        BuildFacts(
            const Function &function,
            const BlockMap &blocks,
            const std::unordered_set<BlockId> &reachable,
            const std::vector<HandleId> &handles,
            const FactMap &incoming,
            const FactMap &outgoing) -> std::vector<BlockFacts>
        {
            std::vector<BlockFacts> facts;
            facts.reserve(blocks.size());
            for (const auto &block : function.blocks)
            {
                if (blocks.at(block.id) != &block)
                    continue;
                const auto isReachable = reachable.contains(block.id);
                facts.push_back(
                    { block.id,
                      isReachable,
                      isReachable ? SortedFacts(incoming.at(block.id))
                                  : SortedFacts(EmptyState(handles)),
                      isReachable ? SortedFacts(outgoing.at(block.id))
                                  : SortedFacts(EmptyState(handles)) });
            }
            std::ranges::sort(facts, {}, &BlockFacts::block);
            return facts;
        }
    } // namespace

    auto
    StateFor(HandleKind kind) noexcept -> StateMask
    {
        switch (kind)
        {
            case HandleKind::Strong:
                return kStrong;
            case HandleKind::Weak:
                return kWeak;
            case HandleKind::Unowned:
                return kUnowned;
        }
        return kAbsent;
    }

    auto
    Contains(StateMask states, StateBit state) noexcept -> bool
    {
        return (states & static_cast<StateMask>(state)) != 0U;
    }

    auto
    IsExactly(StateMask states, HandleKind kind) noexcept -> bool
    {
        return states == StateFor(kind);
    }

    auto
    Analyze(const Function &function) -> Result
    {
        Result result;
        const auto blocks = CatalogBlocks(function, result.issues);
        const auto predecessors = BuildPredecessors(function, blocks, result.issues);
        const auto reachable = ReachableBlocks(function, blocks);
        const auto handles = HandleUniverse(function);
        const auto initial = InitialState(function, handles, result.issues);

        FactMap incoming;
        FactMap outgoing;
        ComputeFixedPoint(
            function,
            blocks,
            predecessors,
            reachable,
            handles,
            initial,
            incoming,
            outgoing);
        ValidateActions(function, blocks, reachable, incoming, result.issues);
        result.facts = BuildFacts(
            function,
            blocks,
            reachable,
            handles,
            incoming,
            outgoing);
        return result;
    }
} // namespace Visual::XSharp::Analysis::OwnershipFlow
