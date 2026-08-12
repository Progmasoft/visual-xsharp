// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "Visual/XSharp/Core/CorePrep.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace visual_xsharp::xpp
{
using SymbolId = core::SymbolId;
using BlockId = core::BlockId;

enum class Opcode : std::uint8_t
{
    Copy, Call, Add, Subtract, Multiply, Divide, FloorDivide, Remainder,
    CompareLess, CompareLessEqual, CompareGreater, CompareGreaterEqual, CompareEqual, CompareNotEqual,
    LogicalAnd, LogicalOr, Negate, LogicalNot
};
struct Operand final
{
    enum class Kind : std::uint8_t { Symbol, Literal } kind{Kind::Literal};
    core::Type type{core::Type::unit()};
    SymbolId symbol{};
    core::Literal literal{};
};
struct Instruction final
{
    enum class Effect : std::uint8_t { Define, Store, Discard } effect{Effect::Discard};
    Opcode opcode{Opcode::Copy};
    SymbolId destination{};
    core::Type result_type{core::Type::unit()};
    std::vector<Operand> operands;
};
struct Terminator final
{
    enum class Kind : std::uint8_t { Return, Branch, Jump, Unreachable } kind{Kind::Unreachable};
    Operand value{};
    BlockId true_target{};
    BlockId false_target{};
};
struct Block final { BlockId id{}; std::vector<Instruction> instructions; Terminator terminator; };
struct Function final
{
    SymbolId symbol{};
    std::vector<core::Parameter> parameters;
    core::Type return_type{core::Type::unit()};
    BlockId entry{};
    std::vector<Block> blocks;
};
struct Module final { std::vector<std::u32string> name; std::vector<Function> functions; };

[[nodiscard]] auto lower(const core::CorePrepModule &module) -> Module;
[[nodiscard]] auto optimize(Module module) -> Module;
} // namespace visual_xsharp::xpp
