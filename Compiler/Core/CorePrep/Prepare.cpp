// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

#include <algorithm>
#include <cstdlib>
#include <deque>
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
            // One counter names every generated symbol of the module:
            // temporaries, condition and short-circuit slots, and lifted
            // closure functions. It starts above every identity in the
            // whole module and is never reset between functions, because
            // identities are module-wide and a per-function start would
            // reuse another function's symbols.
            SymbolId nextSymbol{ 1U };
            Prepared::BlockId nextBlock{ 1U };
            std::vector<Function> pendingFunctions;
            // Innermost first; every pair stores (break exit, continue target).
            std::vector<std::pair<Prepared::BlockId, Prepared::BlockId>>
                loopTargets;
        };

        /// Lower a Core literal to a CorePrep atom.
        ///
        /// Core still carries fixed-width integer payloads. CorePrep has one
        /// canonical integer form, the sign and magnitude that its wire
        /// reader produces, so the adapter converts here. Without this the
        /// same module compares unequal before and after a wire round trip.
        [[nodiscard]] auto
        LowerLiteral(const Expression &expression) -> Prepared::Atom
        {
            if (const auto *wide
                = std::get_if<std::int64_t>(&expression.literal))
                return Prepared::Atom::constant(
                    Prepared::integer_from_signed(*wide),
                    expression.type);
            if (const auto *narrow
                = std::get_if<std::int32_t>(&expression.literal))
                return Prepared::Atom::constant(
                    Prepared::integer_from_signed(*narrow),
                    expression.type);
            return Prepared::Atom::constant(expression.literal,
                                            expression.type);
        }

        [[nodiscard]] auto
        LowerPrimitive(Primitive primitive) -> Prepared::Operation
        {
            // Both enums intentionally follow the VXCR/VXCP stable operation
            // order. Keep the explicit switch so a future enum edit cannot
            // silently alter the wire contract.
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
                case Primitive::Power:
                    return Prepared::Operation::Power;
                case Primitive::ShiftLeft:
                    return Prepared::Operation::ShiftLeft;
                case Primitive::ShiftRight:
                    return Prepared::Operation::ShiftRight;
                case Primitive::BitwiseAnd:
                    return Prepared::Operation::BitwiseAnd;
                case Primitive::BitwiseXor:
                    return Prepared::Operation::BitwiseXor;
                case Primitive::BitwiseOr:
                    return Prepared::Operation::BitwiseOr;
                case Primitive::BitwiseNot:
                    return Prepared::Operation::BitwiseNot;
                case Primitive::TypeIs:
                    return Prepared::Operation::TypeIs;
            }
            // Reaching this point means the Core enum and adapter diverged.
            // C++20 has no std::unreachable; abort explicitly instead of
            // silently translating an unknown primitive to Copy.
            std::abort();
        }

        /**
         * @brief The block receiving instructions plus every finished block.
         *
         * Expression atomization is not confined to one block: a
         * short-circuit operator ends the current block with a branch and
         * continues in a join block. Threading one cursor through both
         * expressions and statements lets either of them close and reopen
         * blocks without a second lowering path.
         */
        struct Cursor final
        {
            State state;
            Prepared::BlockId block{};
            std::vector<Prepared::Instruction> instructions;
            std::vector<Prepared::Block> closed;
            /// False after a return, break or continue ended the region.
            bool open{ true };

            void
            Emit(Prepared::Instruction instruction)
            {
                instructions.push_back(std::move(instruction));
            }

            /// End the current block; no block is open until Open is called.
            void
            Close(Prepared::Terminator terminator)
            {
                closed.push_back(
                    { block, std::move(instructions), std::move(terminator) });
                instructions.clear();
                open = false;
            }

            void
            Open(const Prepared::BlockId id)
            {
                block = id;
                instructions.clear();
                open = true;
            }

            void
            Jump(const Prepared::BlockId target)
            {
                Close({ Prepared::Terminator::Kind::Jump, {}, target, 0U });
            }

            void
            Branch(Prepared::Atom condition,
                   const Prepared::BlockId whenTrue,
                   const Prepared::BlockId whenFalse)
            {
                Close({ Prepared::Terminator::Kind::Branch,
                        std::move(condition),
                        whenTrue,
                        whenFalse });
            }

            [[nodiscard]] auto
            Temporary(std::u32string prefix) -> SymbolName
            {
                const auto id = state.nextSymbol++;
                const auto digits = std::to_string(id);
                prefix.append(digits.begin(), digits.end());
                return SymbolName{ id, std::move(prefix) };
            }
        };

        struct OperationResult final
        {
            Prepared::Operation operation{ Prepared::Operation::Copy };
            std::vector<Prepared::Atom> operands;
            SymbolName closureFunction{};
            std::vector<Prepared::Capture> captures;
        };

        [[nodiscard]] auto
        Atomize(Cursor &cursor, const Expression &expression) -> Prepared::Atom;

        [[nodiscard]] auto
        AtomizeMany(Cursor &cursor, const std::vector<Expression> &expressions)
            -> std::vector<Prepared::Atom>
        {
            std::vector<Prepared::Atom> atoms;
            atoms.reserve(expressions.size());
            for (const auto &expression : expressions)
                atoms.push_back(Atomize(cursor, expression));
            return atoms;
        }

        [[nodiscard]] auto
        ZeroForBooleanContext(const Type &type) -> Prepared::Atom
        {
            if (Prepared::is_floating(type))
                return Prepared::Atom::constant(
                    Prepared::FloatingLiteral{ "0" },
                    type);
            return Prepared::Atom::constant(Prepared::integer_from_signed(0),
                                            type);
        }

        /// Canonicalize a numeric truth value to `value != 0` in the open
        /// block; a Boolean atom is returned unchanged.
        [[nodiscard]] auto
        Booleanize(Cursor &cursor, Prepared::Atom atom) -> Prepared::Atom
        {
            if (atom.type == Type::boolean())
                return atom;
            auto temporary = cursor.Temporary(U"$condition");
            std::vector<Prepared::Atom> operands;
            operands.push_back(std::move(atom));
            operands.push_back(ZeroForBooleanContext(operands.front().type));
            cursor.Emit(Prepared::Instruction{
                Prepared::Instruction::Kind::Bind,
                temporary,
                Type::boolean(),
                false,
                Prepared::Operation::NotEqual,
                std::move(operands),
                {},
                {},
            });
            return Prepared::Atom::variable(std::move(temporary),
                                            Type::boolean());
        }

        [[nodiscard]] auto
        IsShortCircuit(const Expression &expression) -> bool
        {
            return expression.kind == Expression::Kind::Primitive
                   && (expression.primitive == Primitive::LogicalAnd
                       || expression.primitive == Primitive::LogicalOr)
                   && expression.operands.size() == 2U;
        }

        /**
         * @brief Lower `&&` and `||` as control flow, not as an eager operator.
         *
         * The right operand is evaluated only on the path that needs it, so
         * its calls, traps and non-termination stay conditional exactly as
         * the source wrote them. The result slot is initialized with the
         * short-circuit value before the branch and overwritten only on the
         * path that evaluates the right operand; every predecessor of the
         * join therefore carries an initialized Boolean without a phi node
         * in CorePrep's storage-oriented form. This matches the Haskell
         * CorePrep lowering.
         */
        [[nodiscard]] auto
        AtomizeShortCircuit(Cursor &cursor, const Expression &expression)
            -> Prepared::Atom
        {
            const auto isOr = expression.primitive == Primitive::LogicalOr;
            auto condition
                = Booleanize(cursor,
                             Atomize(cursor, expression.operands.front()));
            auto result = cursor.Temporary(U"$shortcircuit");
            cursor.Emit(Prepared::Instruction{
                Prepared::Instruction::Kind::Bind,
                result,
                Type::boolean(),
                true,
                Prepared::Operation::Copy,
                { Prepared::Atom::constant(Prepared::Literal{ isOr },
                                           Type::boolean()) },
                {},
                {},
            });
            const auto rightId = cursor.state.nextBlock;
            const auto joinId = rightId + 1U;
            cursor.state.nextBlock = joinId + 1U;
            if (isOr)
                cursor.Branch(std::move(condition), joinId, rightId);
            else
                cursor.Branch(std::move(condition), rightId, joinId);

            cursor.Open(rightId);
            auto right
                = Booleanize(cursor,
                             Atomize(cursor, expression.operands.back()));
            cursor.Emit(Prepared::Instruction{
                Prepared::Instruction::Kind::Assign,
                result,
                Type::boolean(),
                false,
                Prepared::Operation::Copy,
                { std::move(right) },
                {},
                {},
            });
            cursor.Jump(joinId);
            cursor.Open(joinId);
            return Prepared::Atom::variable(std::move(result), Type::boolean());
        }

        [[nodiscard]] auto
        IsConditional(const Expression &expression) -> bool
        {
            return expression.kind == Expression::Kind::Conditional
                   && expression.operands.size() == 3U;
        }

        /**
         * @brief Lower a conditional expression to a branch over a slot.
         *
         * Exactly one arm is evaluated. The result slot is bound before
         * the branch with the neutral literal of its type and each arm
         * overwrites it on its own path, so the join block reads an
         * initialized value from either predecessor without a phi node.
         * The neutral value is never observable: no path reaches the join
         * without passing through one of the two assignments.
         *
         * The symbol and block allocation order matches the Haskell
         * CorePrep lowering.
         */
        [[nodiscard]] auto
        AtomizeConditional(Cursor &cursor, const Expression &expression)
            -> Prepared::Atom
        {
            auto condition
                = Booleanize(cursor, Atomize(cursor, expression.operands[0]));
            auto result = cursor.Temporary(U"$conditional");
            cursor.Emit(Prepared::Instruction{
                Prepared::Instruction::Kind::Bind,
                result,
                expression.type,
                true,
                Prepared::Operation::Copy,
                { expression.type == Type::boolean()
                      ? Prepared::Atom::constant(Prepared::Literal{ false },
                                                 Type::boolean())
                      : ZeroForBooleanContext(expression.type) },
                {},
                {},
            });
            const auto trueId = cursor.state.nextBlock;
            const auto falseId = trueId + 1U;
            const auto joinId = falseId + 1U;
            cursor.state.nextBlock = joinId + 1U;
            cursor.Branch(std::move(condition), trueId, falseId);

            const auto prepareArm
                = [&](const Prepared::BlockId armId, const Expression &arm) {
                      cursor.Open(armId);
                      auto value = Atomize(cursor, arm);
                      cursor.Emit(Prepared::Instruction{
                          Prepared::Instruction::Kind::Assign,
                          result,
                          expression.type,
                          false,
                          Prepared::Operation::Copy,
                          { std::move(value) },
                          {},
                          {},
                      });
                      cursor.Jump(joinId);
                  };
            prepareArm(trueId, expression.operands[1]);
            prepareArm(falseId, expression.operands[2]);
            cursor.Open(joinId);
            return Prepared::Atom::variable(std::move(result), expression.type);
        }

        struct PreparedFunctionResult final
        {
            Prepared::Function function;
            std::vector<Function> pendingFunctions;
            SymbolId nextSymbol{};
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
                highest = std::max(
                    highest,
                    HighestExpressionSymbol(statement.binding.value));
            }
            else
            {
                if (statement.kind == Statement::Kind::Assign)
                    highest = statement.destination.id;
                highest
                    = std::max(highest,
                               HighestExpressionSymbol(statement.expression));
            }
            for (const auto &nested : statement.trueBranch)
                highest = std::max(highest, HighestStatementSymbol(nested));
            for (const auto &nested : statement.falseBranch)
                highest = std::max(highest, HighestStatementSymbol(nested));
            for (const auto &nested : statement.loopBody)
                highest = std::max(highest, HighestStatementSymbol(nested));
            for (const auto &nested : statement.loopUpdate)
                highest = std::max(highest, HighestStatementSymbol(nested));
            return highest;
        }

        [[nodiscard]] auto
        HighestExpressionSymbol(const Expression &expression) -> SymbolId
        {
            SymbolId highest = expression.kind == Expression::Kind::Variable
                                   ? expression.symbol.id
                                   : 0U;
            if (expression.callee)
                highest = std::max(highest,
                                   HighestExpressionSymbol(*expression.callee));
            for (const auto &operand : expression.operands)
                highest = std::max(highest, HighestExpressionSymbol(operand));
            for (const auto &capture : expression.captures)
            {
                highest = std::max(highest, capture.symbol.id);
                if (capture.value)
                    highest = std::max(highest,
                                       HighestExpressionSymbol(*capture.value));
            }
            for (const auto &[parameter, type] : expression.closureParameters)
            {
                static_cast<void>(type);
                highest = std::max(highest, parameter.id);
            }
            if (expression.closureBody)
                for (const auto &statement : *expression.closureBody)
                    highest
                        = std::max(highest, HighestStatementSymbol(statement));
            if (expression.kind == Expression::Kind::Let)
            {
                highest = std::max(highest, expression.letSymbol.id);
                if (expression.letValue)
                    highest = std::max(
                        highest,
                        HighestExpressionSymbol(*expression.letValue));
                if (expression.letBody)
                    highest = std::max(
                        highest,
                        HighestExpressionSymbol(*expression.letBody));
            }
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
        AtomizeCaptures(Cursor &cursor, const std::vector<Capture> &captures)
            -> std::vector<Prepared::Capture>
        {
            std::vector<Prepared::Capture> prepared;
            prepared.reserve(captures.size());
            for (const auto &capture : captures)
            {
                // Core verification requires the value, but keep this total
                // for direct native API callers as well.
                if (!capture.value)
                    std::abort();
                prepared.push_back(Prepared::Capture{
                    capture.mode,
                    capture.symbol,
                    capture.type,
                    Atomize(cursor, *capture.value),
                });
            }
            return prepared;
        }

        [[nodiscard]] auto
        AtomizeOperation(Cursor &cursor, const Expression &expression)
            -> OperationResult
        {
            if (expression.kind == Expression::Kind::Let
                || IsShortCircuit(expression) || IsConditional(expression))
                return { Prepared::Operation::Copy,
                         { Atomize(cursor, expression) },
                         {},
                         {} };
            if (expression.kind == Expression::Kind::Variable)
                return { Prepared::Operation::Copy,
                         { Prepared::Atom::variable(expression.symbol,
                                                    expression.type) },
                         {},
                         {} };
            if (expression.kind == Expression::Kind::Literal)
                return { Prepared::Operation::Copy,
                         { LowerLiteral(expression) },
                         {},
                         {} };
            if (expression.kind == Expression::Kind::Apply)
            {
                std::vector<Prepared::Atom> operands;
                operands.reserve(expression.operands.size() + 1U);
                operands.push_back(Atomize(cursor, *expression.callee));
                for (const auto &argument : expression.operands)
                    operands.push_back(Atomize(cursor, argument));
                return { Prepared::Operation::Call,
                         std::move(operands),
                         {},
                         {} };
            }
            if (expression.kind == Expression::Kind::Closure)
            {
                if (!expression.closureBody)
                    std::abort();
                const auto closureId = cursor.state.nextSymbol++;
                const auto digits = std::to_string(closureId);
                std::u32string spelling = U"$closure";
                spelling.append(digits.begin(), digits.end());
                SymbolName closureName{ closureId, std::move(spelling) };

                auto captures = AtomizeCaptures(cursor, expression.captures);
                std::vector<Parameter> parameters;
                parameters.reserve(expression.captures.size()
                                   + expression.closureParameters.size());
                for (const auto &capture : expression.captures)
                    parameters.push_back(
                        Parameter{ capture.symbol, capture.type });
                for (const auto &[symbol, type] : expression.closureParameters)
                    parameters.push_back(Parameter{ symbol, type });
                cursor.state.pendingFunctions.push_back(Function{
                    closureName,
                    std::move(parameters),
                    expression.closureReturnType,
                    *expression.closureBody,
                });
                return { Prepared::Operation::MakeClosure,
                         {},
                         std::move(closureName),
                         std::move(captures) };
            }
            auto operands = AtomizeMany(cursor, expression.operands);
            if (expression.primitive == Primitive::LogicalAnd
                || expression.primitive == Primitive::LogicalOr
                || expression.primitive == Primitive::LogicalNot)
                for (auto &operand : operands)
                    operand = Booleanize(cursor, std::move(operand));
            return { LowerPrimitive(expression.primitive),
                     std::move(operands),
                     {},
                     {} };
        }

        [[nodiscard]] auto
        Atomize(Cursor &cursor, const Expression &expression) -> Prepared::Atom
        {
            if (expression.kind == Expression::Kind::Variable)
                return Prepared::Atom::variable(expression.symbol,
                                                expression.type);
            if (expression.kind == Expression::Kind::Literal)
                return LowerLiteral(expression);
            if (expression.kind == Expression::Kind::Let)
            {
                if (!expression.letValue || !expression.letBody)
                    std::abort();
                // The bound value keeps its own operation, as in the
                // Haskell lowering; an intermediate temporary would make
                // the two adapters disagree on every non-atomic value.
                auto value = AtomizeOperation(cursor, *expression.letValue);
                cursor.Emit(
                    Prepared::Instruction{ Prepared::Instruction::Kind::Bind,
                                           expression.letSymbol,
                                           expression.letType,
                                           false,
                                           value.operation,
                                           std::move(value.operands),
                                           std::move(value.closureFunction),
                                           std::move(value.captures) });
                return Atomize(cursor, *expression.letBody);
            }
            if (IsShortCircuit(expression))
                return AtomizeShortCircuit(cursor, expression);
            if (expression.kind == Expression::Kind::Conditional)
            {
                // Core verification requires three operands, but keep this
                // total for direct native API callers as well.
                if (!IsConditional(expression))
                    std::abort();
                return AtomizeConditional(cursor, expression);
            }

            auto operation = AtomizeOperation(cursor, expression);
            auto temporary = cursor.Temporary(U"$coreprep");
            cursor.Emit(
                Prepared::Instruction{ Prepared::Instruction::Kind::Bind,
                                       temporary,
                                       expression.type,
                                       false,
                                       operation.operation,
                                       std::move(operation.operands),
                                       std::move(operation.closureFunction),
                                       std::move(operation.captures) });
            return Prepared::Atom::variable(std::move(temporary),
                                            expression.type);
        }

        void
        PrepareStatements(Cursor &cursor,
                          const std::vector<Statement> &statements);

        /**
         * @brief Prepare one loop region with a scoped transfer-target pair.
         *
         * The region starts in a fresh block. While it is built, `break`
         * jumps to breakTarget and `continue` to continueTarget; the
         * enclosing pair is restored afterwards, so an inner transfer
         * cannot target an outer loop. A region that is still open at its
         * end jumps to fallthroughTarget.
         *
         * The fallthrough successor is a separate argument because it is
         * not always the `continue` target: a for-loop update region is
         * entered by `continue` but must fall through to the condition.
         * Reusing the continue target there makes the region branch to
         * itself and never re-test the loop condition.
         */
        void
        PrepareLoopRegion(Cursor &cursor,
                          const Prepared::BlockId startId,
                          const Prepared::BlockId breakTarget,
                          const Prepared::BlockId continueTarget,
                          const Prepared::BlockId fallthroughTarget,
                          const std::vector<Statement> &statements)
        {
            cursor.state.loopTargets.emplace_back(breakTarget, continueTarget);
            cursor.Open(startId);
            PrepareStatements(cursor, statements);
            if (cursor.open)
                cursor.Jump(fallthroughTarget);
            cursor.state.loopTargets.pop_back();
        }

        /// Evaluate a loop or branch condition in the open block and return
        /// its Boolean atom. Short-circuit operands may leave a different
        /// block open than the one the condition started in.
        [[nodiscard]] auto
        PrepareCondition(Cursor &cursor, const Expression &condition)
            -> Prepared::Atom
        {
            return Booleanize(cursor, Atomize(cursor, condition));
        }

        void
        PrepareBranchRegion(Cursor &cursor,
                            const Prepared::BlockId startId,
                            const Prepared::BlockId joinId,
                            const std::vector<Statement> &statements)
        {
            cursor.Open(startId);
            PrepareStatements(cursor, statements);
            if (cursor.open)
                cursor.Jump(joinId);
        }

        /**
         * @brief Prepare an `if` and every `else if` that continues it.
         *
         * An `else if` reaches Core as a false branch that holds exactly
         * one nested `if`. Preparing such a chain by recursion uses stack
         * in proportion to its length, and a long chain in valid source
         * would overflow it, so the links are prepared in a loop.
         *
         * The blocks are created, numbered and closed exactly as the
         * recursive formulation would: each link takes three consecutive
         * block identities for its true branch, its false branch and its
         * join; a false branch that only holds the next link opens that
         * link in the false block; and when the last link is done, the
         * joins are completed from the innermost outwards, each enclosing
         * false branch falling through to its own join. Both CorePrep
         * lowerings are compared block by block, so this order is part of
         * the contract.
         */
        void
        PrepareConditionalChain(Cursor &cursor, const Statement &first)
        {
            std::vector<Prepared::BlockId> joins;
            const Statement *link = &first;
            for (;;)
            {
                auto condition = PrepareCondition(cursor, link->expression);
                const auto trueId = cursor.state.nextBlock;
                const auto falseId = trueId + 1U;
                const auto joinId = falseId + 1U;
                cursor.state.nextBlock = joinId + 1U;
                cursor.Branch(std::move(condition), trueId, falseId);
                PrepareBranchRegion(cursor, trueId, joinId, link->trueBranch);
                joins.push_back(joinId);
                const auto continues
                    = link->falseBranch.size() == 1U
                      && link->falseBranch.front().kind == Statement::Kind::If;
                if (!continues)
                {
                    PrepareBranchRegion(cursor,
                                        falseId,
                                        joinId,
                                        link->falseBranch);
                    break;
                }
                // The false branch is the next link: it starts in the false
                // block, and its own join is completed before this one.
                cursor.Open(falseId);
                link = &link->falseBranch.front();
            }
            cursor.Open(joins.back());
            joins.pop_back();
            while (!joins.empty())
            {
                // The end of an enclosing false branch: fall through to its
                // join when the inner join is reachable, then continue there.
                if (cursor.open)
                    cursor.Jump(joins.back());
                cursor.Open(joins.back());
                joins.pop_back();
            }
        }

        /**
         * @brief Append structured statements to the cursor's open block.
         *
         * On return the cursor is either still open, meaning control falls
         * through to whatever the caller places next, or closed by a
         * return, break or continue, after which the remaining statements
         * of the region are unreachable and are not lowered.
         */
        void
        PrepareStatements(Cursor &cursor,
                          const std::vector<Statement> &statements)
        {
            for (const auto &statement : statements)
            {
                switch (statement.kind)
                {
                    case Statement::Kind::Bind:
                    {
                        auto operation
                            = AtomizeOperation(cursor, statement.binding.value);
                        cursor.Emit(Prepared::Instruction{
                            Prepared::Instruction::Kind::Bind,
                            statement.binding.symbol,
                            statement.binding.type,
                            statement.binding.mutableBinding,
                            operation.operation,
                            std::move(operation.operands),
                            std::move(operation.closureFunction),
                            std::move(operation.captures) });
                        break;
                    }
                    case Statement::Kind::Assign:
                    {
                        auto value = Atomize(cursor, statement.expression);
                        cursor.Emit(Prepared::Instruction{
                            Prepared::Instruction::Kind::Assign,
                            statement.destination,
                            statement.expression.type,
                            false,
                            Prepared::Operation::Copy,
                            { std::move(value) },
                            {},
                            {} });
                        break;
                    }
                    case Statement::Kind::Evaluate:
                    {
                        // Only a call may be an instruction whose result is
                        // dropped: the record has no result type on the
                        // wire, and a reader recovers it from the callee.
                        // Any other value is computed into an ordinary
                        // temporary, so its operands still run and may
                        // trap, and the unused atom is ignored.
                        if (statement.expression.kind
                            != Expression::Kind::Apply)
                        {
                            static_cast<void>(
                                Atomize(cursor, statement.expression));
                            break;
                        }
                        auto operation
                            = AtomizeOperation(cursor, statement.expression);
                        cursor.Emit(Prepared::Instruction{
                            Prepared::Instruction::Kind::Evaluate,
                            {},
                            statement.expression.type,
                            false,
                            operation.operation,
                            std::move(operation.operands),
                            std::move(operation.closureFunction),
                            std::move(operation.captures) });
                        break;
                    }
                    case Statement::Kind::Return:
                    {
                        auto value = Atomize(cursor, statement.expression);
                        cursor.Close({ Prepared::Terminator::Kind::Return,
                                       std::move(value),
                                       0U,
                                       0U });
                        return;
                    }
                    case Statement::Kind::Break:
                    case Statement::Kind::Continue:
                    {
                        // Core verification rejects a transfer outside a
                        // loop; stay total for direct native API callers.
                        if (cursor.state.loopTargets.empty())
                        {
                            cursor.Close(
                                { Prepared::Terminator::Kind::Unreachable,
                                  {},
                                  0U,
                                  0U });
                            return;
                        }
                        const auto targets = cursor.state.loopTargets.back();
                        cursor.Jump(statement.kind == Statement::Kind::Break
                                        ? targets.first
                                        : targets.second);
                        return;
                    }
                    case Statement::Kind::While:
                    {
                        // The condition owns a dedicated header block.
                        // Folding it into the incoming block would make the
                        // back-edge re-execute every straight-line statement
                        // that precedes the loop, including the initializers
                        // it tests.
                        const auto conditionId = cursor.state.nextBlock;
                        const auto bodyId = conditionId + 1U;
                        const auto exitId = bodyId + 1U;
                        cursor.state.nextBlock = exitId + 1U;

                        cursor.Jump(conditionId);
                        cursor.Open(conditionId);
                        cursor.Branch(
                            PrepareCondition(cursor, statement.expression),
                            bodyId,
                            exitId);
                        PrepareLoopRegion(cursor,
                                          bodyId,
                                          exitId,
                                          conditionId,
                                          conditionId,
                                          statement.loopBody);
                        cursor.Open(exitId);
                        break;
                    }
                    case Statement::Kind::DoWhile:
                    {
                        const auto bodyId = cursor.state.nextBlock;
                        const auto conditionId = bodyId + 1U;
                        const auto exitId = conditionId + 1U;
                        cursor.state.nextBlock = exitId + 1U;

                        cursor.Jump(bodyId);
                        PrepareLoopRegion(cursor,
                                          bodyId,
                                          exitId,
                                          conditionId,
                                          conditionId,
                                          statement.loopBody);
                        cursor.Open(conditionId);
                        cursor.Branch(
                            PrepareCondition(cursor, statement.expression),
                            bodyId,
                            exitId);
                        cursor.Open(exitId);
                        break;
                    }
                    case Statement::Kind::For:
                    {
                        const auto conditionId = cursor.state.nextBlock;
                        const auto bodyId = conditionId + 1U;
                        const auto updateId = bodyId + 1U;
                        const auto exitId = updateId + 1U;
                        cursor.state.nextBlock = exitId + 1U;

                        cursor.Jump(conditionId);
                        cursor.Open(conditionId);
                        // A numeric condition's canonicalizing comparison is
                        // part of the header and is re-evaluated each pass.
                        cursor.Branch(
                            PrepareCondition(cursor, statement.expression),
                            bodyId,
                            exitId);
                        PrepareLoopRegion(cursor,
                                          bodyId,
                                          exitId,
                                          updateId,
                                          updateId,
                                          statement.loopBody);
                        // `continue` enters the update region; the region
                        // itself then returns to the condition, never to its
                        // own entry.
                        PrepareLoopRegion(cursor,
                                          updateId,
                                          exitId,
                                          updateId,
                                          conditionId,
                                          statement.loopUpdate);
                        cursor.Open(exitId);
                        break;
                    }
                    case Statement::Kind::If:
                        PrepareConditionalChain(cursor, statement);
                        break;
                }
            }
        }

        [[nodiscard]] auto
        PrepareFunction(const Function &function, SymbolId nextSymbol)
            -> PreparedFunctionResult
        {
            std::vector<Prepared::Parameter> parameters;
            parameters.reserve(function.parameters.size());
            for (const auto &parameter : function.parameters)
            {
                parameters.push_back({ parameter.symbol, parameter.type });
            }
            Cursor cursor{ State{ nextSymbol, 1U, {}, {} }, 0U, {}, {}, true };
            PrepareStatements(cursor, function.body);
            // Core verification proves every path returns. A body that still
            // falls off its end is marked instead of given an invented value.
            if (cursor.open)
                cursor.Close(
                    { Prepared::Terminator::Kind::Unreachable, {}, 0U, 0U });
            return {
                Prepared::Function{ function.symbol,
                                    std::move(parameters),
                                    function.returnType,
                                    0U,
                                    std::move(cursor.closed) },
                std::move(cursor.state.pendingFunctions),
                cursor.state.nextSymbol,
            };
        }
    } // namespace

    auto
    Prepare(const Module &module) -> ::visual_xsharp::core::CorePrepModule
    {
        ::visual_xsharp::core::CorePrepModule prepared{ module.name,
                                                        {},
                                                        module.sourceFiles };
        // The queue refers to the functions of the module instead of copying
        // them: a copy would duplicate every statement of the program, and
        // copying nested statements recurses once per level of nesting.
        // Lifted closure bodies are owned by a deque, whose elements keep
        // their addresses while the queue grows.
        std::deque<Function> lifted;
        std::vector<std::pair<const Function *, std::u32string>> pending;
        pending.reserve(module.functions.size());
        for (const auto &function : module.functions)
            pending.emplace_back(&function, function.sourceFile);
        // Lifted closure bodies only mention identities already counted
        // here or generated by the shared counter, so the seed stays valid
        // for functions appended to the queue later.
        SymbolId nextSymbol = 1U;
        for (const auto &[function, _] : pending)
            nextSymbol
                = std::max(nextSymbol, HighestFunctionSymbol(*function) + 1U);

        // Closure bodies are lifted as ordinary Core functions and fed back
        // through the same work queue. This naturally handles nested closures
        // without adding a second, subtly different lowering implementation.
        for (std::size_t index = 0U; index < pending.size(); ++index)
        {
            auto result = PrepareFunction(*pending[index].first, nextSymbol);
            result.function.sourceFile = pending[index].second;
            prepared.functions.push_back(std::move(result.function));
            nextSymbol = result.nextSymbol;
            for (auto &function : result.pendingFunctions)
            {
                lifted.push_back(std::move(function));
                // Copy the owner's path first: growing the queue may move
                // the element it is read from.
                auto sourceFile = pending[index].second;
                pending.emplace_back(&lifted.back(), std::move(sourceFile));
            }
        }
        return prepared;
    }
} // namespace Visual::XSharp::Core::CorePrep
