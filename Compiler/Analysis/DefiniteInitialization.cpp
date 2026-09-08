// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

#include <algorithm>
#include <deque>
#include <iterator>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "Visual/XSharp/Analysis/DefiniteInitialization.hpp"

namespace Visual::XSharp::Analysis
{
    namespace
    {
        using StorageSet = std::unordered_set<StorageId>;
        using BlockMap = std::unordered_map<BlockId, const Block *>;
        using PredecessorMap = std::unordered_map<BlockId, std::vector<BlockId>>;
        using FactMap = std::unordered_map<BlockId, StorageSet>;

        [[nodiscard]] auto
        Sorted(const StorageSet &values) -> std::vector<StorageId>
        {
            std::vector<StorageId> output(values.begin(), values.end());
            std::ranges::sort(output);
            return output;
        }

        [[nodiscard]] auto
        Intersect(const StorageSet &left, const StorageSet &right) -> StorageSet
        {
            const auto *smaller = &left;
            const auto *larger = &right;
            if (smaller->size() > larger->size())
                std::swap(smaller, larger);

            StorageSet result;
            result.reserve(smaller->size());
            for (const auto storage : *smaller)
                if (larger->contains(storage))
                    result.insert(storage);
            return result;
        }

        [[nodiscard]] auto
        DeclaredStorage(const Function &function, std::vector<Issue> &issues) -> StorageSet
        {
            StorageSet declarations;
            declarations.reserve(function.declarations.size());
            for (const auto storage : function.declarations)
                if (!declarations.insert(storage).second)
                    issues.push_back({ IssueKind::DuplicateDeclaration, function.entry, 0U, false, storage, 0U });
            return declarations;
        }

        [[nodiscard]] auto
        CatalogBlocks(const Function &function, std::vector<Issue> &issues) -> BlockMap
        {
            BlockMap blocks;
            blocks.reserve(function.blocks.size());
            for (const auto &block : function.blocks)
                if (!blocks.emplace(block.id, &block).second)
                    issues.push_back({ IssueKind::DuplicateBlock, block.id, 0U, false, 0U, 0U });
            if (!blocks.contains(function.entry))
                issues.push_back({ IssueKind::MissingEntry, function.entry, 0U, false, 0U, 0U });
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

            // Iterate source blocks in their supplied order for stable issue
            // locations. The fixed point itself is keyed by identity and is
            // therefore independent of this presentation order.
            for (const auto &block : function.blocks)
            {
                std::unordered_set<BlockId> uniqueTargets;
                for (const auto target : block.successors)
                {
                    if (!blocks.contains(target))
                    {
                        issues.push_back({ IssueKind::MissingTarget, block.id, block.accesses.size(), true, 0U, target });
                        continue;
                    }
                    if (uniqueTargets.insert(target).second)
                        predecessors[target].push_back(block.id);
                }
            }
            return predecessors;
        }

        [[nodiscard]] auto
        ReachableBlocks(const Function &function, const BlockMap &blocks) -> std::unordered_set<BlockId>
        {
            std::unordered_set<BlockId> reachable;
            if (!blocks.contains(function.entry))
                return reachable;

            std::deque<BlockId> pending{ function.entry };
            while (!pending.empty())
            {
                const auto current = pending.front();
                pending.pop_front();
                if (!reachable.insert(current).second)
                    continue;
                for (const auto successor : blocks.at(current)->successors)
                    if (blocks.contains(successor) && !reachable.contains(successor))
                        pending.push_back(successor);
            }
            return reachable;
        }

        [[nodiscard]] auto
        InitialStorage(
            const Function &function,
            const StorageSet &declarations,
            std::vector<Issue> &issues) -> StorageSet
        {
            StorageSet initialized;
            initialized.reserve(function.initiallyInitialized.size());
            for (const auto storage : function.initiallyInitialized)
            {
                if (!declarations.contains(storage))
                {
                    issues.push_back({ IssueKind::UnknownInitialStorage, function.entry, 0U, false, storage, 0U });
                    continue;
                }
                initialized.insert(storage);
            }
            return initialized;
        }

        [[nodiscard]] auto
        TransferWrites(const Block &block, StorageSet initialized, const StorageSet &declarations) -> StorageSet
        {
            for (const auto &access : block.accesses)
                if (access.write && declarations.contains(*access.write))
                    initialized.insert(*access.write);
            return initialized;
        }

        [[nodiscard]] auto
        IncomingFacts(
            BlockId block,
            const Function &function,
            const PredecessorMap &predecessors,
            const std::unordered_set<BlockId> &reachable,
            const StorageSet &initial,
            const FactMap &outgoing) -> StorageSet
        {
            // Entry represents the externally callable edge. A backedge cannot
            // make a parameter or local initialized on the function's first
            // invocation, so its incoming facts remain exactly the seed set.
            if (block == function.entry)
                return initial;

            const auto found = predecessors.find(block);
            if (found == predecessors.end())
                return {};

            bool first = true;
            StorageSet incoming;
            for (const auto predecessor : found->second)
            {
                if (!reachable.contains(predecessor))
                    continue;
                if (const auto facts = outgoing.find(predecessor); facts != outgoing.end())
                {
                    if (first)
                    {
                        incoming = facts->second;
                        first = false;
                    }
                    else
                        incoming = Intersect(incoming, facts->second);
                }
            }
            return first ? StorageSet{} : incoming;
        }

        void
        ComputeFixedPoint(
            const Function &function,
            const BlockMap &blocks,
            const PredecessorMap &predecessors,
            const std::unordered_set<BlockId> &reachable,
            const StorageSet &declarations,
            const StorageSet &initial,
            FactMap &incoming,
            FactMap &outgoing)
        {
            // Must analysis starts non-entry blocks at top. Repeated
            // predecessor intersection can only remove facts; local writes add
            // the same facts each iteration, so convergence is finite.
            for (const auto block : reachable)
            {
                incoming[block] = block == function.entry ? initial : declarations;
                outgoing[block] = TransferWrites(*blocks.at(block), incoming[block], declarations);
            }

            bool changed = true;
            while (changed)
            {
                changed = false;
                for (const auto &[blockId, blockPointer] : blocks)
                {
                    if (!reachable.contains(blockId))
                        continue;
                    const auto &block = *blockPointer;
                    auto nextIncoming = IncomingFacts(
                        blockId,
                        function,
                        predecessors,
                        reachable,
                        initial,
                        outgoing);
                    auto nextOutgoing = TransferWrites(block, nextIncoming, declarations);
                    if (incoming[blockId] != nextIncoming || outgoing[blockId] != nextOutgoing)
                    {
                        incoming[blockId] = std::move(nextIncoming);
                        outgoing[blockId] = std::move(nextOutgoing);
                        changed = true;
                    }
                }
            }
        }

        void
        ValidateAccesses(
            const Function &function,
            const std::unordered_set<BlockId> &reachable,
            const StorageSet &declarations,
            const FactMap &incoming,
            std::vector<Issue> &issues)
        {
            for (const auto &block : function.blocks)
            {
                if (!reachable.contains(block.id))
                    continue;
                auto initialized = incoming.at(block.id);
                for (const auto &access : block.accesses)
                {
                    for (const auto storage : access.reads)
                    {
                        if (!declarations.contains(storage))
                            issues.push_back(
                                { IssueKind::UnknownReadStorage,
                                  block.id,
                                  access.instruction,
                                  access.terminator,
                                  storage,
                                  0U });
                        else if (!initialized.contains(storage))
                            issues.push_back(
                                { IssueKind::ReadBeforeInitialization,
                                  block.id,
                                  access.instruction,
                                  access.terminator,
                                  storage,
                                  0U });
                    }
                    if (access.write)
                    {
                        if (!declarations.contains(*access.write))
                            issues.push_back(
                                { IssueKind::UnknownWriteStorage,
                                  block.id,
                                  access.instruction,
                                  access.terminator,
                                  *access.write,
                                  0U });
                        else
                            initialized.insert(*access.write);
                    }
                }
            }
        }

        [[nodiscard]] auto
        BuildFacts(
            const BlockMap &blocks,
            const std::unordered_set<BlockId> &reachable,
            const FactMap &incoming,
            const FactMap &outgoing) -> std::vector<BlockFacts>
        {
            std::vector<BlockFacts> facts;
            facts.reserve(blocks.size());
            for (const auto &[blockId, block] : blocks)
            {
                static_cast<void>(block);
                const auto isReachable = reachable.contains(blockId);
                facts.push_back(
                    { blockId,
                      isReachable,
                      isReachable ? Sorted(incoming.at(blockId)) : std::vector<StorageId>{},
                      isReachable ? Sorted(outgoing.at(blockId)) : std::vector<StorageId>{} });
            }
            std::ranges::sort(facts, {}, &BlockFacts::block);
            return facts;
        }
    } // namespace

    auto
    Analyze(const Function &function) -> Result
    {
        Result result;
        const auto declarations = DeclaredStorage(function, result.issues);
        const auto blocks = CatalogBlocks(function, result.issues);
        const auto predecessors = BuildPredecessors(function, blocks, result.issues);
        const auto reachable = ReachableBlocks(function, blocks);
        const auto initial = InitialStorage(function, declarations, result.issues);

        FactMap incoming;
        FactMap outgoing;
        ComputeFixedPoint(function, blocks, predecessors, reachable, declarations, initial, incoming, outgoing);
        ValidateAccesses(function, reachable, declarations, incoming, result.issues);
        result.facts = BuildFacts(blocks, reachable, incoming, outgoing);
        return result;
    }
} // namespace Visual::XSharp::Analysis
