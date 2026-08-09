// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "Visual/XSharp/Xpp/IR.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace visual_xsharp::xmm
{

using VirtualRegister = std::uint32_t;

struct Instruction final
{
    std::string opcode;
    std::vector<VirtualRegister> registers;
};

struct Block final
{
    std::string label;
    std::vector<Instruction> instructions;
};

struct Module final
{
    std::string name;
    std::vector<Block> blocks;
};

[[nodiscard]] auto lower(const xpp::Module &module) -> Module;
[[nodiscard]] auto optimize(Module module) -> Module;

} // namespace visual_xsharp::xmm
