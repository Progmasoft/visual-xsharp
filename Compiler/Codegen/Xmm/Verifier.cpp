// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

#include <algorithm>
#include <array>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "Visual/XSharp/Core/Callable.hpp"
#include "Visual/XSharp/Core/Ownership.hpp"
#include "Visual/XSharp/Core/Scalar.hpp"
#include "Visual/XSharp/Xmm/Verifier.hpp"

namespace Visual::XSharp::Xmm
{
    namespace core = ::visual_xsharp::core;
    namespace xmm = ::visual_xsharp::xmm;

    namespace
    {
        // Context records the exact source location while validation walks the module. Keeping
        // diagnostics as data (rather than printing here) lets the CLI, tests and embedding
        // tools choose their own presentation without weakening the verifier.
        struct Context final
        {
            std::vector<VerificationIssue> issues;
            core::SymbolId function{};
            xmm::BlockId block{};
            std::size_t instruction{};

            void
            add(IssueKind kind, std::string code, std::string message)
            {
                issues.push_back(VerificationIssue{ kind, std::move(code), std::move(message), function, block, instruction });
            }
        };

        [[nodiscard]] auto
        SupportedType(const core::Type &type) -> bool
        {
            // Named and polymorphic types must be resolved before Xmm reaches LLVM. Guessing a
            // layout here would turn an incomplete front-end decision into a platform ABI.
            switch (type.kind)
            {
                case core::Type::Kind::Unit:
                case core::Type::Kind::Bool:
                case core::Type::Kind::Character:
                case core::Type::Kind::Int8:
                case core::Type::Kind::Int16:
                case core::Type::Kind::Int64:
                case core::Type::Kind::Int32:
                case core::Type::Kind::Int128:
                case core::Type::Kind::UInt8:
                case core::Type::Kind::UInt16:
                case core::Type::Kind::UInt32:
                case core::Type::Kind::UInt64:
                case core::Type::Kind::UInt128:
                case core::Type::Kind::Float16:
                case core::Type::Kind::Float32:
                case core::Type::Kind::Float64:
                case core::Type::Kind::Float128:
                case core::Type::Kind::String:
                    return true;
                case core::Type::Kind::Function:
                    return !type.components.empty() && std::ranges::all_of(type.components, [](const core::Type &component) {
                        return SupportedType(component);
                    });
                case core::Type::Kind::Named:
                    return true;
                case core::Type::Kind::TypeVariable:
                    return false;
            }
            return false;
        }

        [[nodiscard]] auto
        TypeName(const core::Type &type) -> std::string_view
        {
            switch (type.kind)
            {
                case core::Type::Kind::Unit:
                    return "Unit";
                case core::Type::Kind::Bool:
                    return "Bool";
                case core::Type::Kind::Character:
                    return "Character";
                case core::Type::Kind::Int8:
                    return "Byte";
                case core::Type::Kind::Int16:
                    return "Short";
                case core::Type::Kind::Int64:
                    return "Int";
                case core::Type::Kind::Int32:
                    return "Long";
                case core::Type::Kind::Int128:
                    return "LongInt";
                case core::Type::Kind::UInt8:
                    return "UByte";
                case core::Type::Kind::UInt16:
                    return "UShort";
                case core::Type::Kind::UInt32:
                    return "ULong";
                case core::Type::Kind::UInt64:
                    return "UInt";
                case core::Type::Kind::UInt128:
                    return "ULongInt";
                case core::Type::Kind::Float16:
                    return "SFloat";
                case core::Type::Kind::Float32:
                    return "LFloat";
                case core::Type::Kind::Float64:
                    return "Float";
                case core::Type::Kind::Float128:
                    return "Double";
                case core::Type::Kind::String:
                    return "String";
                case core::Type::Kind::Function:
                    return "function";
                case core::Type::Kind::Named:
                    return "named type";
                case core::Type::Kind::TypeVariable:
                    return "type variable";
            }
            return "unknown";
        }

        [[nodiscard]] auto
        ExpectedOperandCount(xmm::Opcode opcode) -> std::size_t
        {
            switch (opcode)
            {
                case xmm::Opcode::LoadImmediate:
                case xmm::Opcode::Move:
                case xmm::Opcode::Negate:
                case xmm::Opcode::NotBool:
                case xmm::Opcode::RetainStrong:
                case xmm::Opcode::ReleaseStrong:
                case xmm::Opcode::MakeWeak:
                case xmm::Opcode::LockWeak:
                case xmm::Opcode::ReleaseWeak:
                case xmm::Opcode::MakeUnowned:
                case xmm::Opcode::LoadUnowned:
                case xmm::Opcode::ReleaseUnowned:
                    return 1;
                case xmm::Opcode::Add:
                case xmm::Opcode::Subtract:
                case xmm::Opcode::Multiply:
                case xmm::Opcode::Divide:
                case xmm::Opcode::FloorDivide:
                case xmm::Opcode::Remainder:
                case xmm::Opcode::CompareLess:
                case xmm::Opcode::CompareLessEqual:
                case xmm::Opcode::CompareGreater:
                case xmm::Opcode::CompareGreaterEqual:
                case xmm::Opcode::CompareEqual:
                case xmm::Opcode::CompareNotEqual:
                case xmm::Opcode::AndBool:
                case xmm::Opcode::OrBool:
                    return 2;
                case xmm::Opcode::Call:
                case xmm::Opcode::MakeClosure:
                    return 0;
            }
            return 0;
        }

        [[nodiscard]] auto
        IsAarcType(const core::Type &type) -> bool
        {
            return core::UsesAarc(type) || type.kind == core::Type::Kind::Named;
        }

        [[nodiscard]] auto
        IsOwnershipOpcode(xmm::Opcode opcode) -> bool
        {
            return opcode >= xmm::Opcode::RetainStrong && opcode <= xmm::Opcode::ReleaseUnowned;
        }

        void
        VerifyLiteral(Context &context, const xmm::Value &value)
        {
            if (value.kind != xmm::Value::Kind::Immediate)
                return;
            if (const auto issue = core::validate_literal(value.immediate, value.type))
                context.add(IssueKind::InvalidLiteral, "VXL1018", "immediate payload is invalid: " + *issue);
        }

        void
        VerifyValue(Context &context, const xmm::Value &value, const std::unordered_map<xmm::VirtualRegister, core::Type> &registers, const std::unordered_map<core::SymbolId, const xmm::Function *> &functions)
        {
            if (!SupportedType(value.type))
                context.add(IssueKind::UnsupportedType, "VXL1005", "LLVM lowering does not support " + std::string(TypeName(value.type)) + " yet");
            if (value.kind == xmm::Value::Kind::Register)
            {
                const auto found = registers.find(value.reg);
                if (value.reg == 0 || found == registers.end())
                    context.add(IssueKind::UndefinedRegister, "VXL1011", "operand reads an undefined virtual register");
                else if (found->second != value.type)
                    context.add(IssueKind::OperandType, "VXL1012", "register operand type disagrees with its definition");
            }
            else if (value.kind == xmm::Value::Kind::Function)
            {
                const auto found = functions.find(value.symbol);
                if (value.symbol == 0 || found == functions.end())
                    context.add(IssueKind::InvalidCall, "VXL1013", "call operand refers to an unknown function symbol");
                if (value.type.kind != core::Type::Kind::Function)
                    context.add(IssueKind::OperandType, "VXL1014", "function operand must carry a function type");
            }
            else
                VerifyLiteral(context, value);
        }

        void
        VerifyInstruction(Context &context, const xmm::Instruction &instruction, std::unordered_map<xmm::VirtualRegister, core::Type> &registers, const std::unordered_map<core::SymbolId, const xmm::Function *> &functions)
        {
            if (!SupportedType(instruction.result_type))
                context.add(IssueKind::UnsupportedType, "VXL1005", "instruction result has an unsupported LLVM type");
            for (const auto &operand : instruction.operands)
                VerifyValue(context, operand, registers, functions);

            if (instruction.opcode == xmm::Opcode::Call)
            {
                // A function type stores parameters followed by its result. The instruction
                // stores either a direct Function identity or a Register-backed closure first.
                // Capture parameters remain private to the closure thunk and never appear at
                // an ordinary call site.
                if (instruction.operands.empty()
                    || (instruction.operands.front().kind != xmm::Value::Kind::Function
                        && instruction.operands.front().kind != xmm::Value::Kind::Register))
                    context.add(IssueKind::InvalidCall, "VXL1015", "call must begin with a direct function or closure register");
                else
                {
                    const auto signature = ::Visual::XSharp::Core::Callable::Decompose(
                        instruction.operands.front().type);
                    if (!signature)
                        context.add(IssueKind::InvalidCall, "VXL1041", "call operand does not carry a callable type");
                    else if (instruction.operands.size() != signature->parameters.size() + 1U)
                        context.add(IssueKind::OperandCount, "VXL1016", "call argument count does not match its signature");
                    else
                    {
                        for (std::size_t index = 1; index < instruction.operands.size(); ++index)
                            if (instruction.operands[index].type != signature->parameters[index - 1U])
                                context.add(IssueKind::OperandType, "VXL1017", "call argument type does not match its signature");
                        if (instruction.result_type != signature->result)
                            context.add(IssueKind::ResultType, "VXL1019", "call result type does not match its signature");
                    }
                }
            }
            else if (instruction.opcode == xmm::Opcode::MakeClosure)
            {
                const auto target = functions.find(instruction.closure_function);
                if (instruction.closure_function == 0 || target == functions.end())
                    context.add(IssueKind::InvalidCall, "VXL1032", "closure operation refers to an unknown lifted function");
                if (instruction.result_type.kind != core::Type::Kind::Function)
                    context.add(IssueKind::ResultType, "VXL1033", "closure operation result must be callable");
                if (instruction.capture_modes.size() != instruction.operands.size())
                    context.add(IssueKind::OperandCount, "VXL1034", "closure capture modes and operands differ in length");
                if (target != functions.end())
                {
                    const auto &types = target->second->parameter_types;
                    std::vector<core::Type> captures;
                    captures.reserve(instruction.operands.size());
                    for (const auto &operand : instruction.operands)
                        captures.push_back(operand.type);
                    const auto contract = ::Visual::XSharp::Core::Callable::ValidateClosure(
                        captures,
                        types,
                        target->second->return_type,
                        instruction.result_type);
                    using ContractError = ::Visual::XSharp::Core::Callable::ClosureContractError;
                    switch (contract.error)
                    {
                        case ContractError::None:
                            break;
                        case ContractError::ResultIsNotCallable:
                            // VXL1033 already reports the public result shape.
                            break;
                        case ContractError::TargetHasTooFewParameters:
                            context.add(IssueKind::ParameterShape, "VXL1035", "lifted function has fewer parameters than closure captures");
                            break;
                        case ContractError::CaptureTypeMismatch:
                            context.add(IssueKind::OperandType, "VXL1036", "closure capture type differs from its lifted parameter");
                            break;
                        case ContractError::PublicParameterCountMismatch:
                            context.add(IssueKind::ParameterShape, "VXL1042", "lifted function public parameter count differs from the closure signature");
                            break;
                        case ContractError::PublicParameterTypeMismatch:
                            context.add(IssueKind::OperandType, "VXL1043", "lifted function public parameter type differs from the closure signature");
                            break;
                        case ContractError::ResultTypeMismatch:
                            context.add(IssueKind::ResultType, "VXL1044", "lifted function result differs from the closure signature");
                            break;
                    }
                }
            }
            else if (IsOwnershipOpcode(instruction.opcode))
            {
                if (instruction.operands.size() != 1U)
                    context.add(IssueKind::OperandCount, "VXL1037", "ownership instruction requires exactly one operand");
                else if (!IsAarcType(instruction.operands.front().type))
                    context.add(IssueKind::OperandType, "VXL1038", "ownership instruction requires an AARC reference type");

                const auto releases = instruction.opcode == xmm::Opcode::ReleaseStrong
                                      || instruction.opcode == xmm::Opcode::ReleaseWeak
                                      || instruction.opcode == xmm::Opcode::ReleaseUnowned;
                if (releases)
                {
                    if (instruction.has_result || instruction.result_type.kind != core::Type::Kind::Unit)
                        context.add(IssueKind::ResultType, "VXL1039", "release ownership instruction must have no result");
                }
                else if (!instruction.has_result || instruction.operands.empty()
                         || instruction.result_type != instruction.operands.front().type)
                    context.add(IssueKind::ResultType, "VXL1040", "producing ownership instruction must preserve its operand type");
            }
            else
            {
                const auto expected = ExpectedOperandCount(instruction.opcode);
                if (instruction.operands.size() != expected)
                    context.add(IssueKind::OperandCount, "VXL1020", "instruction has the wrong operand count");
                if (instruction.opcode == xmm::Opcode::Move || instruction.opcode == xmm::Opcode::LoadImmediate)
                {
                    if (!instruction.operands.empty() && instruction.operands.front().type != instruction.result_type)
                        context.add(IssueKind::ResultType, "VXL1021", "move result type differs from its operand");
                }
                else if (instruction.opcode == xmm::Opcode::AndBool || instruction.opcode == xmm::Opcode::OrBool || instruction.opcode == xmm::Opcode::NotBool)
                {
                    if (instruction.result_type.kind != core::Type::Kind::Bool || std::ranges::any_of(instruction.operands, [](const xmm::Value &value) {
                            return value.type.kind != core::Type::Kind::Bool;
                        }))
                        context.add(IssueKind::OperandType, "VXL1022", "logical instruction requires Bool operands and result");
                }
                else if (instruction.opcode >= xmm::Opcode::CompareLess && instruction.opcode <= xmm::Opcode::CompareNotEqual)
                {
                    if (instruction.result_type.kind != core::Type::Kind::Bool || (instruction.operands.size() == 2 && instruction.operands[0].type != instruction.operands[1].type))
                        context.add(IssueKind::OperandType, "VXL1023", "comparison requires equal operand types and Bool result");
                }
                else if (instruction.opcode != xmm::Opcode::Call)
                {
                    if (!core::is_numeric(instruction.result_type) || std::ranges::any_of(instruction.operands, [&instruction](const xmm::Value &value) {
                            return value.type != instruction.result_type;
                        }))
                        context.add(IssueKind::OperandType, "VXL1024", "numeric instruction operand and result types must agree");
                }
            }

            if (instruction.has_result)
            {
                if (instruction.destination == 0)
                    context.add(IssueKind::InvalidFunction, "VXL1025", "result-producing instruction has register zero");
                else if (const auto [found, inserted] = registers.emplace(instruction.destination, instruction.result_type);
                         !inserted && found->second != instruction.result_type)
                    // Rewriting a virtual register is legal because Xmm registers are storage.
                    // Changing its established type is not: one LLVM alloca cannot safely serve
                    // two unrelated layouts on different control-flow paths.
                    context.add(IssueKind::RegisterRedefinition, "VXL1026", "virtual register is written with a type that differs from its established storage type");
            }
            else if (instruction.result_type.kind != core::Type::Kind::Unit
                     && instruction.opcode != xmm::Opcode::Call)
                context.add(IssueKind::ResultType, "VXL1027", "discarded non-call instruction must have Unit result");
        }

        void
        VerifyTerminator(Context &context, const xmm::Terminator &terminator, const xmm::Function &function, const std::unordered_map<xmm::VirtualRegister, core::Type> &registers, const std::unordered_map<core::SymbolId, const xmm::Function *> &functions, const std::unordered_set<xmm::BlockId> &blocks)
        {
            if (terminator.kind == xmm::Terminator::Kind::Return)
            {
                VerifyValue(context, terminator.value, registers, functions);
                if (terminator.value.type != function.return_type)
                    context.add(IssueKind::InvalidReturn, "VXL1028", "return value type differs from the function result type");
            }
            else if (terminator.kind == xmm::Terminator::Kind::Branch)
            {
                VerifyValue(context, terminator.value, registers, functions);
                if (terminator.value.type.kind != core::Type::Kind::Bool)
                    context.add(IssueKind::InvalidBranch, "VXL1029", "branch condition must be Bool");
                if (!blocks.contains(terminator.true_target) || !blocks.contains(terminator.false_target))
                    context.add(IssueKind::InvalidTarget, "VXL1030", "branch target does not name a function block");
            }
            else if (terminator.kind == xmm::Terminator::Kind::Jump && !blocks.contains(terminator.true_target))
                context.add(IssueKind::InvalidTarget, "VXL1031", "jump target does not name a function block");
        }
    } // namespace

    auto
    Verify(const xmm::Module &module) -> std::vector<VerificationIssue>
    {
        Context context;
        if (module.functions.empty())
            context.add(IssueKind::EmptyModule, "VXL1001", "Xmm module contains no functions");
        if (module.name.empty() || std::ranges::any_of(module.name, [](const std::u32string &part) {
                return part.empty();
            }))
            context.add(IssueKind::InvalidModuleName, "VXL1002", "Xmm module name must contain non-empty components");

        std::unordered_map<core::SymbolId, const xmm::Function *> functions;
        // Build the complete symbol catalog before checking bodies. Forward calls and
        // recursion then validate exactly like calls to functions declared earlier.
        for (const auto &function : module.functions)
        {
            // Parameter types seed the register storage table. Instruction results may add
            // registers or rewrite them with the same type; every later read is checked
            // against this table before LLVM lowering assumes a matching slot exists.
            if (function.symbol.id == 0 || function.symbol.spelling.empty())
            {
                context.function = function.symbol.id;
                context.add(IssueKind::InvalidFunction, "VXL1003", "function requires a non-zero symbol and spelling");
            }
            if (!functions.emplace(function.symbol.id, &function).second)
            {
                context.function = function.symbol.id;
                context.add(IssueKind::DuplicateFunction, "VXL1004", "function symbol is declared more than once");
            }
        }

        for (const auto &function : module.functions)
        {
            context.function = function.symbol.id;
            context.block = 0;
            context.instruction = 0;
            if (!SupportedType(function.return_type))
                context.add(IssueKind::UnsupportedType, "VXL1005", "function result type is not lowerable to LLVM");
            if (function.parameter_registers.size() != function.parameter_types.size())
                context.add(IssueKind::ParameterShape, "VXL1006", "parameter registers and parameter types differ in length");

            std::unordered_map<xmm::VirtualRegister, core::Type> registers;
            const auto parameterCount = std::min(function.parameter_registers.size(), function.parameter_types.size());
            for (std::size_t index = 0; index < parameterCount; ++index)
            {
                if (function.parameter_registers[index] == 0 || !registers.emplace(function.parameter_registers[index], function.parameter_types[index]).second)
                    context.add(IssueKind::ParameterShape, "VXL1007", "parameter virtual registers must be unique and non-zero");
                if (!SupportedType(function.parameter_types[index]))
                    context.add(IssueKind::UnsupportedType, "VXL1005", "parameter type is not lowerable to LLVM");
            }

            std::unordered_set<xmm::BlockId> blocks;
            for (const auto &block : function.blocks)
                if (!blocks.insert(block.id).second)
                {
                    context.block = block.id;
                    context.add(IssueKind::DuplicateBlock, "VXL1008", "block id is declared more than once");
                }
            if (!blocks.contains(function.entry))
                context.add(IssueKind::MissingEntry, "VXL1009", "function entry does not name a block");

            for (const auto &block : function.blocks)
            {
                context.block = block.id;
                for (std::size_t index = 0; index < block.instructions.size(); ++index)
                {
                    context.instruction = index;
                    VerifyInstruction(context, block.instructions[index], registers, functions);
                }
                context.instruction = block.instructions.size();
                VerifyTerminator(context, block.terminator, function, registers, functions, blocks);
            }
        }
        return context.issues;
    }
} // namespace Visual::XSharp::Xmm
