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
using BlockId = xpp::BlockId;
enum class Opcode : std::uint8_t
{
    LoadImmediate, Move, Call, AddI64, SubI64, MulI64, DivI64, FloorDivI64, RemI64,
    CompareLessI64, CompareLessEqualI64, CompareGreaterI64, CompareGreaterEqualI64,
    CompareEqual, CompareNotEqual, AndBool, OrBool, NegateI64, NotBool
};
struct Value final
{
    // Function values preserve callable identity separately from data registers. This
    // prevents a direct call from being mistaken for a load from mutable Xmm storage.
    enum class Kind : std::uint8_t { Register, Immediate, Function } kind{Kind::Immediate};
    core::Type type{core::Type::unit()};
    VirtualRegister reg{};
    xpp::SymbolId symbol{};
    core::Literal immediate{};
};
struct Instruction final
{
    Opcode opcode{Opcode::Move};
    VirtualRegister destination{};
    // result_type is retained even when the result is discarded: verification and call
    // lowering still need the operation's semantic type.
    core::Type result_type{core::Type::unit()};
    std::vector<Value> operands;
    bool has_result{};
};
struct Terminator final
{
    enum class Kind : std::uint8_t { Return, Branch, Jump, Unreachable } kind{Kind::Unreachable};
    Value value{};
    BlockId true_target{};
    BlockId false_target{};
};
struct Block final { BlockId id{}; std::vector<Instruction> instructions; Terminator terminator; };
struct Function final
{
    core::SymbolName symbol{};
    std::vector<VirtualRegister> parameter_registers;
    // Registers alone cannot reconstruct an ABI. Keep parameter types in declaration
    // order so the LLVM function signature never depends on first-use inference.
    std::vector<core::Type> parameter_types;
    core::Type return_type{core::Type::unit()};
    BlockId entry{};
    std::vector<Block> blocks;
};
struct Module final { std::vector<std::u32string> name; std::vector<Function> functions; };

[[nodiscard]] auto lower(const xpp::Module &module) -> Module;
[[nodiscard]] auto optimize(Module module) -> Module;
} // namespace visual_xsharp::xmm
