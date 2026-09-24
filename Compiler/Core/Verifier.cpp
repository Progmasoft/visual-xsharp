// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

#include <algorithm>
#include <llvm/ADT/ArrayRef.h>

#include "Visual/XSharp/ADTs/DenseIdMap.hpp"
#include "Visual/XSharp/Core/Ownership.hpp"
#include "Visual/XSharp/Core/Scalar.hpp"
#include "Visual/XSharp/Core/Template.hpp"
#include "Visual/XSharp/Core/Verifier.hpp"

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
        using Environment = ADTs::DenseIdMap<SymbolId, Definition>;

        class FunctionVerifier final
        {
        public:
            FunctionVerifier(const Function &function,
                             const Environment &functions,
                             std::vector<VerificationIssue> &issues)
                : function_(function)
                , functions_(functions)
                , issues_(issues)
            {}

            void
            Run()
            {
                CheckSymbol(function_.symbol,
                            "VXC1006",
                            "Core function symbol must be positive");
                CheckType(function_.returnType,
                          "VXC1003",
                          "Core function has an unresolved return type");
                ADTs::DenseIdSet<SymbolId> parameters;
                parameters.Reserve(function_.parameters.size());
                for (const auto &parameter : function_.parameters)
                {
                    CheckSymbol(parameter.symbol,
                                "VXC1006",
                                "Core parameter symbol must be positive");
                    CheckType(parameter.type,
                              "VXC1007",
                              "Core parameter has an unresolved type");
                    if (!parameters.Insert(parameter.symbol.id))
                        Add("VXC1004",
                            "duplicate Core parameter symbol",
                            parameter.symbol.id);
                    environment_.InsertOrAssign(
                        parameter.symbol.id,
                        Definition{ parameter.type,
                                    false,
                                    parameter.symbol.spelling });
                }
                VerifyStatements(function_.body,
                                 environment_,
                                 function_.returnType);
                if (function_.returnType != Type::unit()
                    && !AlwaysReturns(function_.body))
                    Add("VXC1005",
                        "non-void Core function may complete without returning "
                        "a value");
            }

        private:
            const Function &function_;
            const Environment &functions_;
            Environment environment_;
            std::vector<VerificationIssue> &issues_;

            [[nodiscard]] auto
            FindDefinition(const Environment &locals,
                           const SymbolId symbol) const -> const Definition *
            {
                if (const auto *found = locals.Find(symbol))
                    return found;
                return functions_.Find(symbol);
            }

            [[nodiscard]] auto
            ContainsDefinition(const Environment &locals,
                               const SymbolId symbol) const -> bool
            {
                return locals.Contains(symbol) || functions_.Contains(symbol);
            }

            void
            Add(std::string code, std::string message, SymbolId symbol = 0U)
            {
                issues_.push_back(VerificationIssue{ std::move(code),
                                                     std::move(message),
                                                     function_.symbol.id,
                                                     symbol });
            }
            void
            CheckSymbol(const SymbolName &symbol,
                        std::string code,
                        std::string message)
            {
                if (symbol.id == 0U)
                    Add(std::move(code), std::move(message), symbol.id);
            }
            void
            CheckType(const Type &type, std::string code, std::string message)
            {
                if (ContainsInvalidType(type))
                    Add(std::move(code), std::move(message));
                for (const auto &templateIssue : Template::Validate(type))
                    Add("VXC1040",
                        "invalid Core template type: " + templateIssue.message);
            }
            void
            CheckSameType(const Type &expected,
                          const Type &actual,
                          std::string code,
                          std::string message,
                          SymbolId symbol = 0U)
            {
                if (expected != actual)
                    Add(std::move(code), std::move(message), symbol);
            }
            [[nodiscard]] static auto
            ContainsInvalidType(const Type &type) -> bool
            {
                // Core v1 has no ErrorType tag. Empty named types and malformed
                // function component lists are the native model's equivalent
                // unresolved shapes.
                if (type.kind == Type::Kind::Named && type.name.empty())
                    return true;
                if (type.kind == Type::Kind::Function
                    && type.components.empty())
                    return true;
                if (std::ranges::any_of(type.components, ContainsInvalidType))
                    return true;
                return std::ranges::any_of(
                    type.templateArguments,
                    [](const auto &argument) {
                        if (argument.kind
                            == ::visual_xsharp::core::TemplateArgument::Kind::
                                Type)
                            return !argument.type
                                   || ContainsInvalidType(*argument.type);
                        if (argument.value.kind
                            == ::visual_xsharp::core::TemplateValue::Kind::
                                Parameter)
                            return argument.value.parameter.id == 0U;
                        return !::visual_xsharp::core::integer_is_canonical(
                                   argument.value.integer)
                               && argument.value.kind
                                      != ::visual_xsharp::core::TemplateValue::
                                          Kind::Boolean;
                    });
            }
            [[nodiscard]] static auto
            AlwaysReturns(const llvm::ArrayRef<Statement> statements) -> bool
            {
                for (const auto &statement : statements)
                {
                    if (statement.kind == Statement::Kind::Return)
                        return true;
                    if (statement.kind == Statement::Kind::If
                        && !statement.falseBranch.empty()
                        && AlwaysReturns(statement.trueBranch)
                        && AlwaysReturns(statement.falseBranch))
                        return true;
                }
                return false;
            }
            void
            VerifyStatements(const llvm::ArrayRef<Statement> statements,
                             Environment &environment,
                             const Type &expectedReturnType)
            {
                for (const auto &statement : statements)
                    VerifyStatement(statement, environment, expectedReturnType);
            }
            void
            VerifyStatement(const Statement &statement,
                            Environment &environment,
                            const Type &expectedReturnType)
            {
                switch (statement.kind)
                {
                    case Statement::Kind::Bind:
                    {
                        const auto &binding = statement.binding;
                        CheckSymbol(binding.symbol,
                                    "VXC1008",
                                    "Core binding symbol must be positive");
                        CheckType(binding.type,
                                  "VXC1009",
                                  "Core binding has an unresolved type");
                        VerifyExpression(binding.value, environment);
                        CheckSameType(binding.type,
                                      binding.value.type,
                                      "VXC1011",
                                      "Core binding value type does not match "
                                      "its declaration",
                                      binding.symbol.id);
                        if (ContainsDefinition(environment, binding.symbol.id))
                            Add("VXC1010",
                                "Core binding symbol is already defined",
                                binding.symbol.id);
                        environment.InsertOrAssign(
                            binding.symbol.id,
                            Definition{ binding.type,
                                        binding.mutableBinding,
                                        binding.symbol.spelling });
                        return;
                    }
                    case Statement::Kind::Assign:
                    {
                        CheckSymbol(statement.destination,
                                    "VXC1015",
                                    "Core assignment symbol must be positive");
                        VerifyExpression(statement.expression, environment);
                        const auto *found
                            = FindDefinition(environment,
                                             statement.destination.id);
                        if (found == nullptr)
                            Add("VXC1012",
                                "Core assignment targets an undefined symbol",
                                statement.destination.id);
                        else if (!found->mutableBinding)
                            Add("VXC1013",
                                "Core assignment targets an immutable symbol",
                                statement.destination.id);
                        else
                            CheckSameType(
                                found->type,
                                statement.expression.type,
                                "VXC1014",
                                "Core assignment value has the wrong type",
                                statement.destination.id);
                        return;
                    }
                    case Statement::Kind::Return:
                        VerifyExpression(statement.expression, environment);
                        CheckSameType(expectedReturnType,
                                      statement.expression.type,
                                      "VXC1016",
                                      "Core return value has the wrong type");
                        return;
                    case Statement::Kind::If:
                    {
                        VerifyExpression(statement.expression, environment);
                        if (!accepts_boolean_context(statement.expression.type))
                            Add("VXC1017",
                                "Core condition must be bool or numeric");
                        auto trueEnvironment = environment;
                        auto falseEnvironment = environment;
                        VerifyStatements(statement.trueBranch,
                                         trueEnvironment,
                                         expectedReturnType);
                        VerifyStatements(statement.falseBranch,
                                         falseEnvironment,
                                         expectedReturnType);
                        return;
                    }
                    case Statement::Kind::Evaluate:
                        VerifyExpression(statement.expression, environment);
                        return;
                }
            }
            void
            VerifyExpression(const Expression &expression,
                             const Environment &environment)
            {
                CheckType(expression.type,
                          "VXC1018",
                          "Core expression has an unresolved type");
                switch (expression.kind)
                {
                    case Expression::Kind::Variable:
                    {
                        CheckSymbol(expression.symbol,
                                    "VXC1019",
                                    "Core variable symbol must be positive");
                        const auto *found
                            = FindDefinition(environment, expression.symbol.id);
                        if (found == nullptr)
                            Add("VXC1020",
                                "Core expression references an undefined "
                                "symbol",
                                expression.symbol.id);
                        else
                        {
                            CheckSameType(found->type,
                                          expression.type,
                                          "VXC1021",
                                          "Core variable type disagrees with "
                                          "its definition",
                                          expression.symbol.id);
                            if (!expression.symbol.spelling.empty()
                                && !found->spelling.empty()
                                && expression.symbol.spelling
                                       != found->spelling)
                                Add("VXC1030",
                                    "Core symbol spelling disagrees with its "
                                    "definition",
                                    expression.symbol.id);
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
                    case Expression::Kind::Closure:
                        VerifyClosure(expression, environment);
                        return;
                    case Expression::Kind::Let:
                    {
                        CheckSymbol(expression.letSymbol,
                                    "VXC1045",
                                    "Core let symbol must be positive");
                        CheckType(expression.letType,
                                  "VXC1046",
                                  "Core let binding has an unresolved type");
                        if (!expression.letValue || !expression.letBody)
                        {
                            Add("VXC1047", "Core let value or body is missing");
                            return;
                        }
                        VerifyExpression(*expression.letValue, environment);
                        CheckSameType(expression.letType,
                                      expression.letValue->type,
                                      "VXC1048",
                                      "Core let value has the wrong type");
                        auto bodyEnvironment = environment;
                        bodyEnvironment.InsertOrAssign(
                            expression.letSymbol.id,
                            Definition{ expression.letType,
                                        false,
                                        expression.letSymbol.spelling });
                        VerifyExpression(*expression.letBody, bodyEnvironment);
                        CheckSameType(
                            expression.type,
                            expression.letBody->type,
                            "VXC1049",
                            "Core let result disagrees with its body");
                        return;
                    }
                }
            }
            void
            VerifyLiteral(const Expression &expression)
            {
                if (const auto issue
                    = validate_literal(expression.literal, expression.type))
                    Add("VXC1029",
                        "Core literal payload does not match its type: "
                            + *issue);
            }
            void
            VerifyCall(const Expression &expression,
                       const Environment &environment)
            {
                if (!expression.callee)
                {
                    Add("VXC1025", "Core call target is missing");
                    return;
                }
                VerifyExpression(*expression.callee, environment);
                for (const auto &argument : expression.operands)
                    VerifyExpression(argument, environment);
                const auto &calleeType = expression.callee->type;
                if (calleeType.kind != Type::Kind::Function
                    || calleeType.components.empty())
                {
                    Add("VXC1025", "Core call target is not a function");
                    return;
                }
                const auto parameterCount = calleeType.components.size() - 1U;
                if (parameterCount != expression.operands.size())
                    Add("VXC1022", "Core call has the wrong argument count");
                const auto comparable
                    = std::min(parameterCount, expression.operands.size());
                for (std::size_t index = 0; index < comparable; ++index)
                    CheckSameType(calleeType.components[index],
                                  expression.operands[index].type,
                                  "VXC1023",
                                  "Core call argument has the wrong type");
                CheckSameType(
                    calleeType.components.back(),
                    expression.type,
                    "VXC1024",
                    "Core call result type disagrees with the callee");
            }
            void
            VerifyPrimitive(const Expression &expression,
                            const Environment &environment)
            {
                for (const auto &operand : expression.operands)
                    VerifyExpression(operand, environment);
                const auto unary
                    = expression.primitive == Primitive::Negate
                      || expression.primitive == Primitive::LogicalNot
                      || expression.primitive == Primitive::BitwiseNot;
                const auto logical
                    = expression.primitive == Primitive::LogicalAnd
                      || expression.primitive == Primitive::LogicalOr
                      || expression.primitive == Primitive::LogicalNot;
                const auto integerOnly
                    = expression.primitive == Primitive::ShiftLeft
                      || expression.primitive == Primitive::ShiftRight
                      || expression.primitive == Primitive::BitwiseAnd
                      || expression.primitive == Primitive::BitwiseXor
                      || expression.primitive == Primitive::BitwiseOr
                      || expression.primitive == Primitive::BitwiseNot;
                const auto comparison
                    = expression.primitive >= Primitive::LessThan
                      && expression.primitive <= Primitive::NotEqual;
                if (expression.operands.size() != (unary ? 1U : 2U))
                    Add("VXC1026",
                        "Core primitive has the wrong operand count");
                if (expression.operands.empty())
                    return;
                const auto &operandType = expression.operands.front().type;
                const auto typeTest = expression.primitive == Primitive::TypeIs;
                if (!logical && !typeTest)
                    for (const auto &operand : expression.operands)
                        CheckSameType(
                            operandType,
                            operand.type,
                            "VXC1027",
                            "Core primitive operands must have matching types");

                if (typeTest)
                {
                    if (expression.operands.size() == 2U)
                    {
                        const auto &subjectType = expression.operands[0].type;
                        const auto &identityType = expression.operands[1].type;
                        const auto referenceSubject
                            = subjectType.kind == Type::Kind::Named
                              || subjectType.kind == Type::Kind::String
                              || subjectType.kind == Type::Kind::Function;
                        if (!referenceSubject || identityType != Type::uint64())
                            Add("VXC1050",
                                "Core type test requires a reference subject "
                                "and uint identity");
                    }
                }
                else if (logical)
                {
                    for (const auto &operand : expression.operands)
                        if (!accepts_boolean_context(operand.type))
                            Add("VXC1027",
                                "Core logical primitive requires bool or "
                                "numeric operands");
                }
                else if (integerOnly && !is_integer(operandType))
                    Add("VXC1027",
                        "Core bitwise primitive requires integer operands");
                else if (!is_numeric(operandType)
                         && expression.primitive != Primitive::Equal
                         && expression.primitive != Primitive::NotEqual)
                    Add("VXC1027",
                        "Core arithmetic or ordering primitive requires "
                        "numeric operands");

                if (expression.primitive == Primitive::Negate
                    && !is_signed_integer(operandType)
                    && !is_floating(operandType))
                    Add("VXC1027",
                        "Core negation requires a signed integer or floating "
                        "operand");

                const auto expectedResult
                    = logical || comparison || typeTest ? Type::boolean()
                      : expression.primitive == Primitive::FloorDivide
                              && is_floating(operandType)
                          ? Type::int64()
                          : operandType;
                CheckSameType(expectedResult,
                              expression.type,
                              "VXC1028",
                              "Core primitive result has the wrong type");
            }
            void
            VerifyClosure(const Expression &expression,
                          const Environment &outerEnvironment)
            {
                if (!expression.closureBody)
                {
                    Add("VXC1031", "Core closure body is missing");
                    return;
                }

                CheckType(expression.closureReturnType,
                          "VXC1032",
                          "Core closure has an unresolved return type");
                Environment closureEnvironment = outerEnvironment;
                ADTs::DenseIdSet<SymbolId> localSymbols;
                localSymbols.Reserve(expression.captures.size()
                                     + expression.closureParameters.size());

                for (const auto &capture : expression.captures)
                {
                    CheckSymbol(capture.symbol,
                                "VXC1033",
                                "Core closure capture symbol must be positive");
                    CheckType(capture.type,
                              "VXC1034",
                              "Core closure capture has an unresolved type");
                    if (capture.mode != CaptureMode::Strong
                        && capture.mode != CaptureMode::Weak
                        && capture.mode != CaptureMode::Unowned)
                        Add("VXC1043",
                            "Core closure capture has an invalid ownership "
                            "mode",
                            capture.symbol.id);
                    if (capture.mode != CaptureMode::Strong
                        && !UsesAarc(capture.type)
                        && capture.type.kind != Type::Kind::Named)
                        Add("VXC1044",
                            "weak or unowned Core capture requires an AARC "
                            "reference value",
                            capture.symbol.id);
                    if (!capture.value)
                    {
                        Add("VXC1035",
                            "Core closure capture value is missing",
                            capture.symbol.id);
                        continue;
                    }
                    VerifyExpression(*capture.value, outerEnvironment);
                    CheckSameType(capture.type,
                                  capture.value->type,
                                  "VXC1036",
                                  "Core closure capture value type does not "
                                  "match its binding",
                                  capture.symbol.id);
                    if (!localSymbols.Insert(capture.symbol.id))
                        Add("VXC1037",
                            "duplicate Core closure local symbol",
                            capture.symbol.id);
                    closureEnvironment.InsertOrAssign(
                        capture.symbol.id,
                        // Captured storage is addressable inside the callable.
                        // Haskell Core verification uses the same mutability
                        // rule; rejecting assignment here split the two VXCR
                        // consumers for otherwise identical closure bodies.
                        Definition{ capture.type,
                                    true,
                                    capture.symbol.spelling });
                }

                std::vector<Type> parameterTypes;
                parameterTypes.reserve(expression.closureParameters.size());
                for (const auto &[symbol, type] : expression.closureParameters)
                {
                    CheckSymbol(
                        symbol,
                        "VXC1038",
                        "Core closure parameter symbol must be positive");
                    CheckType(type,
                              "VXC1039",
                              "Core closure parameter has an unresolved type");
                    parameterTypes.push_back(type);
                    if (!localSymbols.Insert(symbol.id))
                        Add("VXC1037",
                            "duplicate Core closure local symbol",
                            symbol.id);
                    closureEnvironment.InsertOrAssign(
                        symbol.id,
                        Definition{ type, false, symbol.spelling });
                }

                const auto expectedType
                    = Type::function(parameterTypes,
                                     expression.closureReturnType);
                CheckSameType(expectedType,
                              expression.type,
                              "VXC1041",
                              "Core closure value type disagrees with its "
                              "parameter and return types");
                VerifyStatements(*expression.closureBody,
                                 closureEnvironment,
                                 expression.closureReturnType);
                if (expression.closureReturnType != Type::unit()
                    && !AlwaysReturns(*expression.closureBody))
                    Add("VXC1042",
                        "non-void Core closure may complete without returning "
                        "a value");
            }
        };
    } // namespace

    auto
    Verify(const Module &module) -> std::vector<VerificationIssue>
    {
        std::vector<VerificationIssue> issues;
        if (module.name.empty()
            || std::ranges::any_of(module.name, [](const auto &part) {
                   return part.empty();
               }))
            issues.push_back(
                { "VXC1001",
                  "Core module name must contain at least one non-empty part",
                  0U,
                  0U });

        Environment functions;
        functions.Reserve(module.functions.size());
        for (const auto &function : module.functions)
        {
            if (function.symbol.id == 0U)
                issues.push_back({ "VXC1006",
                                   "Core function symbol must be positive",
                                   function.symbol.id,
                                   function.symbol.id });
            const auto functionType = Type::function(
                [&function] {
                    std::vector<Type> types;
                    types.reserve(function.parameters.size());
                    for (const auto &parameter : function.parameters)
                        types.push_back(parameter.type);
                    return types;
                }(),
                function.returnType);
            if (!functions
                     .TryEmplace(function.symbol.id,
                                 Definition{ functionType,
                                             false,
                                             function.symbol.spelling })
                     .inserted)
                issues.push_back({ "VXC1002",
                                   "duplicate Core function symbol",
                                   function.symbol.id,
                                   function.symbol.id });
        }
        for (const auto &function : module.functions)
            FunctionVerifier(function, functions, issues).Run();
        return issues;
    }
} // namespace Visual::XSharp::Core
