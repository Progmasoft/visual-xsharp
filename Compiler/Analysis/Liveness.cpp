// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "Visual/XSharp/Analysis/Liveness.hpp"

namespace Visual::XSharp::Analysis::Liveness
{
    namespace
    {
        using StorageSet = std::unordered_set<StorageId>;
        using BlockMap = std::unordered_map<BlockId, const Block *>;
        using FactMap = std::unordered_map<BlockId, StorageSet>;
        using FlowFactMap = std::unordered_map<BlockId, const ControlFlowBlockFacts *>;

        [[nodiscard]] auto
        Sorted(const StorageSet &values) -> std::vector<StorageId>
        {
            std::vector<StorageId> result(values.begin(), values.end());
            std::ranges::sort(result);
            return result;
        }

        [[nodiscard]] auto
        ControlFlowFor(const Function &function) -> ControlFlowGraph
        {
            ControlFlowGraph graph;
            graph.entry = function.entry;
            graph.blocks.reserve(function.blocks.size());
            for (const auto &block : function.blocks)
                graph.blocks.push_back({ block.id, block.successors });
            return graph;
        }

        [[nodiscard]] auto
        Catalog(const Function &function) -> BlockMap
        {
            BlockMap blocks;
            blocks.reserve(function.blocks.size());
            for (const auto &block : function.blocks)
                blocks.emplace(block.id, &block);
            return blocks;
        }

        [[nodiscard]] auto
        Apply(const Access &access, StorageSet live) -> StorageSet
        {
            // A removable dead definition is semantically absent. In
            // particular, its operands do not keep earlier definitions alive.
            if (access.write && access.removable && !live.contains(*access.write))
                return live;

            if (access.write)
                live.erase(*access.write);
            live.insert(access.reads.begin(), access.reads.end());
            return live;
        }

        [[nodiscard]] auto
        Transfer(const Block &block, StorageSet live) -> StorageSet
        {
            for (auto access = block.accesses.rbegin(); access != block.accesses.rend(); ++access)
                live = Apply(*access, std::move(live));
            return live;
        }

        [[nodiscard]] auto
        ExitFacts(
            const ControlFlowBlockFacts &block,
            const FactMap &incoming) -> StorageSet
        {
            StorageSet live;
            for (const auto successor : block.successors)
                if (const auto found = incoming.find(successor); found != incoming.end())
                    live.insert(found->second.begin(), found->second.end());
            return live;
        }

        void
        ComputeFixedPoint(
            const ControlFlowResult &flow,
            const BlockMap &blocks,
            FactMap &incoming,
            FactMap &outgoing)
        {
            FlowFactMap flowFacts;
            flowFacts.reserve(flow.facts.size());
            for (const auto &facts : flow.facts)
                flowFacts.emplace(facts.block, &facts);
            for (const auto block : flow.reversePostorder)
            {
                incoming.emplace(block, StorageSet{});
                outgoing.emplace(block, StorageSet{});
            }

            bool changed = true;
            while (changed)
            {
                changed = false;
                // Reverse postorder reversed visits successors before their
                // predecessors and reaches the fixed point quickly for the
                // common acyclic case. Loops remain an ordinary monotone union.
                for (auto block = flow.reversePostorder.rbegin(); block != flow.reversePostorder.rend(); ++block)
                {
                    auto nextOutgoing = ExitFacts(*flowFacts.at(*block), incoming);
                    auto nextIncoming = Transfer(*blocks.at(*block), nextOutgoing);
                    if (incoming.at(*block) != nextIncoming || outgoing.at(*block) != nextOutgoing)
                    {
                        incoming.at(*block) = std::move(nextIncoming);
                        outgoing.at(*block) = std::move(nextOutgoing);
                        changed = true;
                    }
                }
            }
        }

        [[nodiscard]] auto
        BuildAccessFacts(const Block &block, StorageSet live) -> std::vector<AccessFacts>
        {
            std::vector<AccessFacts> reversed;
            reversed.reserve(block.accesses.size());
            for (auto access = block.accesses.rbegin(); access != block.accesses.rend(); ++access)
            {
                const auto liveAfter = Sorted(live);
                const auto retained = !access->write || !access->removable || live.contains(*access->write);
                if (retained)
                    live = Apply(*access, std::move(live));
                reversed.push_back({
                    access->instruction,
                    access->terminator,
                    retained,
                    Sorted(live),
                    liveAfter,
                });
            }
            return { reversed.rbegin(), reversed.rend() };
        }

        [[nodiscard]] auto
        BuildFacts(
            const Function &function,
            const ControlFlowResult &flow,
            const FactMap &incoming,
            const FactMap &outgoing) -> std::vector<BlockFacts>
        {
            const std::unordered_set<BlockId> reachable(flow.preorder.begin(), flow.preorder.end());
            std::vector<BlockFacts> facts;
            facts.reserve(function.blocks.size());
            std::unordered_set<BlockId> emitted;
            for (const auto &block : function.blocks)
            {
                // Duplicate blocks are already rejected by ControlFlow. Keep
                // one conservative fact record so malformed input does not
                // acquire contradictory elimination decisions.
                if (!emitted.insert(block.id).second)
                    continue;
                const auto isReachable = reachable.contains(block.id);
                if (!isReachable)
                {
                    std::vector<AccessFacts> accessFacts;
                    accessFacts.reserve(block.accesses.size());
                    for (const auto &access : block.accesses)
                        accessFacts.push_back({ access.instruction, access.terminator, true, {}, {} });
                    facts.push_back({ block.id, false, {}, {}, std::move(accessFacts) });
                    continue;
                }
                facts.push_back({
                    block.id,
                    true,
                    Sorted(incoming.at(block.id)),
                    Sorted(outgoing.at(block.id)),
                    BuildAccessFacts(block, outgoing.at(block.id)),
                });
            }
            std::ranges::sort(facts, {}, &BlockFacts::block);
            return facts;
        }
    } // namespace

    auto
    Analyze(const Function &function) -> Result
    {
        const auto flow = AnalyzeControlFlow(ControlFlowFor(function));
        Result result;
        result.issues = flow.issues;
        const auto blocks = Catalog(function);
        FactMap incoming;
        FactMap outgoing;
        ComputeFixedPoint(flow, blocks, incoming, outgoing);
        result.facts = BuildFacts(function, flow, incoming, outgoing);
        return result;
    }
} // namespace Visual::XSharp::Analysis::Liveness
