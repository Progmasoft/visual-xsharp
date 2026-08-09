// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "Visual/XSharp/Core.hpp"
#include "Visual/XSharp/Core/CorePrep.hpp"

#include <string>
#include <vector>

namespace visual_xsharp::xpp
{

struct Instruction final
{
    std::string opcode;
    std::vector<SymbolId> operands;
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

[[nodiscard]] auto lower(const core::CorePrepModule &module) -> Module;
[[nodiscard]] auto optimize(Module module) -> Module;

} // namespace visual_xsharp::xpp
