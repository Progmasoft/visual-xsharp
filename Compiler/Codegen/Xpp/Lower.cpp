// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

#include <algorithm>
#include <cstdlib>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "Visual/XSharp/Analysis/ControlFlow.hpp"
#include "Visual/XSharp/Xpp/IR.hpp"

namespace visual_xsharp::xpp
{
    namespace
    {
        namespace ControlFlow = ::Visual::XSharp::Analysis;

        auto
        LowerOperand(const core::Atom &atom) -> Operand
        {
            // CorePrep has already resolved names, so the numeric symbol is authoritative here.
            // Spelling remains on declarations for diagnostics and eventual external mangling.
            return Operand{ atom.kind == core::Atom::Kind::Variable ? Operand::Kind::Symbol : Operand::Kind::Literal, atom.type, atom.symbol.id, atom.literal };
        }

        auto
        LowerOperation(core::Operation operation) -> Opcode
        {
            switch (operation)
            {
                case core::Operation::Copy:
                    return Opcode::Copy;
                case core::Operation::Call:
                    return Opcode::Call;
                case core::Operation::Add:
                    return Opcode::Add;
                case core::Operation::Subtract:
                    return Opcode::Subtract;
                case core::Operation::Multiply:
                    return Opcode::Multiply;
                case core::Operation::Divide:
                    return Opcode::Divide;
                case core::Operation::FloorDivide:
                    return Opcode::FloorDivide;
                case core::Operation::Remainder:
                    return Opcode::Remainder;
                case core::Operation::LessThan:
                    return Opcode::CompareLess;
                case core::Operation::LessEqual:
                    return Opcode::CompareLessEqual;
                case core::Operation::GreaterThan:
                    return Opcode::CompareGreater;
                case core::Operation::GreaterEqual:
                    return Opcode::CompareGreaterEqual;
                case core::Operation::Equal:
                    return Opcode::CompareEqual;
                case core::Operation::NotEqual:
                    return Opcode::CompareNotEqual;
                case core::Operation::LogicalAnd:
                    return Opcode::LogicalAnd;
                case core::Operation::LogicalOr:
                    return Opcode::LogicalOr;
                case core::Operation::Negate:
                    return Opcode::Negate;
                case core::Operation::LogicalNot:
                    return Opcode::LogicalNot;
                case core::Operation::MakeClosure:
                    return Opcode::MakeClosure;
            }
            // An unknown CorePrep operation indicates an adapter/version bug;
            // translating it to Copy would silently change program semantics.
            std::abort();
        }

        auto
        LowerInstruction(const core::Instruction &instruction) -> Instruction
        {
            Instruction lowered{};
            // Preserve Bind/Assign/Discard as an explicit effect. Collapsing them into opcode
            // alone would lose the difference between defining storage and mutating it.
            lowered.effect = instruction.kind == core::Instruction::Kind::Bind     ? Instruction::Effect::Define
                             : instruction.kind == core::Instruction::Kind::Assign ? Instruction::Effect::Store
                                                                                   : Instruction::Effect::Discard;
            lowered.opcode = LowerOperation(instruction.operation);
            lowered.destination = instruction.destination.id;
            lowered.result_type = instruction.type;
            lowered.operands.reserve(instruction.operands.size());
            for (const auto &operand : instruction.operands)
                lowered.operands.push_back(LowerOperand(operand));
            if (instruction.operation == core::Operation::MakeClosure)
            {
                lowered.closure_function = instruction.closure_function.id;
                lowered.operands.reserve(instruction.captures.size());
                lowered.capture_modes.reserve(instruction.captures.size());
                for (const auto &capture : instruction.captures)
                {
                    lowered.operands.push_back(LowerOperand(capture.value));
                    lowered.capture_modes.push_back(capture.mode);
                }
            }
            return lowered;
        }

        auto
        LowerTerminator(const core::Terminator &terminator) -> Terminator
        {
            Terminator lowered{};
            switch (terminator.kind)
            {
                case core::Terminator::Kind::Return:
                    lowered.kind = Terminator::Kind::Return;
                    break;
                case core::Terminator::Kind::Branch:
                    lowered.kind = Terminator::Kind::Branch;
                    break;
                case core::Terminator::Kind::Jump:
                    lowered.kind = Terminator::Kind::Jump;
                    break;
                case core::Terminator::Kind::Unreachable:
                    lowered.kind = Terminator::Kind::Unreachable;
                    break;
            }
            lowered.value = LowerOperand(terminator.value);
            lowered.true_target = terminator.true_target;
            lowered.false_target = terminator.false_target;
            return lowered;
        }

        [[nodiscard]] auto
        Successors(const Terminator &terminator) -> std::vector<ControlFlow::ControlFlowBlockId>
        {
            switch (terminator.kind)
            {
                case Terminator::Kind::Branch:
                    return { terminator.true_target, terminator.false_target };
                case Terminator::Kind::Jump:
                    return { terminator.true_target };
                case Terminator::Kind::Return:
                case Terminator::Kind::Unreachable:
                    return {};
            }
            return {};
        }

        [[nodiscard]] auto
        Analyze(const Function &function) -> ControlFlow::ControlFlowResult
        {
            ControlFlow::ControlFlowGraph graph;
            graph.entry = function.entry;
            graph.blocks.reserve(function.blocks.size());
            for (const auto &block : function.blocks)
                graph.blocks.push_back({ block.id, Successors(block.terminator) });
            return ControlFlow::AnalyzeControlFlow(graph);
        }

        [[nodiscard]] auto
        BlockCatalog(Function &function) -> std::unordered_map<BlockId, Block *>
        {
            std::unordered_map<BlockId, Block *> blocks;
            blocks.reserve(function.blocks.size());
            for (auto &block : function.blocks)
                blocks.emplace(block.id, &block);
            return blocks;
        }

        [[nodiscard]] auto
        ResolveTrampoline(
            BlockId target,
            const std::unordered_map<BlockId, Block *> &blocks) -> BlockId
        {
            std::unordered_set<BlockId> visited;
            while (visited.insert(target).second)
            {
                const auto found = blocks.find(target);
                if (found == blocks.end())
                    break;
                const auto &block = *found->second;
                if (!block.instructions.empty() || block.terminator.kind != Terminator::Kind::Jump)
                    break;
                const auto next = block.terminator.true_target;
                if (next == target)
                    break;
                target = next;
            }
            return target;
        }

        void
        ThreadTrampolines(Function &function)
        {
            const auto blocks = BlockCatalog(function);
            for (auto &block : function.blocks)
            {
                auto &terminator = block.terminator;
                if (terminator.kind == Terminator::Kind::Jump)
                    terminator.true_target = ResolveTrampoline(terminator.true_target, blocks);
                else if (terminator.kind == Terminator::Kind::Branch)
                {
                    terminator.true_target = ResolveTrampoline(terminator.true_target, blocks);
                    terminator.false_target = ResolveTrampoline(terminator.false_target, blocks);
                    if (terminator.true_target == terminator.false_target)
                    {
                        // Once both edges agree the condition has no control-flow
                        // effect. Its producer remains in place; only the terminator
                        // stops pretending that two executions are possible.
                        terminator.kind = Terminator::Kind::Jump;
                        terminator.false_target = 0U;
                    }
                }
            }
        }

        void
        RetainReachableReversePostorder(Function &function)
        {
            const auto flow = Analyze(function);
            const std::unordered_set<BlockId> reachable(
                flow.reversePostorder.begin(),
                flow.reversePostorder.end());
            std::erase_if(function.blocks, [&reachable](const Block &block) {
                return !reachable.contains(block.id);
            });

            std::unordered_map<BlockId, std::size_t> order;
            order.reserve(flow.reversePostorder.size());
            for (std::size_t index = 0U; index < flow.reversePostorder.size(); ++index)
                order.emplace(flow.reversePostorder[index], index);
            std::ranges::sort(function.blocks, [&order](const Block &left, const Block &right) {
                return order.at(left.id) < order.at(right.id);
            });
        }

        auto
        IsSelfCopy(const Instruction &instruction) -> bool
        {
            return instruction.opcode == Opcode::Copy && instruction.effect != Instruction::Effect::Discard && instruction.operands.size() == 1U && instruction.operands.front().kind == Operand::Kind::Symbol && instruction.operands.front().symbol == instruction.destination;
        }
    } // namespace

    auto
    lower(const core::CorePrepModule &module) -> Module
    {
        Module lowered{ module.name, {} };
        lowered.functions.reserve(module.functions.size());
        for (const auto &function : module.functions)
        {
            Function loweredFunction{ function.symbol, function.parameters, function.return_type, function.entry, {} };
            loweredFunction.blocks.reserve(function.blocks.size());
            for (const auto &block : function.blocks)
            {
                Block loweredBlock{ block.id, {}, LowerTerminator(block.terminator) };
                loweredBlock.instructions.reserve(block.instructions.size());
                for (const auto &instruction : block.instructions)
                    loweredBlock.instructions.push_back(LowerInstruction(instruction));
                loweredFunction.blocks.push_back(std::move(loweredBlock));
            }
            lowered.functions.push_back(std::move(loweredFunction));
        }
        return lowered;
    }

    auto
    optimize(Module module) -> Module
    {
        for (auto &function : module.functions)
        {
            for (auto &block : function.blocks)
                std::erase_if(block.instructions, IsSelfCopy);
            ThreadTrampolines(function);
            RetainReachableReversePostorder(function);
        }
        return module;
    }
} // namespace visual_xsharp::xpp
