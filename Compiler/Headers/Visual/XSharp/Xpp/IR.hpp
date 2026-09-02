// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Visual/XSharp/Core/CorePrep.hpp"

namespace visual_xsharp::xpp
{
    using SymbolId = core::SymbolId;
    using BlockId = core::BlockId;

    enum class Opcode : std::uint8_t
    {
        Copy,
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
        LogicalAnd,
        LogicalOr,
        Negate,
        LogicalNot,
        MakeClosure,

        // Ownership is explicit from Xpp onward. Strong operations consume object
        // pointers; weak and unowned operations consume/produce control handles while
        // preserving the source language type in result_type.
        RetainStrong,
        ReleaseStrong,
        MakeWeak,
        LockWeak,
        ReleaseWeak,
        MakeUnowned,
        LoadUnowned,
        ReleaseUnowned
    };
    struct Operand final
    {
        enum class Kind : std::uint8_t
        {
            Symbol,
            Literal
        } kind{ Kind::Literal };
        core::Type type{ core::Type::unit() };
        SymbolId symbol{};
        core::Literal literal{};
        [[nodiscard]] auto
        operator==(const Operand &) const -> bool = default;
    };
    struct Instruction final
    {
        enum class Effect : std::uint8_t
        {
            Define,
            Store,
            Discard
        } effect{ Effect::Discard };
        Opcode opcode{ Opcode::Copy };
        SymbolId destination{};
        core::Type result_type{ core::Type::unit() };
        std::vector<Operand> operands;
        SymbolId closure_function{};
        std::vector<core::CaptureMode> capture_modes;
        [[nodiscard]] auto
        operator==(const Instruction &) const -> bool = default;
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
        Operand value{};
        BlockId true_target{};
        BlockId false_target{};
        [[nodiscard]] auto
        operator==(const Terminator &) const -> bool = default;
    };
    struct Block final
    {
        BlockId id{};
        std::vector<Instruction> instructions;
        Terminator terminator;
        [[nodiscard]] auto
        operator==(const Block &) const -> bool = default;
    };
    struct Function final
    {
        core::SymbolName symbol{};
        std::vector<core::Parameter> parameters;
        core::Type return_type{ core::Type::unit() };
        BlockId entry{};
        std::vector<Block> blocks;
        [[nodiscard]] auto
        operator==(const Function &) const -> bool = default;
    };
    struct Module final
    {
        std::vector<std::u32string> name;
        std::vector<Function> functions;
        [[nodiscard]] auto
        operator==(const Module &) const -> bool = default;
    };

    [[nodiscard]] auto
    lower(const core::CorePrepModule &module) -> Module;
    [[nodiscard]] auto
    optimize(Module module) -> Module;
} // namespace visual_xsharp::xpp
