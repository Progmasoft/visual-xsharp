// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

#include "Visual/XSharp/Xmm/IR.hpp"

#include <algorithm>
#include <unordered_map>
#include <utility>

namespace visual_xsharp::xmm
{
namespace
{
struct RegisterMap final
{
    // Allocate deterministically on first encounter while preserving one register for
    // each resolved symbol across all blocks. Register zero remains invalid/reserved.
    std::unordered_map<xpp::SymbolId, VirtualRegister> registers;
    VirtualRegister next{1U};

    auto Get(xpp::SymbolId symbol) -> VirtualRegister
    {
        if(const auto found = registers.find(symbol); found != registers.end())
            return found->second;
        const auto allocated = next++;
        registers.emplace(symbol, allocated);
        return allocated;
    }
};

auto LowerOpcode(xpp::Opcode opcode) -> Opcode
{
    switch(opcode)
    {
    case xpp::Opcode::Copy:
        return Opcode::Move;
    case xpp::Opcode::Call:
        return Opcode::Call;
    case xpp::Opcode::Add:
        return Opcode::AddI64;
    case xpp::Opcode::Subtract:
        return Opcode::SubI64;
    case xpp::Opcode::Multiply:
        return Opcode::MulI64;
    case xpp::Opcode::Divide:
        return Opcode::DivI64;
    case xpp::Opcode::FloorDivide:
        return Opcode::FloorDivI64;
    case xpp::Opcode::Remainder:
        return Opcode::RemI64;
    case xpp::Opcode::CompareLess:
        return Opcode::CompareLessI64;
    case xpp::Opcode::CompareLessEqual:
        return Opcode::CompareLessEqualI64;
    case xpp::Opcode::CompareGreater:
        return Opcode::CompareGreaterI64;
    case xpp::Opcode::CompareGreaterEqual:
        return Opcode::CompareGreaterEqualI64;
    case xpp::Opcode::CompareEqual:
        return Opcode::CompareEqual;
    case xpp::Opcode::CompareNotEqual:
        return Opcode::CompareNotEqual;
    case xpp::Opcode::LogicalAnd:
        return Opcode::AndBool;
    case xpp::Opcode::LogicalOr:
        return Opcode::OrBool;
    case xpp::Opcode::Negate:
        return Opcode::NegateI64;
    case xpp::Opcode::LogicalNot:
        return Opcode::NotBool;
    }
    return Opcode::Move;
}

auto LowerValue(const xpp::Operand &operand, RegisterMap &map) -> Value
{
    if(operand.kind == xpp::Operand::Kind::Symbol)
    {
        if(operand.type.kind == core::Type::Kind::Function)
            // Direct callees retain symbol identity and never consume a data register.
            // This lets the backend resolve forward and recursive calls uniformly.
            return Value{Value::Kind::Function, operand.type, 0U, operand.symbol, {}};
        return Value{Value::Kind::Register, operand.type, map.Get(operand.symbol), 0U, {}};
    }
    return Value{Value::Kind::Immediate, operand.type, 0U, 0U, operand.literal};
}

auto LowerTerminator(const xpp::Terminator &terminator, RegisterMap &map) -> Terminator
{
    return {static_cast<Terminator::Kind>(terminator.kind), LowerValue(terminator.value, map), terminator.true_target,
            terminator.false_target};
}
} // namespace

auto lower(const xpp::Module &module) -> Module
{
    Module lowered{module.name, {}};
    lowered.functions.reserve(module.functions.size());
    for(const auto &function : module.functions)
    {
        RegisterMap registerMap;
        Function loweredFunction{function.symbol, {}, {}, function.return_type, function.entry, {}};
        for(const auto &parameter : function.parameters)
        {
            loweredFunction.parameter_registers.push_back(registerMap.Get(parameter.symbol.id));
            loweredFunction.parameter_types.push_back(parameter.type);
        }
        loweredFunction.blocks.reserve(function.blocks.size());
        for(const auto &block : function.blocks)
        {
            Block loweredBlock{block.id, {}, {}};
            loweredBlock.instructions.reserve(block.instructions.size());
            for(const auto &instruction : block.instructions)
            {
                Instruction loweredInstruction{};
                loweredInstruction.opcode = LowerOpcode(instruction.opcode);
                loweredInstruction.has_result = instruction.effect != xpp::Instruction::Effect::Discard;
                loweredInstruction.result_type = instruction.result_type;
                if(loweredInstruction.has_result)
                    loweredInstruction.destination = registerMap.Get(instruction.destination);
                for(const auto &operand : instruction.operands)
                    loweredInstruction.operands.push_back(LowerValue(operand, registerMap));
                loweredBlock.instructions.push_back(std::move(loweredInstruction));
            }
            loweredBlock.terminator = LowerTerminator(block.terminator, registerMap);
            loweredFunction.blocks.push_back(std::move(loweredBlock));
        }
        lowered.functions.push_back(std::move(loweredFunction));
    }
    return lowered;
}

auto optimize(Module module) -> Module
{
    // This pass removes only storage no-ops. Propagation requires a control-flow and
    // data-flow proof and must never be approximated by a local rewrite.
    for(auto &function : module.functions)
        for(auto &block : function.blocks)
            std::erase_if(block.instructions,
                          [](const Instruction &instruction)
                          {
                              return instruction.opcode == Opcode::Move && instruction.has_result &&
                                     instruction.operands.size() == 1U &&
                                     instruction.operands.front().kind == Value::Kind::Register &&
                                     instruction.destination == instruction.operands.front().reg;
                          });
    return module;
}
} // namespace visual_xsharp::xmm
