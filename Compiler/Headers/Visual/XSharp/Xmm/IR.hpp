// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Visual/XSharp/Xpp/IR.hpp"

namespace visual_xsharp::xmm
{
    using VirtualRegister = std::uint32_t;
    using BlockId = xpp::BlockId;
    enum class Opcode : std::uint8_t
    {
        LoadImmediate,
        Move,
        Call,
        Add,
        Subtract,
        Multiply,
        Divide,
        FloorDivide,
        Remainder,
        CompareLess,
        CompareLessEqual,
        CompareGreater,
        CompareGreaterEqual,
        CompareEqual,
        CompareNotEqual,
        AndBool,
        OrBool,
        Negate,
        NotBool,
        MakeClosure,

        // Source-compatible names for pre-v3 native clients. They intentionally alias the
        // typed operations; width and signedness now come from Instruction::result_type.
        AddI64 = Add,
        SubI64 = Subtract,
        MulI64 = Multiply,
        DivI64 = Divide,
        FloorDivI64 = FloorDivide,
        RemI64 = Remainder,
        CompareLessI64 = CompareLess,
        CompareLessEqualI64 = CompareLessEqual,
        CompareGreaterI64 = CompareGreater,
        CompareGreaterEqualI64 = CompareGreaterEqual,
        NegateI64 = Negate
    };
    struct Value final
    {
        // Function values preserve callable identity separately from data registers. This
        // prevents a direct call from being mistaken for a load from mutable Xmm storage.
        enum class Kind : std::uint8_t
        {
            Register,
            Immediate,
            Function
        } kind{ Kind::Immediate };
        core::Type type{ core::Type::unit() };
        VirtualRegister reg{};
        xpp::SymbolId symbol{};
        core::Literal immediate{};
    };
    struct Instruction final
    {
        Opcode opcode{ Opcode::Move };
        VirtualRegister destination{};
        // result_type is retained even when the result is discarded: verification and call
        // lowering still need the operation's semantic type.
        core::Type result_type{ core::Type::unit() };
        std::vector<Value> operands;
        bool has_result{};
        xpp::SymbolId closure_function{};
        std::vector<core::CaptureMode> capture_modes;
    };
    struct Terminator final
    {
        enum class Kind : std::uint8_t
        {
            Return,
            Branch,
            Jump,
            Unreachable
        } kind{ Kind::Unreachable };
        Value value{};
        BlockId true_target{};
        BlockId false_target{};
    };
    struct Block final
    {
        BlockId id{};
        std::vector<Instruction> instructions;
        Terminator terminator;
    };
    struct Function final
    {
        core::SymbolName symbol{};
        std::vector<VirtualRegister> parameter_registers;
        // Registers alone cannot reconstruct an ABI. Keep parameter types in declaration
        // order so the LLVM function signature never depends on first-use inference.
        std::vector<core::Type> parameter_types;
        core::Type return_type{ core::Type::unit() };
        BlockId entry{};
        std::vector<Block> blocks;
    };
    struct Module final
    {
        std::vector<std::u32string> name;
        std::vector<Function> functions;
    };

    [[nodiscard]] auto
    lower(const xpp::Module &module) -> Module;
    [[nodiscard]] auto
    optimize(Module module) -> Module;
} // namespace visual_xsharp::xmm
