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

struct SymbolName final
{
    SymbolId id{};
    std::u32string spelling;
    [[nodiscard]] auto operator==(const SymbolName &) const -> bool = default;
};

struct Type final
{
    enum class Kind : std::uint8_t { Unit, Bool, Int64, Int32, String, Function, Named, TypeVariable };
    Kind kind{Kind::Unit};
    std::vector<std::u32string> name;
    std::vector<Type> components;
    SymbolName variable;

    [[nodiscard]] static auto unit() -> Type { return Type{Kind::Unit, {}, {}, {}}; }
    [[nodiscard]] static auto boolean() -> Type { return Type{Kind::Bool, {}, {}, {}}; }
    [[nodiscard]] static auto int64() -> Type { return Type{Kind::Int64, {}, {}, {}}; }
    [[nodiscard]] static auto int32() -> Type { return Type{Kind::Int32, {}, {}, {}}; }
    [[nodiscard]] static auto string() -> Type { return Type{Kind::String, {}, {}, {}}; }
    [[nodiscard]] static auto function(std::vector<Type> parameters, Type result) -> Type
    {
        parameters.push_back(std::move(result));
        return Type{Kind::Function, {}, std::move(parameters), {}};
    }
    [[nodiscard]] static auto named(std::vector<std::u32string> qualified_name, std::vector<Type> arguments = {}) -> Type
    {
        return Type{Kind::Named, std::move(qualified_name), std::move(arguments), {}};
    }
    [[nodiscard]] static auto type_variable(SymbolName symbol) -> Type
    {
        return Type{Kind::TypeVariable, {}, {}, std::move(symbol)};
    }
    [[nodiscard]] auto operator==(const Type &) const -> bool = default;
};

using Literal = std::variant<std::monostate, bool, std::int64_t, std::int32_t, std::u32string>;

struct Atom final
{
    enum class Kind : std::uint8_t { Variable, Literal } kind{Kind::Literal};
    Type type{Type::unit()};
    SymbolName symbol{};
    Literal literal{};

    [[nodiscard]] static auto variable(SymbolName name, Type value_type) -> Atom
    {
        return Atom{Kind::Variable, std::move(value_type), std::move(name), {}};
    }
    [[nodiscard]] static auto variable(SymbolId id, Type value_type) -> Atom
    {
        return variable(SymbolName{id, {}}, std::move(value_type));
    }
    [[nodiscard]] static auto constant(Literal value, Type value_type) -> Atom
    {
        return Atom{Kind::Literal, std::move(value_type), {}, std::move(value)};
    }
    [[nodiscard]] auto operator==(const Atom &) const -> bool = default;
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
    SymbolName destination{};
    Type type{Type::unit()};
    bool mutable_binding{};
    Operation operation{Operation::Copy};
    std::vector<Atom> operands;
    [[nodiscard]] auto operator==(const Instruction &) const -> bool = default;
};

struct Terminator final
{
    enum class Kind : std::uint8_t { Return, Branch, Jump, Unreachable } kind{Kind::Unreachable};
    Atom value{};
    BlockId true_target{};
    BlockId false_target{};
    [[nodiscard]] auto operator==(const Terminator &) const -> bool = default;
};

struct Block final
{
    BlockId id{};
    std::vector<Instruction> instructions;
    Terminator terminator;
    [[nodiscard]] auto operator==(const Block &) const -> bool = default;
};

struct Parameter final
{
    SymbolName symbol{};
    Type type{Type::unit()};
    [[nodiscard]] auto operator==(const Parameter &) const -> bool = default;
};
struct Function final
{
    SymbolName symbol{};
    std::vector<Parameter> parameters;
    Type return_type{Type::unit()};
    BlockId entry{};
    std::vector<Block> blocks;
    [[nodiscard]] auto operator==(const Function &) const -> bool = default;
};
struct CorePrepModule final
{
    std::vector<std::u32string> name;
    std::vector<Function> functions;
    [[nodiscard]] auto operator==(const CorePrepModule &) const -> bool = default;
};

} // namespace visual_xsharp::core
