// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0
#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <vector>
#include <utility>

namespace visual_xsharp::core
{
using SymbolId = std::uint64_t;
using BlockId = std::uint32_t;

enum class ValueType : std::uint8_t { Unit, Bool, Int64, Int32, String };
using Literal = std::variant<std::monostate, bool, std::int64_t, std::int32_t, std::string>;

struct Atom final
{
    enum class Kind : std::uint8_t { Variable, Literal } kind{Kind::Literal};
    ValueType type{ValueType::Unit};
    SymbolId symbol{};
    Literal literal{};

    [[nodiscard]] static auto variable(SymbolId id, ValueType value_type) -> Atom
    {
        return Atom{Kind::Variable, value_type, id, {}};
    }
    [[nodiscard]] static auto constant(Literal value, ValueType value_type) -> Atom
    {
        return Atom{Kind::Literal, value_type, 0, std::move(value)};
    }
};

enum class Operation : std::uint8_t
{
    Copy, Call, Add, Subtract, Multiply, Divide, FloorDivide, Remainder,
    LessThan, LessEqual, GreaterThan, GreaterEqual, Equal, NotEqual,
    LogicalAnd, LogicalOr, Negate, LogicalNot
};

struct Instruction final
{
    enum class Kind : std::uint8_t { Bind, Assign, Evaluate } kind{Kind::Evaluate};
    SymbolId destination{};
    ValueType type{ValueType::Unit};
    bool mutable_binding{};
    Operation operation{Operation::Copy};
    std::vector<Atom> operands;
};

struct Terminator final
{
    enum class Kind : std::uint8_t { Return, Branch, Jump, Unreachable } kind{Kind::Unreachable};
    Atom value{};
    BlockId true_target{};
    BlockId false_target{};
};

struct Block final
{
    BlockId id{};
    std::vector<Instruction> instructions;
    Terminator terminator;
};

struct Parameter final { SymbolId symbol{}; ValueType type{ValueType::Unit}; };
struct Function final
{
    SymbolId symbol{};
    std::vector<Parameter> parameters;
    ValueType return_type{ValueType::Unit};
    BlockId entry{};
    std::vector<Block> blocks;
};
struct CorePrepModule final { std::string name; std::vector<Function> functions; };

} // namespace visual_xsharp::core
