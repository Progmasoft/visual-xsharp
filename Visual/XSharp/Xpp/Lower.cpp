// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

#include "Visual/XSharp/Xpp/IR.hpp"

#include <algorithm>
#include <unordered_set>
#include <utility>

namespace visual_xsharp::xpp
{
namespace
{
auto LowerOperand(const core::Atom &atom) -> Operand
{
    // CorePrep has already resolved names, so the numeric symbol is authoritative here.
    // Spelling remains on declarations for diagnostics and eventual external mangling.
    return Operand{atom.kind == core::Atom::Kind::Variable ? Operand::Kind::Symbol : Operand::Kind::Literal, atom.type,
                   atom.symbol.id, atom.literal};
}

auto LowerOperation(core::Operation operation) -> Opcode
{
    switch(operation)
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
    }
    return Opcode::Copy;
}

auto LowerInstruction(const core::Instruction &instruction) -> Instruction
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
    for(const auto &operand : instruction.operands)
        lowered.operands.push_back(LowerOperand(operand));
    return lowered;
}

auto LowerTerminator(const core::Terminator &terminator) -> Terminator
{
    Terminator lowered{};
    lowered.kind = static_cast<Terminator::Kind>(terminator.kind);
    lowered.value = LowerOperand(terminator.value);
    lowered.true_target = terminator.true_target;
    lowered.false_target = terminator.false_target;
    return lowered;
}

auto ReachableBlocks(const Function &function) -> std::unordered_set<BlockId>
{
    // Reachability is structural and deliberately avoids speculating about constant
    // conditions. Xpp optimization must not become a hidden semantic evaluator.
    std::unordered_set<BlockId> reachable;
    std::vector<BlockId> pending{function.entry};
    while(!pending.empty())
    {
        const auto id = pending.back();
        pending.pop_back();
        if(!reachable.insert(id).second)
            continue;
        const auto found = std::ranges::find(function.blocks, id, &Block::id);
        if(found == function.blocks.end())
            continue;
        if(found->terminator.kind == Terminator::Kind::Branch)
        {
            pending.push_back(found->terminator.true_target);
            pending.push_back(found->terminator.false_target);
        }
        else if(found->terminator.kind == Terminator::Kind::Jump)
            pending.push_back(found->terminator.true_target);
    }
    return reachable;
}

auto IsSelfCopy(const Instruction &instruction) -> bool
{
    return instruction.opcode == Opcode::Copy && instruction.effect != Instruction::Effect::Discard &&
           instruction.operands.size() == 1U && instruction.operands.front().kind == Operand::Kind::Symbol &&
           instruction.operands.front().symbol == instruction.destination;
}
} // namespace

auto lower(const core::CorePrepModule &module) -> Module
{
    Module lowered{module.name, {}};
    lowered.functions.reserve(module.functions.size());
    for(const auto &function : module.functions)
    {
        Function loweredFunction{function.symbol, function.parameters, function.return_type, function.entry, {}};
        loweredFunction.blocks.reserve(function.blocks.size());
        for(const auto &block : function.blocks)
        {
            Block loweredBlock{block.id, {}, LowerTerminator(block.terminator)};
            loweredBlock.instructions.reserve(block.instructions.size());
            for(const auto &instruction : block.instructions)
                loweredBlock.instructions.push_back(LowerInstruction(instruction));
            loweredFunction.blocks.push_back(std::move(loweredBlock));
        }
        lowered.functions.push_back(std::move(loweredFunction));
    }
    return lowered;
}

auto optimize(Module module) -> Module
{
    for(auto &function : module.functions)
    {
        const auto reachable = ReachableBlocks(function);
        std::erase_if(function.blocks, [&reachable](const Block &block) { return !reachable.contains(block.id); });
        for(auto &block : function.blocks)
            std::erase_if(block.instructions, IsSelfCopy);
    }
    return module;
}
} // namespace visual_xsharp::xpp
