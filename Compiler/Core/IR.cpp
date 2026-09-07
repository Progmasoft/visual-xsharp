// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

#include "Visual/XSharp/Core/IR.hpp"

namespace Visual::XSharp::Core
{
    auto
    Capture::operator==(const Capture &other) const -> bool
    {
        const auto equalValue = (!value && !other.value) || (value && other.value && *value == *other.value);
        return mode == other.mode && symbol == other.symbol && type == other.type && equalValue;
    }

    auto
    Expression::Variable(SymbolName name, Type valueType) -> Expression
    {
        Expression expression;
        expression.kind = Kind::Variable;
        expression.type = std::move(valueType);
        expression.symbol = std::move(name);
        return expression;
    }

    auto
    Expression::Constant(Literal value, Type valueType) -> Expression
    {
        Expression expression;
        expression.kind = Kind::Literal;
        expression.type = std::move(valueType);
        expression.literal = std::move(value);
        return expression;
    }

    auto
    Expression::Apply(Expression target, std::vector<Expression> arguments, Type resultType) -> Expression
    {
        Expression expression;
        expression.kind = Kind::Apply;
        expression.type = std::move(resultType);
        expression.callee = std::make_shared<Expression>(std::move(target));
        expression.operands = std::move(arguments);
        return expression;
    }

    auto
    Expression::InvokePrimitive(Core::Primitive operation, std::vector<Expression> arguments, Type resultType)
        -> Expression
    {
        Expression expression;
        expression.kind = Kind::Primitive;
        expression.type = std::move(resultType);
        expression.primitive = operation;
        expression.operands = std::move(arguments);
        return expression;
    }

    auto
    Expression::Closure(
        std::vector<Capture> captured,
        std::vector<std::pair<SymbolName, Type>> parameters,
        Type returnType,
        std::vector<Statement> body,
        Type valueType) -> Expression
    {
        Expression expression;
        expression.kind = Kind::Closure;
        expression.type = std::move(valueType);
        expression.captures = std::move(captured);
        expression.closureParameters = std::move(parameters);
        expression.closureReturnType = std::move(returnType);
        expression.closureBody = std::make_shared<std::vector<Statement>>(std::move(body));
        return expression;
    }

    auto
    Expression::operator==(const Expression &other) const -> bool
    {
        const auto equalCallee = (!callee && !other.callee) || (callee && other.callee && *callee == *other.callee);
        const auto equalBody = (!closureBody && !other.closureBody)
                               || (closureBody && other.closureBody && *closureBody == *other.closureBody);
        return kind == other.kind && type == other.type && symbol == other.symbol
               && literal == other.literal && primitive == other.primitive
               && equalCallee && operands == other.operands && captures == other.captures
               && closureParameters == other.closureParameters
               && closureReturnType == other.closureReturnType && equalBody;
    }

    auto
    Statement::Bind(Binding value) -> Statement
    {
        Statement statement;
        statement.kind = Kind::Bind;
        statement.binding = std::move(value);
        return statement;
    }

    auto
    Statement::Assign(SymbolName target, Expression value) -> Statement
    {
        Statement statement;
        statement.kind = Kind::Assign;
        statement.destination = std::move(target);
        statement.expression = std::move(value);
        return statement;
    }

    auto
    Statement::Return(Expression value) -> Statement
    {
        Statement statement;
        statement.kind = Kind::Return;
        statement.expression = std::move(value);
        return statement;
    }

    auto
    Statement::If(Expression condition, std::vector<Statement> whenTrue, std::vector<Statement> whenFalse) -> Statement
    {
        Statement statement;
        statement.kind = Kind::If;
        statement.expression = std::move(condition);
        statement.trueBranch = std::move(whenTrue);
        statement.falseBranch = std::move(whenFalse);
        return statement;
    }

    auto
    Statement::Evaluate(Expression value) -> Statement
    {
        Statement statement;
        statement.kind = Kind::Evaluate;
        statement.expression = std::move(value);
        return statement;
    }
} // namespace Visual::XSharp::Core
