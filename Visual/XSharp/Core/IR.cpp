// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

#include "Visual/XSharp/Core/IR.hpp"

namespace Visual::XSharp::Core
{
auto Expression::Variable(SymbolName name, Type valueType) -> Expression
{
    return Expression{Kind::Variable, std::move(valueType), std::move(name), {}, Primitive::Add, {}, {}};
}

auto Expression::Constant(Literal value, Type valueType) -> Expression
{
    return Expression{Kind::Literal, std::move(valueType), {}, std::move(value), Primitive::Add, {}, {}};
}

auto Expression::Apply(Expression target, std::vector<Expression> arguments, Type resultType) -> Expression
{
    return Expression{Kind::Apply,
                      std::move(resultType),
                      {},
                      {},
                      Primitive::Add,
                      std::make_shared<Expression>(std::move(target)),
                      std::move(arguments)};
}

auto Expression::InvokePrimitive(Core::Primitive operation, std::vector<Expression> arguments, Type resultType)
    -> Expression
{
    return Expression{Kind::Primitive, std::move(resultType), {}, {}, operation, {}, std::move(arguments)};
}

auto Expression::operator==(const Expression &other) const -> bool
{
    const auto equalCallee = (!callee && !other.callee) || (callee && other.callee && *callee == *other.callee);
    return kind == other.kind && type == other.type && symbol == other.symbol && literal == other.literal &&
           primitive == other.primitive && equalCallee && operands == other.operands;
}

auto Statement::Bind(Binding value) -> Statement
{
    Statement statement;
    statement.kind = Kind::Bind;
    statement.binding = std::move(value);
    return statement;
}

auto Statement::Assign(SymbolName target, Expression value) -> Statement
{
    Statement statement;
    statement.kind = Kind::Assign;
    statement.destination = std::move(target);
    statement.expression = std::move(value);
    return statement;
}

auto Statement::Return(Expression value) -> Statement
{
    Statement statement;
    statement.kind = Kind::Return;
    statement.expression = std::move(value);
    return statement;
}

auto Statement::If(Expression condition, std::vector<Statement> whenTrue, std::vector<Statement> whenFalse) -> Statement
{
    Statement statement;
    statement.kind = Kind::If;
    statement.expression = std::move(condition);
    statement.trueBranch = std::move(whenTrue);
    statement.falseBranch = std::move(whenFalse);
    return statement;
}

auto Statement::Evaluate(Expression value) -> Statement
{
    Statement statement;
    statement.kind = Kind::Evaluate;
    statement.expression = std::move(value);
    return statement;
}
} // namespace Visual::XSharp::Core
