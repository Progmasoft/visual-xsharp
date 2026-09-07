// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

#include <algorithm>
#include <cstdlib>
#include <utility>

#include "Visual/XSharp/Core/CorePrep/Prepare.hpp"
#include "Visual/XSharp/Core/Scalar.hpp"

namespace Visual::XSharp::Core::CorePrep
{
    namespace
    {
        namespace Prepared = ::visual_xsharp::core;

        struct State final
        {
            SymbolId nextTemporary{ 1U };
            Prepared::BlockId nextBlock{ 1U };
            SymbolId nextFunction{ 1U };
            std::vector<Function> pendingFunctions;
        };

        struct Atomized final
        {
            std::vector<Prepared::Instruction> prefix;
            Prepared::Atom atom;
            State state;
        };

        struct OperationResult final
        {
            std::vector<Prepared::Instruction> prefix;
            Prepared::Operation operation{ Prepared::Operation::Copy };
            std::vector<Prepared::Atom> operands;
            SymbolName closureFunction{};
            std::vector<Prepared::Capture> captures;
            State state;
        };

        struct BlocksResult final
        {
            std::vector<Prepared::Block> blocks;
            State state;
        };

        [[nodiscard]] auto
        LowerLiteral(const Expression &expression) -> Prepared::Atom
        {
            return Prepared::Atom::constant(expression.literal, expression.type);
        }

        [[nodiscard]] auto
        LowerPrimitive(Primitive primitive) -> Prepared::Operation
        {
            // Both enums intentionally follow the VXCR/VXCP stable operation order. Keep the
            // explicit switch so a future enum edit cannot silently alter the wire contract.
            switch (primitive)
            {
                case Primitive::Add:
                    return Prepared::Operation::Add;
                case Primitive::Subtract:
                    return Prepared::Operation::Subtract;
                case Primitive::Multiply:
                    return Prepared::Operation::Multiply;
                case Primitive::Divide:
                    return Prepared::Operation::Divide;
                case Primitive::FloorDivide:
                    return Prepared::Operation::FloorDivide;
                case Primitive::Remainder:
                    return Prepared::Operation::Remainder;
                case Primitive::LessThan:
                    return Prepared::Operation::LessThan;
                case Primitive::LessEqual:
                    return Prepared::Operation::LessEqual;
                case Primitive::GreaterThan:
                    return Prepared::Operation::GreaterThan;
                case Primitive::GreaterEqual:
                    return Prepared::Operation::GreaterEqual;
                case Primitive::Equal:
                    return Prepared::Operation::Equal;
                case Primitive::NotEqual:
                    return Prepared::Operation::NotEqual;
                case Primitive::LogicalAnd:
                    return Prepared::Operation::LogicalAnd;
                case Primitive::LogicalOr:
                    return Prepared::Operation::LogicalOr;
                case Primitive::Negate:
                    return Prepared::Operation::Negate;
                case Primitive::LogicalNot:
                    return Prepared::Operation::LogicalNot;
            }
            // Reaching this point means the Core enum and adapter diverged.
            // C++20 has no std::unreachable; abort explicitly instead of
            // silently translating an unknown primitive to Copy.
            std::abort();
        }

        [[nodiscard]] auto
        Atomize(State state, const Expression &expression) -> Atomized;

        [[nodiscard]] auto
        AtomizeMany(State state, const std::vector<Expression> &expressions)
            -> std::pair<std::vector<Prepared::Instruction>, std::pair<std::vector<Prepared::Atom>, State>>
        {
            std::vector<Prepared::Instruction> prefix;
            std::vector<Prepared::Atom> atoms;
            atoms.reserve(expressions.size());
            for (const auto &expression : expressions)
            {
                auto atomized = Atomize(state, expression);
                prefix.insert(prefix.end(), std::make_move_iterator(atomized.prefix.begin()), std::make_move_iterator(atomized.prefix.end()));
                atoms.push_back(std::move(atomized.atom));
                state = atomized.state;
            }
            return { std::move(prefix), { std::move(atoms), state } };
        }

        [[nodiscard]] auto
        ZeroForBooleanContext(const Type &type) -> Prepared::Atom
        {
            if (Prepared::is_floating(type))
                return Prepared::Atom::constant(Prepared::FloatingLiteral{ "0" }, type);
            return Prepared::Atom::constant(Prepared::integer_from_signed(0), type);
        }

        [[nodiscard]] auto
        Booleanize(State state, Prepared::Atom atom) -> Atomized
        {
            if (atom.type == Type::boolean())
                return { {}, std::move(atom), std::move(state) };
            const auto id = state.nextTemporary++;
            const auto digits = std::to_string(id);
            std::u32string spelling = U"$condition";
            spelling.append(digits.begin(), digits.end());
            auto temporary = SymbolName{ id, std::move(spelling) };
            std::vector<Prepared::Atom> operands;
            operands.push_back(std::move(atom));
            operands.push_back(ZeroForBooleanContext(operands.front().type));
            Prepared::Instruction comparison{
                Prepared::Instruction::Kind::Bind,
                temporary,
                Type::boolean(),
                false,
                Prepared::Operation::NotEqual,
                std::move(operands),
                {},
                {},
            };
            return {
                { std::move(comparison) },
                Prepared::Atom::variable(std::move(temporary), Type::boolean()),
                std::move(state),
            };
        }

        [[nodiscard]] auto
        BooleanizeMany(State state, std::vector<Prepared::Atom> atoms)
            -> std::pair<std::vector<Prepared::Instruction>, std::pair<std::vector<Prepared::Atom>, State>>
        {
            std::vector<Prepared::Instruction> prefix;
            std::vector<Prepared::Atom> booleans;
            booleans.reserve(atoms.size());
            for (auto &atom : atoms)
            {
                auto boolean = Booleanize(std::move(state), std::move(atom));
                prefix.insert(prefix.end(), std::make_move_iterator(boolean.prefix.begin()), std::make_move_iterator(boolean.prefix.end()));
                booleans.push_back(std::move(boolean.atom));
                state = std::move(boolean.state);
            }
            return { std::move(prefix), { std::move(booleans), std::move(state) } };
        }

        struct CapturesResult final
        {
            std::vector<Prepared::Instruction> prefix;
            std::vector<Prepared::Capture> captures;
            State state;
        };

        struct PreparedFunctionResult final
        {
            Prepared::Function function;
            std::vector<Function> pendingFunctions;
            SymbolId nextFunction{};
        };

        [[nodiscard]] auto
        HighestExpressionSymbol(const Expression &expression) -> SymbolId;

        [[nodiscard]] auto
        HighestStatementSymbol(const Statement &statement) -> SymbolId
        {
            SymbolId highest = 0U;
            if (statement.kind == Statement::Kind::Bind)
            {
                highest = statement.binding.symbol.id;
                highest = std::max(highest, HighestExpressionSymbol(statement.binding.value));
            }
            else
            {
                if (statement.kind == Statement::Kind::Assign)
                    highest = statement.destination.id;
                highest = std::max(highest, HighestExpressionSymbol(statement.expression));
            }
            for (const auto &nested : statement.trueBranch)
                highest = std::max(highest, HighestStatementSymbol(nested));
            for (const auto &nested : statement.falseBranch)
                highest = std::max(highest, HighestStatementSymbol(nested));
            return highest;
        }

        [[nodiscard]] auto
        HighestExpressionSymbol(const Expression &expression) -> SymbolId
        {
            SymbolId highest = expression.kind == Expression::Kind::Variable ? expression.symbol.id : 0U;
            if (expression.callee)
                highest = std::max(highest, HighestExpressionSymbol(*expression.callee));
            for (const auto &operand : expression.operands)
                highest = std::max(highest, HighestExpressionSymbol(operand));
            for (const auto &capture : expression.captures)
            {
                highest = std::max(highest, capture.symbol.id);
                if (capture.value)
                    highest = std::max(highest, HighestExpressionSymbol(*capture.value));
            }
            for (const auto &[parameter, type] : expression.closureParameters)
            {
                static_cast<void>(type);
                highest = std::max(highest, parameter.id);
            }
            if (expression.closureBody)
                for (const auto &statement : *expression.closureBody)
                    highest = std::max(highest, HighestStatementSymbol(statement));
            return highest;
        }

        [[nodiscard]] auto
        HighestFunctionSymbol(const Function &function) -> SymbolId
        {
            SymbolId highest = function.symbol.id;
            for (const auto &parameter : function.parameters)
                highest = std::max(highest, parameter.symbol.id);
            for (const auto &statement : function.body)
                highest = std::max(highest, HighestStatementSymbol(statement));
            return highest;
        }

        [[nodiscard]] auto
        AtomizeCaptures(State state, const std::vector<Capture> &captures) -> CapturesResult
        {
            std::vector<Prepared::Instruction> prefix;
            std::vector<Prepared::Capture> prepared;
            prepared.reserve(captures.size());
            for (const auto &capture : captures)
            {
                // Core verification requires the value, but keep this total
                // for direct native API callers as well.
                if (!capture.value)
                    std::abort();
                auto value = Atomize(state, *capture.value);
                prefix.insert(
                    prefix.end(),
                    std::make_move_iterator(value.prefix.begin()),
                    std::make_move_iterator(value.prefix.end()));
                prepared.push_back(Prepared::Capture{
                    capture.mode,
                    capture.symbol,
                    capture.type,
                    std::move(value.atom),
                });
                state = std::move(value.state);
            }
            return { std::move(prefix), std::move(prepared), std::move(state) };
        }

        [[nodiscard]] auto
        AtomizeOperation(State state, const Expression &expression) -> OperationResult
        {
            if (expression.kind == Expression::Kind::Variable)
                return { {}, Prepared::Operation::Copy, { Prepared::Atom::variable(expression.symbol, expression.type) }, {}, {}, state };
            if (expression.kind == Expression::Kind::Literal)
                return { {}, Prepared::Operation::Copy, { LowerLiteral(expression) }, {}, {}, state };
            if (expression.kind == Expression::Kind::Apply)
            {
                auto callee = Atomize(state, *expression.callee);
                auto arguments = AtomizeMany(callee.state, expression.operands);
                callee.prefix.insert(callee.prefix.end(), std::make_move_iterator(arguments.first.begin()), std::make_move_iterator(arguments.first.end()));
                std::vector<Prepared::Atom> operands;
                operands.reserve(arguments.second.first.size() + 1U);
                operands.push_back(std::move(callee.atom));
                operands.insert(operands.end(), std::make_move_iterator(arguments.second.first.begin()), std::make_move_iterator(arguments.second.first.end()));
                return { std::move(callee.prefix), Prepared::Operation::Call, std::move(operands), {}, {}, arguments.second.second };
            }
            if (expression.kind == Expression::Kind::Closure)
            {
                if (!expression.closureBody)
                    std::abort();
                const auto closureId = state.nextFunction++;
                const auto digits = std::to_string(closureId);
                std::u32string spelling = U"$closure";
                spelling.append(digits.begin(), digits.end());
                SymbolName closureName{ closureId, std::move(spelling) };

                auto captures = AtomizeCaptures(std::move(state), expression.captures);
                std::vector<Parameter> parameters;
                parameters.reserve(expression.captures.size() + expression.closureParameters.size());
                for (const auto &capture : expression.captures)
                    parameters.push_back(Parameter{ capture.symbol, capture.type });
                for (const auto &[symbol, type] : expression.closureParameters)
                    parameters.push_back(Parameter{ symbol, type });
                captures.state.pendingFunctions.push_back(Function{
                    closureName,
                    std::move(parameters),
                    expression.closureReturnType,
                    *expression.closureBody,
                });
                return {
                    std::move(captures.prefix),
                    Prepared::Operation::MakeClosure,
                    {},
                    std::move(closureName),
                    std::move(captures.captures),
                    std::move(captures.state),
                };
            }
            auto arguments = AtomizeMany(state, expression.operands);
            if (expression.primitive == Primitive::LogicalAnd
                || expression.primitive == Primitive::LogicalOr
                || expression.primitive == Primitive::LogicalNot)
            {
                auto booleans = BooleanizeMany(std::move(arguments.second.second), std::move(arguments.second.first));
                arguments.first.insert(arguments.first.end(), std::make_move_iterator(booleans.first.begin()), std::make_move_iterator(booleans.first.end()));
                return { std::move(arguments.first),
                         LowerPrimitive(expression.primitive),
                         std::move(booleans.second.first),
                         {},
                         {},
                         std::move(booleans.second.second) };
            }
            return { std::move(arguments.first), LowerPrimitive(expression.primitive), std::move(arguments.second.first), {}, {}, arguments.second.second };
        }

        [[nodiscard]] auto
        Atomize(State state, const Expression &expression) -> Atomized
        {
            if (expression.kind == Expression::Kind::Variable)
                return { {}, Prepared::Atom::variable(expression.symbol, expression.type), state };
            if (expression.kind == Expression::Kind::Literal)
                return { {}, LowerLiteral(expression), state };

            auto operation = AtomizeOperation(state, expression);
            const auto id = operation.state.nextTemporary++;
            const auto digits = std::to_string(id);
            std::u32string spelling = U"$coreprep";
            spelling.append(digits.begin(), digits.end());
            auto temporary = SymbolName{ id, std::move(spelling) };
            Prepared::Instruction binding{
                Prepared::Instruction::Kind::Bind,
                temporary,
                expression.type,
                false,
                operation.operation,
                std::move(operation.operands),
                std::move(operation.closureFunction),
                std::move(operation.captures)
            };
            operation.prefix.push_back(std::move(binding));
            return { std::move(operation.prefix), Prepared::Atom::variable(std::move(temporary), expression.type), operation.state };
        }

        [[nodiscard]] auto
        PrepareStatements(State state, Prepared::BlockId blockId, std::vector<Prepared::Instruction> instructions, const std::vector<Statement> &statements, std::size_t start = 0U) -> BlocksResult;

        [[nodiscard]] auto
        PrepareBranch(State state, Prepared::BlockId blockId, Prepared::BlockId joinId, const std::vector<Statement> &statements) -> BlocksResult
        {
            auto result = PrepareStatements(state, blockId, {}, statements);
            for (auto &block : result.blocks)
            {
                if (block.terminator.kind == Prepared::Terminator::Kind::Unreachable)
                {
                    block.terminator.kind = Prepared::Terminator::Kind::Jump;
                    block.terminator.true_target = joinId;
                }
            }
            return result;
        }

        [[nodiscard]] auto
        PrepareStatements(State state, Prepared::BlockId blockId, std::vector<Prepared::Instruction> instructions, const std::vector<Statement> &statements, std::size_t start) -> BlocksResult
        {
            for (std::size_t index = start; index < statements.size(); ++index)
            {
                const auto &statement = statements[index];
                if (statement.kind == Statement::Kind::Bind)
                {
                    auto operation = AtomizeOperation(state, statement.binding.value);
                    instructions.insert(instructions.end(), std::make_move_iterator(operation.prefix.begin()), std::make_move_iterator(operation.prefix.end()));
                    instructions.push_back(Prepared::Instruction{ Prepared::Instruction::Kind::Bind,
                                                                  statement.binding.symbol,
                                                                  statement.binding.type,
                                                                  statement.binding.mutableBinding,
                                                                  operation.operation,
                                                                  std::move(operation.operands),
                                                                  std::move(operation.closureFunction),
                                                                  std::move(operation.captures) });
                    state = operation.state;
                    continue;
                }
                if (statement.kind == Statement::Kind::Assign)
                {
                    auto value = Atomize(state, statement.expression);
                    instructions.insert(instructions.end(), std::make_move_iterator(value.prefix.begin()), std::make_move_iterator(value.prefix.end()));
                    instructions.push_back(Prepared::Instruction{ Prepared::Instruction::Kind::Assign,
                                                                  statement.destination,
                                                                  statement.expression.type,
                                                                  false,
                                                                  Prepared::Operation::Copy,
                                                                  { std::move(value.atom) },
                                                                  {},
                                                                  {} });
                    state = value.state;
                    continue;
                }
                if (statement.kind == Statement::Kind::Evaluate)
                {
                    auto operation = AtomizeOperation(state, statement.expression);
                    instructions.insert(instructions.end(), std::make_move_iterator(operation.prefix.begin()), std::make_move_iterator(operation.prefix.end()));
                    instructions.push_back(Prepared::Instruction{ Prepared::Instruction::Kind::Evaluate,
                                                                  {},
                                                                  statement.expression.type,
                                                                  false,
                                                                  operation.operation,
                                                                  std::move(operation.operands),
                                                                  std::move(operation.closureFunction),
                                                                  std::move(operation.captures) });
                    state = operation.state;
                    continue;
                }
                if (statement.kind == Statement::Kind::Return)
                {
                    auto value = Atomize(state, statement.expression);
                    instructions.insert(instructions.end(), std::make_move_iterator(value.prefix.begin()), std::make_move_iterator(value.prefix.end()));
                    return { { { blockId,
                                 std::move(instructions),
                                 { Prepared::Terminator::Kind::Return, std::move(value.atom), 0U, 0U } } },
                             value.state };
                }

                auto condition = Atomize(state, statement.expression);
                auto boolean = Booleanize(std::move(condition.state), std::move(condition.atom));
                instructions.insert(instructions.end(), std::make_move_iterator(condition.prefix.begin()), std::make_move_iterator(condition.prefix.end()));
                instructions.insert(instructions.end(), std::make_move_iterator(boolean.prefix.begin()), std::make_move_iterator(boolean.prefix.end()));
                const auto trueId = boolean.state.nextBlock;
                const auto falseId = trueId + 1U;
                const auto joinId = falseId + 1U;
                boolean.state.nextBlock = joinId + 1U;
                auto trueBlocks = PrepareBranch(boolean.state, trueId, joinId, statement.trueBranch);
                auto falseBlocks = PrepareBranch(trueBlocks.state, falseId, joinId, statement.falseBranch);
                auto tail = PrepareStatements(falseBlocks.state, joinId, {}, statements, index + 1U);
                std::vector<Prepared::Block> blocks;
                blocks.push_back({ blockId,
                                   std::move(instructions),
                                   { Prepared::Terminator::Kind::Branch, std::move(boolean.atom), trueId, falseId } });
                blocks.insert(blocks.end(), std::make_move_iterator(trueBlocks.blocks.begin()), std::make_move_iterator(trueBlocks.blocks.end()));
                blocks.insert(blocks.end(), std::make_move_iterator(falseBlocks.blocks.begin()), std::make_move_iterator(falseBlocks.blocks.end()));
                blocks.insert(blocks.end(), std::make_move_iterator(tail.blocks.begin()), std::make_move_iterator(tail.blocks.end()));
                return { std::move(blocks), tail.state };
            }
            return { { { blockId, std::move(instructions), { Prepared::Terminator::Kind::Unreachable, {}, 0U, 0U } } }, state };
        }

        [[nodiscard]] auto
        PrepareFunction(const Function &function, SymbolId nextFunction) -> PreparedFunctionResult
        {
            SymbolId highest = HighestFunctionSymbol(function);
            std::vector<Prepared::Parameter> parameters;
            parameters.reserve(function.parameters.size());
            for (const auto &parameter : function.parameters)
            {
                parameters.push_back({ parameter.symbol, parameter.type });
            }
            auto body = PrepareStatements(State{ highest + 1U, 1U, nextFunction, {} }, 0U, {}, function.body);
            return {
                Prepared::Function{ function.symbol, std::move(parameters), function.returnType, 0U, std::move(body.blocks) },
                std::move(body.state.pendingFunctions),
                body.state.nextFunction,
            };
        }
    } // namespace

    auto
    Prepare(const Module &module) -> ::visual_xsharp::core::CorePrepModule
    {
        ::visual_xsharp::core::CorePrepModule prepared{ module.name, {} };
        std::vector<Function> pending = module.functions;
        SymbolId nextFunction = 1U;
        for (const auto &function : pending)
            nextFunction = std::max(nextFunction, HighestFunctionSymbol(function) + 1U);

        // Closure bodies are lifted as ordinary Core functions and fed back
        // through the same work queue. This naturally handles nested closures
        // without adding a second, subtly different lowering implementation.
        for (std::size_t index = 0U; index < pending.size(); ++index)
        {
            auto result = PrepareFunction(pending[index], nextFunction);
            prepared.functions.push_back(std::move(result.function));
            nextFunction = result.nextFunction;
            pending.insert(
                pending.end(),
                std::make_move_iterator(result.pendingFunctions.begin()),
                std::make_move_iterator(result.pendingFunctions.end()));
        }
        return prepared;
    }
} // namespace Visual::XSharp::Core::CorePrep
