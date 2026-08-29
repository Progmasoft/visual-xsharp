// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

#include "Visual/XSharp/Xmm/IR.hpp"
#include "Visual/XSharp/Xpp/IR.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace visual_xsharp::xpp
{
namespace
{
auto lower_operand(const core::Atom &atom) -> Operand
{
    // CorePrep has already resolved names, so the numeric symbol is authoritative here.
    // Spelling remains on declarations for diagnostics and eventual external mangling.
    return Operand{atom.kind == core::Atom::Kind::Variable ? Operand::Kind::Symbol : Operand::Kind::Literal,
                   atom.type, atom.symbol.id, atom.literal};
}

auto lower_operation(core::Operation operation) -> Opcode
{
    switch(operation)
    {
    case core::Operation::Copy: return Opcode::Copy;
    case core::Operation::Call: return Opcode::Call;
    case core::Operation::Add: return Opcode::Add;
    case core::Operation::Subtract: return Opcode::Subtract;
    case core::Operation::Multiply: return Opcode::Multiply;
    case core::Operation::Divide: return Opcode::Divide;
    case core::Operation::FloorDivide: return Opcode::FloorDivide;
    case core::Operation::Remainder: return Opcode::Remainder;
    case core::Operation::LessThan: return Opcode::CompareLess;
    case core::Operation::LessEqual: return Opcode::CompareLessEqual;
    case core::Operation::GreaterThan: return Opcode::CompareGreater;
    case core::Operation::GreaterEqual: return Opcode::CompareGreaterEqual;
    case core::Operation::Equal: return Opcode::CompareEqual;
    case core::Operation::NotEqual: return Opcode::CompareNotEqual;
    case core::Operation::LogicalAnd: return Opcode::LogicalAnd;
    case core::Operation::LogicalOr: return Opcode::LogicalOr;
    case core::Operation::Negate: return Opcode::Negate;
    case core::Operation::LogicalNot: return Opcode::LogicalNot;
    }
    return Opcode::Copy;
}

auto lower_instruction(const core::Instruction &instruction) -> Instruction
{
    Instruction lowered{};
    // Preserve Bind/Assign/Discard as an explicit effect. Collapsing them into opcode
    // alone would lose the difference between defining storage and mutating it.
    lowered.effect = instruction.kind == core::Instruction::Kind::Bind
                         ? Instruction::Effect::Define
                         : instruction.kind == core::Instruction::Kind::Assign ? Instruction::Effect::Store
                                                                               : Instruction::Effect::Discard;
    lowered.opcode = lower_operation(instruction.operation);
    lowered.destination = instruction.destination.id;
    lowered.result_type = instruction.type;
    lowered.operands.reserve(instruction.operands.size());
    for(const auto &operand : instruction.operands)
        lowered.operands.push_back(lower_operand(operand));
    return lowered;
}

auto lower_terminator(const core::Terminator &terminator) -> Terminator
{
    Terminator lowered{};
    lowered.kind = static_cast<Terminator::Kind>(terminator.kind);
    lowered.value = lower_operand(terminator.value);
    lowered.true_target = terminator.true_target;
    lowered.false_target = terminator.false_target;
    return lowered;
}

auto reachable_blocks(const Function &function) -> std::unordered_set<BlockId>
{
    // Reachability is intentionally structural and side-effect free. It follows only
    // terminators and does not speculate about constant conditions at this stage.
    std::unordered_set<BlockId> reachable;
    std::vector<BlockId> pending{function.entry};
    while(!pending.empty())
    {
        const auto id = pending.back();
        pending.pop_back();
        if(!reachable.insert(id).second)
            continue;
        const auto found = std::find_if(function.blocks.begin(), function.blocks.end(),
                                        [id](const Block &block) { return block.id == id; });
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

auto is_self_copy(const Instruction &instruction) -> bool
{
    return instruction.opcode == Opcode::Copy && instruction.effect != Instruction::Effect::Discard &&
           instruction.operands.size() == 1 && instruction.operands.front().kind == Operand::Kind::Symbol &&
           instruction.operands.front().symbol == instruction.destination;
}
} // namespace

auto lower(const core::CorePrepModule &module) -> Module
{
    Module lowered{module.name, {}};
    lowered.functions.reserve(module.functions.size());
    for(const auto &function : module.functions)
    {
        Function lowered_function{function.symbol, function.parameters, function.return_type, function.entry, {}};
        lowered_function.blocks.reserve(function.blocks.size());
        for(const auto &block : function.blocks)
        {
            Block lowered_block{block.id, {}, lower_terminator(block.terminator)};
            lowered_block.instructions.reserve(block.instructions.size());
            for(const auto &instruction : block.instructions)
                lowered_block.instructions.push_back(lower_instruction(instruction));
            lowered_function.blocks.push_back(std::move(lowered_block));
        }
        lowered.functions.push_back(std::move(lowered_function));
    }
    return lowered;
}

auto optimize(Module module) -> Module
{
    for(auto &function : module.functions)
    {
        const auto reachable = reachable_blocks(function);
        std::erase_if(function.blocks, [&reachable](const Block &block) { return !reachable.contains(block.id); });
        for(auto &block : function.blocks)
            std::erase_if(block.instructions, is_self_copy);
    }
    return module;
}
} // namespace visual_xsharp::xpp

namespace visual_xsharp::xmm
{
namespace
{
struct RegisterMap final
{
    // Allocate deterministically on first encounter while preserving one register for
    // each resolved symbol across all blocks in the function. Register zero is reserved
    // as the invalid/default value used by the verifier.
    std::unordered_map<xpp::SymbolId, VirtualRegister> registers;
    VirtualRegister next{1};

    auto get(xpp::SymbolId symbol) -> VirtualRegister
    {
        if(const auto found = registers.find(symbol); found != registers.end())
            return found->second;
        const auto allocated = next++;
        registers.emplace(symbol, allocated);
        return allocated;
    }
};

auto lower_opcode(xpp::Opcode opcode, const core::Type &type) -> Opcode
{
    switch(opcode)
    {
    case xpp::Opcode::Copy: return Opcode::Move;
    case xpp::Opcode::Call: return Opcode::Call;
    case xpp::Opcode::Add: return Opcode::AddI64;
    case xpp::Opcode::Subtract: return Opcode::SubI64;
    case xpp::Opcode::Multiply: return Opcode::MulI64;
    case xpp::Opcode::Divide: return Opcode::DivI64;
    case xpp::Opcode::FloorDivide: return Opcode::FloorDivI64;
    case xpp::Opcode::Remainder: return Opcode::RemI64;
    case xpp::Opcode::CompareLess: return Opcode::CompareLessI64;
    case xpp::Opcode::CompareLessEqual: return Opcode::CompareLessEqualI64;
    case xpp::Opcode::CompareGreater: return Opcode::CompareGreaterI64;
    case xpp::Opcode::CompareGreaterEqual: return Opcode::CompareGreaterEqualI64;
    case xpp::Opcode::CompareEqual: return Opcode::CompareEqual;
    case xpp::Opcode::CompareNotEqual: return Opcode::CompareNotEqual;
    case xpp::Opcode::LogicalAnd: return Opcode::AndBool;
    case xpp::Opcode::LogicalOr: return Opcode::OrBool;
    case xpp::Opcode::Negate: return Opcode::NegateI64;
    case xpp::Opcode::LogicalNot: return Opcode::NotBool;
    }
    static_cast<void>(type);
    return Opcode::Move;
}

auto lower_value(const xpp::Operand &operand, RegisterMap &map) -> Value
{
    if(operand.kind == xpp::Operand::Kind::Symbol)
    {
        if(operand.type.kind == core::Type::Kind::Function)
            // Direct callees retain symbol identity and never consume a data register.
            // This distinction is what lets the backend resolve forward/recursive calls.
            return Value{Value::Kind::Function, operand.type, 0, operand.symbol, {}};
        return Value{Value::Kind::Register, operand.type, map.get(operand.symbol), 0, {}};
    }
    return Value{Value::Kind::Immediate, operand.type, 0, 0, operand.literal};
}

auto lower_terminator(const xpp::Terminator &terminator, RegisterMap &map) -> Terminator
{
    return Terminator{static_cast<Terminator::Kind>(terminator.kind), lower_value(terminator.value, map),
                      terminator.true_target, terminator.false_target};
}
} // namespace

auto lower(const xpp::Module &module) -> Module
{
    Module lowered{module.name, {}};
    lowered.functions.reserve(module.functions.size());
    for(const auto &function : module.functions)
    {
        RegisterMap register_map;
        Function lowered_function{function.symbol, {}, {}, function.return_type, function.entry, {}};
        for(const auto &parameter : function.parameters)
        {
            lowered_function.parameter_registers.push_back(register_map.get(parameter.symbol.id));
            lowered_function.parameter_types.push_back(parameter.type);
        }
        lowered_function.blocks.reserve(function.blocks.size());
        for(const auto &block : function.blocks)
        {
            Block lowered_block{block.id, {}, {}};
            lowered_block.instructions.reserve(block.instructions.size());
            for(const auto &instruction : block.instructions)
            {
                Instruction lowered_instruction{};
                lowered_instruction.opcode = lower_opcode(instruction.opcode, instruction.result_type);
                lowered_instruction.has_result = instruction.effect != xpp::Instruction::Effect::Discard;
                lowered_instruction.result_type = instruction.result_type;
                if(lowered_instruction.has_result)
                    lowered_instruction.destination = register_map.get(instruction.destination);
                for(const auto &operand : instruction.operands)
                    lowered_instruction.operands.push_back(lower_value(operand, register_map));
                lowered_block.instructions.push_back(std::move(lowered_instruction));
            }
            lowered_block.terminator = lower_terminator(block.terminator, register_map);
            lowered_function.blocks.push_back(std::move(lowered_block));
        }
        lowered.functions.push_back(std::move(lowered_function));
    }
    return lowered;
}

auto optimize(Module module) -> Module
{
    // This pass removes only storage no-ops. More aggressive propagation needs a real
    // control-flow/data-flow proof and must not be approximated by local rewriting.
    for(auto &function : module.functions)
    {
        for(auto &block : function.blocks)
        {
            std::erase_if(block.instructions, [](const Instruction &instruction) {
                return instruction.opcode == Opcode::Move && instruction.has_result &&
                       instruction.operands.size() == 1 && instruction.operands.front().kind == Value::Kind::Register &&
                       instruction.destination == instruction.operands.front().reg;
            });
        }
    }
    return module;
}
} // namespace visual_xsharp::xmm
