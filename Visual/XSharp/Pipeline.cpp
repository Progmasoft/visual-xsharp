// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

#include "Visual/XSharp/Core/CorePrep.hpp"
#include "Visual/XSharp/Xmm/IR.hpp"
#include "Visual/XSharp/Xpp/IR.hpp"

#include <algorithm>
#include <utility>

namespace visual_xsharp::core
{

auto prepare(const CoreModule &module) -> CorePrepModule
{
    CorePrepModule prepared{module.name, {}};
    prepared.bindings.reserve(module.bindings.size());
    for(const auto &binding : module.bindings)
        prepared.bindings.push_back(CorePrepBinding{binding.symbol, binding.type, binding.value});
    return prepared;
}

} // namespace visual_xsharp::core

namespace visual_xsharp::xpp
{

auto lower(const core::CorePrepModule &module) -> Module
{
    Block entry{"entry", {}};
    entry.instructions.reserve(module.bindings.size() + 1);
    for(const auto &binding : module.bindings)
        entry.instructions.push_back(Instruction{"bind.literal", {binding.symbol}});
    entry.instructions.push_back(Instruction{"return", {}});
    return Module{module.name, {std::move(entry)}};
}

auto optimize(Module module) -> Module
{
    std::erase_if(module.blocks, [](const Block &block) { return block.instructions.empty(); });
    for(auto &block : module.blocks)
    {
        auto duplicate = std::unique(block.instructions.begin(), block.instructions.end(),
                                     [](const Instruction &left, const Instruction &right)
                                     { return left.opcode == "return" && right.opcode == "return"; });
        block.instructions.erase(duplicate, block.instructions.end());
    }
    return module;
}

} // namespace visual_xsharp::xpp

namespace visual_xsharp::xmm
{

auto lower(const xpp::Module &module) -> Module
{
    Module lowered{module.name, {}};
    lowered.blocks.reserve(module.blocks.size());
    for(const auto &block : module.blocks)
    {
        Block lowered_block{block.label, {}};
        lowered_block.instructions.reserve(block.instructions.size());
        for(const auto &instruction : block.instructions)
        {
            std::vector<VirtualRegister> registers;
            registers.reserve(instruction.operands.size());
            for(const auto operand : instruction.operands)
                registers.push_back(static_cast<VirtualRegister>(operand));
            lowered_block.instructions.push_back(Instruction{instruction.opcode, std::move(registers)});
        }
        lowered.blocks.push_back(std::move(lowered_block));
    }
    return lowered;
}

auto optimize(Module module) -> Module
{
    for(auto &block : module.blocks)
    {
        auto duplicate = std::unique(
            block.instructions.begin(), block.instructions.end(), [](const Instruction &left, const Instruction &right)
            { return left.opcode == right.opcode && left.registers == right.registers && left.opcode == "move"; });
        block.instructions.erase(duplicate, block.instructions.end());
    }
    return module;
}

} // namespace visual_xsharp::xmm
