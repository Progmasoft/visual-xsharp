// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "Visual/XSharp/Core/Ownership.hpp"
#include "Visual/XSharp/Core/Scalar.hpp"
#include "Visual/XSharp/Xpp/Verifier.hpp"

namespace Visual::XSharp::Xpp
{
    namespace
    {
        namespace Core = ::visual_xsharp::core;
        namespace IR = ::visual_xsharp::xpp;

        struct Context final
        {
            std::vector<VerificationIssue> issues;
            IR::SymbolId function{};
            IR::BlockId block{};
            std::size_t instruction{};

            void
            Add(std::string code, std::string message)
            {
                issues.push_back({ std::move(code), std::move(message), function, block, instruction });
            }
        };

        [[nodiscard]] auto
        LiteralMatches(const IR::Operand &operand) -> bool
        {
            if (operand.kind != IR::Operand::Kind::Literal)
                return true;
            return !Core::validate_literal(operand.literal, operand.type).has_value();
        }

        [[nodiscard]] auto
        ExpectedArity(IR::Opcode opcode) -> std::size_t
        {
            switch (opcode)
            {
                case IR::Opcode::Copy:
                case IR::Opcode::Negate:
                case IR::Opcode::LogicalNot:
                case IR::Opcode::RetainStrong:
                case IR::Opcode::ReleaseStrong:
                case IR::Opcode::MakeWeak:
                case IR::Opcode::LockWeak:
                case IR::Opcode::ReleaseWeak:
                case IR::Opcode::MakeUnowned:
                case IR::Opcode::LoadUnowned:
                case IR::Opcode::ReleaseUnowned:
                    return 1U;
                case IR::Opcode::Call:
                case IR::Opcode::MakeClosure:
                    return 0U;
                default:
                    return 2U;
            }
        }

        [[nodiscard]] auto
        IsAarcType(const Core::Type &type) -> bool
        {
            // Nominal declarations are resolved before Xpp in the complete frontend. The
            // current Core wire has no nominal-kind slot, so a surviving Named value is the
            // reference-layout branch; CoW values must be expanded before this boundary.
            return Core::UsesAarc(type) || type.kind == Core::Type::Kind::Named;
        }

        [[nodiscard]] auto
        IsOwnershipOpcode(IR::Opcode opcode) -> bool
        {
            return opcode >= IR::Opcode::RetainStrong && opcode <= IR::Opcode::ReleaseUnowned;
        }

        [[nodiscard]] auto
        FunctionType(const IR::Function &function) -> Core::Type
        {
            std::vector<Core::Type> parameters;
            parameters.reserve(function.parameters.size());
            for (const auto &parameter : function.parameters)
                parameters.push_back(parameter.type);
            return Core::Type::function(std::move(parameters), function.return_type);
        }

        void
        VerifyOperand(Context &context, const IR::Operand &operand, const std::unordered_map<IR::SymbolId, Core::Type> &storage, const std::unordered_map<IR::SymbolId, const IR::Function *> &functions)
        {
            if (operand.kind == IR::Operand::Kind::Literal)
            {
                if (!LiteralMatches(operand))
                    context.Add("VXP1010", "literal payload does not match its declared type");
                return;
            }
            if (operand.symbol == 0U)
            {
                context.Add("VXP1011", "symbol operand uses the reserved zero id");
                return;
            }
            if (operand.type.kind == Core::Type::Kind::Function)
            {
                const auto found = functions.find(operand.symbol);
                if (found == functions.end())
                    context.Add("VXP1012", "function operand refers to an unknown function symbol");
                else if (operand.type != FunctionType(*found->second))
                    context.Add("VXP1024", "function operand signature differs from its declaration");
                return;
            }
            const auto found = storage.find(operand.symbol);
            if (found == storage.end())
                context.Add("VXP1013", "operand reads an undefined storage symbol");
            else if (found->second != operand.type)
                context.Add("VXP1014", "operand type differs from its storage declaration");
        }

        void
        VerifyInstruction(Context &context, const IR::Instruction &value, const std::unordered_map<IR::SymbolId, Core::Type> &storage, const std::unordered_map<IR::SymbolId, const IR::Function *> &functions)
        {
            if (value.opcode == IR::Opcode::Call)
            {
                if (value.operands.empty() || value.operands.front().kind != IR::Operand::Kind::Symbol || value.operands.front().type.kind != Core::Type::Kind::Function)
                    context.Add("VXP1015", "call must begin with a typed function-symbol operand");
                else
                {
                    const auto &signature = value.operands.front().type.components;
                    if (signature.empty() || value.operands.size() != signature.size())
                        context.Add("VXP1025", "call argument count does not match its signature");
                    else
                    {
                        for (std::size_t index = 1; index < value.operands.size(); ++index)
                            if (value.operands[index].type != signature[index - 1U])
                                context.Add("VXP1026", "call argument type does not match its signature");
                        if (value.result_type != signature.back())
                            context.Add("VXP1027", "call result type does not match its signature");
                    }
                }
            }
            else if (value.opcode == IR::Opcode::MakeClosure)
            {
                const auto target = functions.find(value.closure_function);
                if (value.closure_function == 0U || target == functions.end())
                    context.Add("VXP1028", "closure operation refers to an unknown lifted function");
                if (value.result_type.kind != Core::Type::Kind::Function)
                    context.Add("VXP1029", "closure operation result must have a function type");
                if (value.capture_modes.size() != value.operands.size())
                    context.Add("VXP1030", "closure capture modes and operands differ in length");
                if (target != functions.end())
                {
                    const auto &parameters = target->second->parameters;
                    if (parameters.size() < value.operands.size())
                        context.Add("VXP1031", "lifted function has fewer parameters than closure captures");
                    else
                        for (std::size_t index = 0; index < value.operands.size(); ++index)
                            if (parameters[index].type != value.operands[index].type)
                                context.Add("VXP1032", "closure capture type differs from its lifted parameter");
                }
                for (std::size_t index = 0; index < value.capture_modes.size(); ++index)
                    if (value.capture_modes[index] != Core::CaptureMode::Strong
                        && !IsAarcType(value.operands[index].type))
                        context.Add("VXP1033", "weak and unowned captures require an AARC reference type");
            }
            else if (IsOwnershipOpcode(value.opcode))
            {
                if (value.operands.size() != 1U)
                    context.Add("VXP1034", "ownership instruction requires exactly one operand");
                else if (!IsAarcType(value.operands.front().type))
                    context.Add("VXP1035", "ownership instruction requires an AARC reference type");

                const auto releases = value.opcode == IR::Opcode::ReleaseStrong
                                      || value.opcode == IR::Opcode::ReleaseWeak
                                      || value.opcode == IR::Opcode::ReleaseUnowned;
                if (releases)
                {
                    if (value.effect != IR::Instruction::Effect::Discard || value.result_type.kind != Core::Type::Kind::Unit)
                        context.Add("VXP1036", "release ownership instruction must discard a Unit result");
                }
                else if (value.effect == IR::Instruction::Effect::Discard
                         || value.operands.empty() || value.result_type != value.operands.front().type)
                    context.Add("VXP1037", "producing ownership instruction must preserve its operand type");
            }
            else if (value.operands.size() != ExpectedArity(value.opcode))
                context.Add("VXP1016", "instruction has the wrong operand count");

            if (value.effect == IR::Instruction::Effect::Discard)
            {
                if (value.destination != 0U)
                    context.Add("VXP1017", "discard instruction must not name a destination");
            }
            else
            {
                const auto found = storage.find(value.destination);
                if (value.destination == 0U || found == storage.end())
                    context.Add("VXP1018", "result destination does not name declared storage");
                else if (found->second != value.result_type)
                    context.Add("VXP1019", "result type differs from destination storage");
            }
            for (const auto &operand : value.operands)
                VerifyOperand(context, operand, storage, functions);
        }

        void
        VerifyTerminator(Context &context, const IR::Terminator &value, const IR::Function &function, const std::unordered_map<IR::SymbolId, Core::Type> &storage, const std::unordered_map<IR::SymbolId, const IR::Function *> &functions, const std::unordered_set<IR::BlockId> &blocks)
        {
            if (value.kind == IR::Terminator::Kind::Return)
            {
                VerifyOperand(context, value.value, storage, functions);
                if (value.value.type != function.return_type)
                    context.Add("VXP1020", "return value type differs from the function result type");
            }
            else if (value.kind == IR::Terminator::Kind::Branch)
            {
                VerifyOperand(context, value.value, storage, functions);
                if (value.value.type.kind != Core::Type::Kind::Bool)
                    context.Add("VXP1021", "branch condition must be Bool");
                if (!blocks.contains(value.true_target) || !blocks.contains(value.false_target))
                    context.Add("VXP1022", "branch target does not name a function block");
            }
            else if (value.kind == IR::Terminator::Kind::Jump && !blocks.contains(value.true_target))
                context.Add("VXP1023", "jump target does not name a function block");
        }
    } // namespace

    auto
    Verify(const ::visual_xsharp::xpp::Module &module) -> std::vector<VerificationIssue>
    {
        Context context;
        if (module.name.empty() || std::ranges::any_of(module.name, [](const auto &part) {
                return part.empty();
            }))
            context.Add("VXP1001", "Xpp module name must contain non-empty components");
        if (module.functions.empty())
            context.Add("VXP1002", "Xpp module contains no functions");

        std::unordered_map<IR::SymbolId, const IR::Function *> functions;
        for (const auto &function : module.functions)
            if (function.symbol.id == 0U || function.symbol.spelling.empty() || !functions.emplace(function.symbol.id, &function).second)
            {
                context.function = function.symbol.id;
                context.Add("VXP1003", "function symbol is missing, empty, or duplicated");
            }

        for (const auto &function : module.functions)
        {
            context.function = function.symbol.id;
            std::unordered_set<IR::BlockId> blocks;
            for (const auto &block : function.blocks)
                if (!blocks.insert(block.id).second)
                {
                    context.block = block.id;
                    context.Add("VXP1004", "block id is declared more than once");
                }
            if (!blocks.contains(function.entry))
                context.Add("VXP1005", "function entry does not name a block");

            std::unordered_map<IR::SymbolId, Core::Type> storage;
            for (const auto &parameter : function.parameters)
                if (parameter.symbol.id == 0U || !storage.emplace(parameter.symbol.id, parameter.type).second)
                    context.Add("VXP1006", "parameter storage symbol is missing or duplicated");
            for (const auto &block : function.blocks)
            {
                context.block = block.id;
                for (std::size_t index = 0; index < block.instructions.size(); ++index)
                {
                    context.instruction = index;
                    const auto &instruction = block.instructions[index];
                    if (instruction.effect == IR::Instruction::Effect::Define && (instruction.destination == 0U || !storage.emplace(instruction.destination, instruction.result_type).second))
                        context.Add("VXP1007", "defined storage symbol is missing or duplicated");
                }
            }

            for (const auto &block : function.blocks)
            {
                context.block = block.id;
                for (std::size_t index = 0; index < block.instructions.size(); ++index)
                {
                    context.instruction = index;
                    VerifyInstruction(context, block.instructions[index], storage, functions);
                }
                context.instruction = block.instructions.size();
                VerifyTerminator(context, block.terminator, function, storage, functions, blocks);
            }
        }
        return context.issues;
    }
} // namespace Visual::XSharp::Xpp
