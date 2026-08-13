// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

#include "Visual/XSharp/Core/Verifier.hpp"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace Visual::XSharp::Core
{
namespace
{
struct Definition final
{
    Type type;
    bool mutableBinding{};
    std::u32string spelling;
};
using Environment = std::unordered_map<SymbolId, Definition>;

class FunctionVerifier final
{
public:
    FunctionVerifier(const Function &function, const Environment &functions, std::vector<VerificationIssue> &issues)
        : function_(function), environment_(functions), issues_(issues)
    {
    }

    void Run()
    {
        CheckSymbol(function_.symbol, "VXC1006", "Core function symbol must be positive");
        CheckType(function_.returnType, "VXC1003", "Core function has an unresolved return type");
        std::unordered_set<SymbolId> parameters;
        for(const auto &parameter : function_.parameters)
        {
            CheckSymbol(parameter.symbol, "VXC1006", "Core parameter symbol must be positive");
            CheckType(parameter.type, "VXC1007", "Core parameter has an unresolved type");
            if(!parameters.insert(parameter.symbol.id).second)
                Add("VXC1004", "duplicate Core parameter symbol", parameter.symbol.id);
            environment_.insert_or_assign(parameter.symbol.id,
                                          Definition{parameter.type, false, parameter.symbol.spelling});
        }
        VerifyStatements(function_.body, environment_);
        if(function_.returnType != Type::unit() && !AlwaysReturns(function_.body))
            Add("VXC1005", "non-void Core function may complete without returning a value");
    }

private:
    const Function &function_;
    Environment environment_;
    std::vector<VerificationIssue> &issues_;

    void Add(std::string code, std::string message, SymbolId symbol = 0U)
    {
        issues_.push_back(VerificationIssue{std::move(code), std::move(message), function_.symbol.id, symbol});
    }
    void CheckSymbol(const SymbolName &symbol, std::string code, std::string message)
    {
        if(symbol.id == 0U)
            Add(std::move(code), std::move(message), symbol.id);
    }
    void CheckType(const Type &type, std::string code, std::string message)
    {
        if(ContainsInvalidType(type))
            Add(std::move(code), std::move(message));
    }
    void CheckSameType(const Type &expected, const Type &actual, std::string code, std::string message,
                       SymbolId symbol = 0U)
    {
        if(expected != actual)
            Add(std::move(code), std::move(message), symbol);
    }
    [[nodiscard]] static auto ContainsInvalidType(const Type &type) -> bool
    {
        // Core v1 has no ErrorType tag. Empty named types and malformed function
        // component lists are the native model's equivalent unresolved shapes.
        if(type.kind == Type::Kind::Named && type.name.empty())
            return true;
        if(type.kind == Type::Kind::Function && type.components.empty())
            return true;
        return std::ranges::any_of(type.components, ContainsInvalidType);
    }
    [[nodiscard]] static auto AlwaysReturns(const std::vector<Statement> &statements) -> bool
    {
        for(const auto &statement : statements)
        {
            if(statement.kind == Statement::Kind::Return)
                return true;
            if(statement.kind == Statement::Kind::If && !statement.falseBranch.empty() &&
               AlwaysReturns(statement.trueBranch) && AlwaysReturns(statement.falseBranch))
                return true;
        }
        return false;
    }
    void VerifyStatements(const std::vector<Statement> &statements, Environment &environment)
    {
        for(const auto &statement : statements)
            VerifyStatement(statement, environment);
    }
    void VerifyStatement(const Statement &statement, Environment &environment)
    {
        switch(statement.kind)
        {
        case Statement::Kind::Bind:
        {
            const auto &binding = statement.binding;
            CheckSymbol(binding.symbol, "VXC1008", "Core binding symbol must be positive");
            CheckType(binding.type, "VXC1009", "Core binding has an unresolved type");
            VerifyExpression(binding.value, environment);
            CheckSameType(binding.type, binding.value.type, "VXC1011",
                          "Core binding value type does not match its declaration", binding.symbol.id);
            if(environment.contains(binding.symbol.id))
                Add("VXC1010", "Core binding symbol is already defined", binding.symbol.id);
            environment.insert_or_assign(binding.symbol.id,
                                         Definition{binding.type, binding.mutableBinding, binding.symbol.spelling});
            return;
        }
        case Statement::Kind::Assign:
        {
            CheckSymbol(statement.destination, "VXC1015", "Core assignment symbol must be positive");
            VerifyExpression(statement.expression, environment);
            const auto found = environment.find(statement.destination.id);
            if(found == environment.end())
                Add("VXC1012", "Core assignment targets an undefined symbol", statement.destination.id);
            else if(!found->second.mutableBinding)
                Add("VXC1013", "Core assignment targets an immutable symbol", statement.destination.id);
            else
                CheckSameType(found->second.type, statement.expression.type, "VXC1014",
                              "Core assignment value has the wrong type", statement.destination.id);
            return;
        }
        case Statement::Kind::Return:
            VerifyExpression(statement.expression, environment);
            CheckSameType(function_.returnType, statement.expression.type, "VXC1016",
                          "Core return value has the wrong type");
            return;
        case Statement::Kind::If:
        {
            VerifyExpression(statement.expression, environment);
            CheckSameType(Type::boolean(), statement.expression.type, "VXC1017", "Core condition must be bool");
            auto trueEnvironment = environment;
            auto falseEnvironment = environment;
            VerifyStatements(statement.trueBranch, trueEnvironment);
            VerifyStatements(statement.falseBranch, falseEnvironment);
            return;
        }
        case Statement::Kind::Evaluate:
            VerifyExpression(statement.expression, environment);
            return;
        }
    }
    void VerifyExpression(const Expression &expression, const Environment &environment)
    {
        CheckType(expression.type, "VXC1018", "Core expression has an unresolved type");
        switch(expression.kind)
        {
        case Expression::Kind::Variable:
        {
            CheckSymbol(expression.symbol, "VXC1019", "Core variable symbol must be positive");
            const auto found = environment.find(expression.symbol.id);
            if(found == environment.end())
                Add("VXC1020", "Core expression references an undefined symbol", expression.symbol.id);
            else
            {
                CheckSameType(found->second.type, expression.type, "VXC1021",
                              "Core variable type disagrees with its definition", expression.symbol.id);
                if(!expression.symbol.spelling.empty() && !found->second.spelling.empty() &&
                   expression.symbol.spelling != found->second.spelling)
                    Add("VXC1030", "Core symbol spelling disagrees with its definition", expression.symbol.id);
            }
            return;
        }
        case Expression::Kind::Literal:
            VerifyLiteral(expression);
            return;
        case Expression::Kind::Apply:
            VerifyCall(expression, environment);
            return;
        case Expression::Kind::Primitive:
            VerifyPrimitive(expression, environment);
            return;
        }
    }
    void VerifyLiteral(const Expression &expression)
    {
        Type expected = Type::unit();
        if(std::holds_alternative<bool>(expression.literal))
            expected = Type::boolean();
        else if(std::holds_alternative<std::int64_t>(expression.literal))
            expected = Type::int64();
        else if(std::holds_alternative<std::u32string>(expression.literal))
            expected = Type::string();
        else if(std::holds_alternative<std::int32_t>(expression.literal))
            expected = Type::int32();
        CheckSameType(expected, expression.type, "VXC1029", "Core literal payload does not match its type");
    }
    void VerifyCall(const Expression &expression, const Environment &environment)
    {
        if(!expression.callee)
        {
            Add("VXC1025", "Core call target is missing");
            return;
        }
        VerifyExpression(*expression.callee, environment);
        for(const auto &argument : expression.operands)
            VerifyExpression(argument, environment);
        const auto &calleeType = expression.callee->type;
        if(calleeType.kind != Type::Kind::Function || calleeType.components.empty())
        {
            Add("VXC1025", "Core call target is not a function");
            return;
        }
        const auto parameterCount = calleeType.components.size() - 1U;
        if(parameterCount != expression.operands.size())
            Add("VXC1022", "Core call has the wrong argument count");
        const auto comparable = std::min(parameterCount, expression.operands.size());
        for(std::size_t index = 0; index < comparable; ++index)
            CheckSameType(calleeType.components[index], expression.operands[index].type, "VXC1023",
                          "Core call argument has the wrong type");
        CheckSameType(calleeType.components.back(), expression.type, "VXC1024",
                      "Core call result type disagrees with the callee");
    }
    void VerifyPrimitive(const Expression &expression, const Environment &environment)
    {
        for(const auto &operand : expression.operands)
            VerifyExpression(operand, environment);
        const auto unary = expression.primitive == Primitive::Negate || expression.primitive == Primitive::LogicalNot;
        const auto logical = expression.primitive == Primitive::LogicalAnd ||
                             expression.primitive == Primitive::LogicalOr ||
                             expression.primitive == Primitive::LogicalNot;
        const auto comparison =
            expression.primitive >= Primitive::LessThan && expression.primitive <= Primitive::NotEqual;
        if(expression.operands.size() != (unary ? 1U : 2U))
            Add("VXC1026", "Core primitive has the wrong operand count");
        const auto operandType = logical ? Type::boolean() : Type::int64();
        for(const auto &operand : expression.operands)
            CheckSameType(operandType, operand.type, "VXC1027", "Core primitive operand has the wrong type");
        const auto resultType = logical || comparison ? Type::boolean() : Type::int64();
        CheckSameType(resultType, expression.type, "VXC1028", "Core primitive result has the wrong type");
    }
};
} // namespace

auto Verify(const Module &module) -> std::vector<VerificationIssue>
{
    std::vector<VerificationIssue> issues;
    if(module.name.empty() || std::ranges::any_of(module.name, [](const auto &part) { return part.empty(); }))
        issues.push_back({"VXC1001", "Core module name must contain at least one non-empty part", 0U, 0U});

    Environment functions;
    for(const auto &function : module.functions)
    {
        if(function.symbol.id == 0U)
            issues.push_back(
                {"VXC1006", "Core function symbol must be positive", function.symbol.id, function.symbol.id});
        const auto functionType = Type::function(
            [&function]
            {
                std::vector<Type> types;
                types.reserve(function.parameters.size());
                for(const auto &parameter : function.parameters)
                    types.push_back(parameter.type);
                return types;
            }(),
            function.returnType);
        if(!functions.emplace(function.symbol.id, Definition{functionType, false, function.symbol.spelling}).second)
            issues.push_back({"VXC1002", "duplicate Core function symbol", function.symbol.id, function.symbol.id});
    }
    for(const auto &function : module.functions)
        FunctionVerifier(function, functions, issues).Run();
    return issues;
}
} // namespace Visual::XSharp::Core
