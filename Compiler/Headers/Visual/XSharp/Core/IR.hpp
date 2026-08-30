// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
#pragma once

#include <cstdint>
#include <memory>
#include <utility>
#include <variant>
#include <vector>

#include "Visual/XSharp/Core/CorePrep.hpp"
#include "Visual/XSharp/Namespace.hpp"

namespace Visual::XSharp::Core
{
    using SymbolId = ::visual_xsharp::core::SymbolId;
    using SymbolName = ::visual_xsharp::core::SymbolName;
    using Type = ::visual_xsharp::core::Type;
    using Literal = ::visual_xsharp::core::Literal;

    enum class Primitive : std::uint8_t
    {
        Add,
        Subtract,
        Multiply,
        Divide,
        FloorDivide,
        Remainder,
        LessThan,
        LessEqual,
        GreaterThan,
        GreaterEqual,
        Equal,
        NotEqual,
        LogicalAnd,
        LogicalOr,
        Negate,
        LogicalNot
    };

    struct Expression final
    {
        enum class Kind : std::uint8_t
        {
            Variable,
            Literal,
            Apply,
            Primitive
        };

        Kind kind{ Kind::Literal };
        Type type{ Type::unit() };
        SymbolName symbol{};
        Literal literal{};
        Core::Primitive primitive{ Core::Primitive::Add };
        std::shared_ptr<Expression> callee;
        std::vector<Expression> operands;

        [[nodiscard]] static auto
        Variable(SymbolName name, Type valueType) -> Expression;
        [[nodiscard]] static auto
        Constant(Literal value, Type valueType) -> Expression;
        [[nodiscard]] static auto
        Apply(Expression target, std::vector<Expression> arguments, Type resultType)
            -> Expression;
        [[nodiscard]] static auto
        InvokePrimitive(Core::Primitive operation, std::vector<Expression> arguments, Type resultType) -> Expression;
        [[nodiscard]] auto
        operator==(const Expression &other) const -> bool;
    };

    struct Binding final
    {
        SymbolName symbol{};
        Type type{ Type::unit() };
        bool mutableBinding{};
        Expression value{};
        [[nodiscard]] auto
        operator==(const Binding &) const -> bool = default;
    };

    struct Statement final
    {
        enum class Kind : std::uint8_t
        {
            Bind,
            Assign,
            Return,
            If,
            Evaluate
        };

        Kind kind{ Kind::Evaluate };
        Binding binding{};
        SymbolName destination{};
        Expression expression{};
        std::vector<Statement> trueBranch;
        std::vector<Statement> falseBranch;

        [[nodiscard]] static auto
        Bind(Binding value) -> Statement;
        [[nodiscard]] static auto
        Assign(SymbolName target, Expression value) -> Statement;
        [[nodiscard]] static auto
        Return(Expression value) -> Statement;
        [[nodiscard]] static auto
        If(Expression condition, std::vector<Statement> whenTrue, std::vector<Statement> whenFalse) -> Statement;
        [[nodiscard]] static auto
        Evaluate(Expression value) -> Statement;
        [[nodiscard]] auto
        operator==(const Statement &) const -> bool = default;
    };

    struct Parameter final
    {
        SymbolName symbol{};
        Type type{ Type::unit() };
        [[nodiscard]] auto
        operator==(const Parameter &) const -> bool = default;
    };

    struct Function final
    {
        SymbolName symbol{};
        std::vector<Parameter> parameters;
        Type returnType{ Type::unit() };
        std::vector<Statement> body;
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
} // namespace Visual::XSharp::Core
