// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

#include <algorithm>
#include <array>
#include <iterator>
#include <llvm/ADT/SmallString.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/ADT/Twine.h>
#include <string>

#include "Visual/XSharp/ADTs/DenseIdMap.hpp"
#include "Visual/XSharp/Analysis/DefiniteInitialization.hpp"
#include "Visual/XSharp/Core/Callable.hpp"
#include "Visual/XSharp/Core/Ownership.hpp"
#include "Visual/XSharp/Core/Scalar.hpp"
#include "Visual/XSharp/Xmm/OwnershipVerifier.hpp"
#include "Visual/XSharp/Xmm/Verifier.hpp"

namespace Visual::XSharp::Xmm
{
    namespace core = ::visual_xsharp::core;
    namespace xmm = ::visual_xsharp::xmm;
    namespace dataflow = ::Visual::XSharp::Analysis;
    namespace adts = ::Visual::XSharp::ADTs;

    namespace
    {
        using FunctionCatalog
            = adts::DenseIdMap<core::SymbolId, const xmm::Function *>;
        using RegisterCatalog
            = adts::DenseIdMap<xmm::VirtualRegister, core::Type>;
        using BlockCatalog = adts::DenseIdSet<xmm::BlockId>;

        // Context records the exact source location while validation walks the
        // module. Keeping diagnostics as data (rather than printing here) lets
        // the CLI, tests and embedding tools choose their own presentation
        // without weakening the verifier.
        struct Context final
        {
            std::vector<VerificationIssue> issues;
            core::SymbolId function{};
            xmm::BlockId block{};
            std::size_t instruction{};

            void
            add(const IssueKind kind,
                const llvm::StringRef code,
                const llvm::Twine &message)
            {
                llvm::SmallString<160> storage;
                issues.push_back(
                    VerificationIssue{ kind,
                                       code.str(),
                                       message.toStringRef(storage).str(),
                                       function,
                                       block,
                                       instruction });
            }
        };

        [[nodiscard]] auto
        SupportedType(const core::Type &type) -> bool
        {
            // Named and polymorphic types must be resolved before Xmm reaches
            // LLVM. Guessing a layout here would turn an incomplete front-end
            // decision into a platform ABI.
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
                    return !type.components.empty()
                           && std::ranges::all_of(
                               type.components,
                               [](const core::Type &component) {
                                   return SupportedType(component);
                               });
                case core::Type::Kind::Named:
                    return std::ranges::all_of(
                        type.templateArguments,
                        [](const auto &argument) {
                            return argument.kind
                                       == core::TemplateArgument::Kind::Value
                                   || (argument.type
                                       && SupportedType(*argument.type));
                        });
                case core::Type::Kind::TypeVariable:
                    return false;
            }
            return false;
        }

        [[nodiscard]] auto
        TypeName(const core::Type &type) -> llvm::StringRef
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
                case xmm::Opcode::BitwiseNot:
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
                case xmm::Opcode::Power:
                case xmm::Opcode::ShiftLeft:
                case xmm::Opcode::ShiftRight:
                case xmm::Opcode::BitwiseAnd:
                case xmm::Opcode::BitwiseXor:
                case xmm::Opcode::BitwiseOr:
                case xmm::Opcode::TypeIs:
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
            return opcode >= xmm::Opcode::RetainStrong
                   && opcode <= xmm::Opcode::ReleaseUnowned;
        }

        void
        VerifyLiteral(Context &context, const xmm::Value &value)
        {
            if (value.kind != xmm::Value::Kind::Immediate)
                return;
            if (const auto issue
                = core::validate_literal(value.immediate, value.type))
                context.add(IssueKind::InvalidLiteral,
                            "VXL1018",
                            "immediate payload is invalid: " + *issue);
        }

        void
        VerifyValue(Context &context,
                    const xmm::Value &value,
                    const RegisterCatalog &registers,
                    const FunctionCatalog &functions)
        {
            if (!SupportedType(value.type))
                context.add(IssueKind::UnsupportedType,
                            "VXL1005",
                            llvm::Twine("LLVM lowering does not support ")
                                + TypeName(value.type) + " yet");
            if (value.kind == xmm::Value::Kind::Register)
            {
                const auto *found = registers.Find(value.reg);
                if (value.reg == 0 || found == nullptr)
                    context.add(IssueKind::UndefinedRegister,
                                "VXL1011",
                                "operand reads an undefined virtual register");
                else if (*found != value.type)
                    context.add(
                        IssueKind::OperandType,
                        "VXL1012",
                        "register operand type disagrees with its definition");
            }
            else if (value.kind == xmm::Value::Kind::Function)
            {
                const auto *found = functions.Find(value.symbol);
                if (value.symbol == 0 || found == nullptr)
                    context.add(
                        IssueKind::InvalidCall,
                        "VXL1013",
                        "call operand refers to an unknown function symbol");
                if (value.type.kind != core::Type::Kind::Function)
                    context.add(IssueKind::OperandType,
                                "VXL1014",
                                "function operand must carry a function type");
            }
            else
                VerifyLiteral(context, value);
        }

        void
        VerifyInstruction(Context &context,
                          const xmm::Instruction &instruction,
                          const RegisterCatalog &registers,
                          const FunctionCatalog &functions)
        {
            if (!SupportedType(instruction.result_type))
                context.add(IssueKind::UnsupportedType,
                            "VXL1005",
                            "instruction result has an unsupported LLVM type");
            for (const auto &operand : instruction.operands)
                VerifyValue(context, operand, registers, functions);

            if (instruction.opcode == xmm::Opcode::Call)
            {
                // A function type stores parameters followed by its result. The
                // instruction stores either a direct Function identity or a
                // Register-backed closure first. Capture parameters remain
                // private to the closure thunk and never appear at an ordinary
                // call site.
                if (instruction.operands.empty()
                    || (instruction.operands.front().kind
                            != xmm::Value::Kind::Function
                        && instruction.operands.front().kind
                               != xmm::Value::Kind::Register))
                    context.add(IssueKind::InvalidCall,
                                "VXL1015",
                                "call must begin with a direct function or "
                                "closure register");
                else
                {
                    const auto signature
                        = ::Visual::XSharp::Core::Callable::Decompose(
                            instruction.operands.front().type);
                    if (!signature)
                        context.add(
                            IssueKind::InvalidCall,
                            "VXL1041",
                            "call operand does not carry a callable type");
                    else if (instruction.operands.size()
                             != signature->parameters.size() + 1U)
                        context.add(
                            IssueKind::OperandCount,
                            "VXL1016",
                            "call argument count does not match its signature");
                    else
                    {
                        for (std::size_t index = 1;
                             index < instruction.operands.size();
                             ++index)
                            if (instruction.operands[index].type
                                != signature->parameters[index - 1U])
                                context.add(IssueKind::OperandType,
                                            "VXL1017",
                                            "call argument type does not match "
                                            "its signature");
                        if (instruction.result_type != signature->result)
                            context.add(IssueKind::ResultType,
                                        "VXL1019",
                                        "call result type does not match its "
                                        "signature");
                    }
                }
            }
            else if (instruction.opcode == xmm::Opcode::MakeClosure)
            {
                const auto *target
                    = functions.Find(instruction.closure_function);
                if (instruction.closure_function == 0 || target == nullptr)
                    context.add(IssueKind::InvalidCall,
                                "VXL1032",
                                "closure operation refers to an unknown lifted "
                                "function");
                if (instruction.result_type.kind != core::Type::Kind::Function)
                    context.add(IssueKind::ResultType,
                                "VXL1033",
                                "closure operation result must be callable");
                if (instruction.capture_modes.size()
                    != instruction.operands.size())
                    context.add(
                        IssueKind::OperandCount,
                        "VXL1034",
                        "closure capture modes and operands differ in length");
                const auto pairedCaptures
                    = std::min(instruction.capture_modes.size(),
                               instruction.operands.size());
                for (std::size_t index = 0U; index < pairedCaptures; ++index)
                    if (instruction.capture_modes[index]
                            != core::CaptureMode::Strong
                        && !IsAarcType(instruction.operands[index].type))
                        context.add(IssueKind::OperandType,
                                    "VXL1051",
                                    "weak and unowned closure captures require "
                                    "an AARC reference type");
                if (target != nullptr)
                {
                    const auto &types = (*target)->parameter_types;
                    llvm::SmallVector<core::Type, 8> captures;
                    captures.reserve(instruction.operands.size());
                    for (const auto &operand : instruction.operands)
                        captures.push_back(operand.type);
                    const auto contract
                        = ::Visual::XSharp::Core::Callable::ValidateClosure(
                            captures,
                            types,
                            (*target)->return_type,
                            instruction.result_type);
                    using ContractError = ::Visual::XSharp::Core::Callable::
                        ClosureContractError;
                    switch (contract.error)
                    {
                        case ContractError::None:
                            break;
                        case ContractError::ResultIsNotCallable:
                            // VXL1033 already reports the public result shape.
                            break;
                        case ContractError::TargetHasTooFewParameters:
                            context.add(IssueKind::ParameterShape,
                                        "VXL1035",
                                        "lifted function has fewer parameters "
                                        "than closure captures");
                            break;
                        case ContractError::CaptureTypeMismatch:
                            context.add(IssueKind::OperandType,
                                        "VXL1036",
                                        "closure capture type differs from its "
                                        "lifted parameter");
                            break;
                        case ContractError::PublicParameterCountMismatch:
                            context.add(
                                IssueKind::ParameterShape,
                                "VXL1042",
                                "lifted function public parameter count "
                                "differs from the closure signature");
                            break;
                        case ContractError::PublicParameterTypeMismatch:
                            context.add(IssueKind::OperandType,
                                        "VXL1043",
                                        "lifted function public parameter type "
                                        "differs from the closure signature");
                            break;
                        case ContractError::ResultTypeMismatch:
                            context.add(IssueKind::ResultType,
                                        "VXL1044",
                                        "lifted function result differs from "
                                        "the closure signature");
                            break;
                    }
                }
            }
            else if (IsOwnershipOpcode(instruction.opcode))
            {
                if (instruction.operands.size() != 1U)
                    context.add(
                        IssueKind::OperandCount,
                        "VXL1037",
                        "ownership instruction requires exactly one operand");
                else if (!IsAarcType(instruction.operands.front().type))
                    context.add(IssueKind::OperandType,
                                "VXL1038",
                                "ownership instruction requires an AARC "
                                "reference type");

                const auto releases
                    = instruction.opcode == xmm::Opcode::ReleaseStrong
                      || instruction.opcode == xmm::Opcode::ReleaseWeak
                      || instruction.opcode == xmm::Opcode::ReleaseUnowned;
                if (releases)
                {
                    if (instruction.has_result
                        || instruction.result_type.kind
                               != core::Type::Kind::Unit)
                        context.add(IssueKind::ResultType,
                                    "VXL1039",
                                    "release ownership instruction must have "
                                    "no result");
                }
                else if (!instruction.has_result || instruction.operands.empty()
                         || instruction.result_type
                                != instruction.operands.front().type)
                    context.add(IssueKind::ResultType,
                                "VXL1040",
                                "producing ownership instruction must preserve "
                                "its operand type");
            }
            else
            {
                const auto expected = ExpectedOperandCount(instruction.opcode);
                if (instruction.operands.size() != expected)
                    context.add(IssueKind::OperandCount,
                                "VXL1020",
                                "instruction has the wrong operand count");
                if (instruction.opcode == xmm::Opcode::Move
                    || instruction.opcode == xmm::Opcode::LoadImmediate)
                {
                    if (!instruction.operands.empty()
                        && instruction.operands.front().type
                               != instruction.result_type)
                        context.add(
                            IssueKind::ResultType,
                            "VXL1021",
                            "move result type differs from its operand");
                }
                else if (instruction.opcode == xmm::Opcode::AndBool
                         || instruction.opcode == xmm::Opcode::OrBool
                         || instruction.opcode == xmm::Opcode::NotBool)
                {
                    if (instruction.result_type.kind != core::Type::Kind::Bool
                        || std::ranges::any_of(
                            instruction.operands,
                            [](const xmm::Value &value) {
                                return value.type.kind
                                       != core::Type::Kind::Bool;
                            }))
                        context.add(IssueKind::OperandType,
                                    "VXL1022",
                                    "logical instruction requires Bool "
                                    "operands and result");
                }
                else if (instruction.opcode >= xmm::Opcode::CompareLess
                         && instruction.opcode <= xmm::Opcode::CompareNotEqual)
                {
                    if (instruction.result_type.kind != core::Type::Kind::Bool
                        || (instruction.operands.size() == 2
                            && instruction.operands[0].type
                                   != instruction.operands[1].type))
                        context.add(IssueKind::OperandType,
                                    "VXL1023",
                                    "comparison requires equal operand types "
                                    "and Bool result");
                }
                else if (instruction.opcode == xmm::Opcode::TypeIs)
                {
                    if (instruction.result_type.kind != core::Type::Kind::Bool
                        || instruction.operands.size() != 2U
                        || (instruction.operands[0].type.kind
                                != core::Type::Kind::Named
                            && instruction.operands[0].type.kind
                                   != core::Type::Kind::String
                            && instruction.operands[0].type.kind
                                   != core::Type::Kind::Function)
                        || instruction.operands[1].type != core::Type::uint64())
                        context.add(IssueKind::OperandType,
                                    "VXL1049",
                                    "type test requires a reference subject, "
                                    "uint identity and Bool result");
                }
                else if (instruction.opcode == xmm::Opcode::ShiftLeft
                         || instruction.opcode == xmm::Opcode::ShiftRight
                         || instruction.opcode == xmm::Opcode::BitwiseAnd
                         || instruction.opcode == xmm::Opcode::BitwiseXor
                         || instruction.opcode == xmm::Opcode::BitwiseOr
                         || instruction.opcode == xmm::Opcode::BitwiseNot)
                {
                    if (!core::is_integer(instruction.result_type)
                        || std::ranges::any_of(
                            instruction.operands,
                            [&instruction](const xmm::Value &value) {
                                return value.type != instruction.result_type;
                            }))
                        context.add(IssueKind::OperandType,
                                    "VXL1050",
                                    "bitwise instruction operands and result "
                                    "must use one integer type");
                }
                else if (instruction.opcode == xmm::Opcode::FloorDivide)
                {
                    const auto hasNumericPair
                        = instruction.operands.size() == 2U
                          && core::is_numeric(instruction.operands[0].type)
                          && instruction.operands[0].type
                                 == instruction.operands[1].type;
                    const auto expectedResult
                        = !hasNumericPair ? core::Type::unit()
                          : core::is_floating(instruction.operands[0].type)
                              ? core::Type::int64()
                              : instruction.operands[0].type;
                    if (!hasNumericPair
                        || instruction.result_type != expectedResult)
                        context.add(
                            IssueKind::OperandType,
                            "VXL1052",
                            "rounded division requires matching numeric "
                            "operands and its specified result type");
                }
                else if (instruction.opcode != xmm::Opcode::Call)
                {
                    if (!core::is_numeric(instruction.result_type)
                        || std::ranges::any_of(
                            instruction.operands,
                            [&instruction](const xmm::Value &value) {
                                return value.type != instruction.result_type;
                            }))
                        context.add(IssueKind::OperandType,
                                    "VXL1024",
                                    "numeric instruction operand and result "
                                    "types must agree");
                }
            }

            if (instruction.has_result)
            {
                if (instruction.destination == 0)
                    context.add(
                        IssueKind::InvalidFunction,
                        "VXL1025",
                        "result-producing instruction has register zero");
                else if (registers.Find(instruction.destination) == nullptr)
                    context.add(IssueKind::InvalidFunction,
                                "VXL1025",
                                "result destination is absent from the "
                                "register catalog");
            }
            else if (instruction.result_type.kind != core::Type::Kind::Unit
                     && instruction.opcode != xmm::Opcode::Call
                     && instruction.opcode != xmm::Opcode::MakeClosure)
                context.add(
                    IssueKind::ResultType,
                    "VXL1027",
                    "discarded non-call instruction must have Unit result");
        }

        void
        VerifyTerminator(Context &context,
                         const xmm::Terminator &terminator,
                         const xmm::Function &function,
                         const RegisterCatalog &registers,
                         const FunctionCatalog &functions,
                         const BlockCatalog &blocks)
        {
            if (terminator.kind == xmm::Terminator::Kind::Return)
            {
                VerifyValue(context, terminator.value, registers, functions);
                if (terminator.value.type != function.return_type)
                    context.add(IssueKind::InvalidReturn,
                                "VXL1028",
                                "return value type differs from the function "
                                "result type");
            }
            else if (terminator.kind == xmm::Terminator::Kind::Branch)
            {
                VerifyValue(context, terminator.value, registers, functions);
                if (terminator.value.type.kind != core::Type::Kind::Bool)
                    context.add(IssueKind::InvalidBranch,
                                "VXL1029",
                                "branch condition must be Bool");
                if (!blocks.Contains(terminator.true_target)
                    || !blocks.Contains(terminator.false_target))
                    context.add(IssueKind::InvalidTarget,
                                "VXL1030",
                                "branch target does not name a function block");
            }
            else if (terminator.kind == xmm::Terminator::Kind::Jump
                     && !blocks.Contains(terminator.true_target))
                context.add(IssueKind::InvalidTarget,
                            "VXL1031",
                            "jump target does not name a function block");
        }

        [[nodiscard]] auto
        Successors(const xmm::Terminator &terminator)
            -> std::vector<dataflow::BlockId>
        {
            switch (terminator.kind)
            {
                case xmm::Terminator::Kind::Branch:
                    return { terminator.true_target, terminator.false_target };
                case xmm::Terminator::Kind::Jump:
                    return { terminator.true_target };
                case xmm::Terminator::Kind::Return:
                case xmm::Terminator::Kind::Unreachable:
                    return {};
            }
            return {};
        }

        void
        AppendRegisterRead(const xmm::Value &value,
                           const RegisterCatalog &registers,
                           std::vector<dataflow::StorageId> &reads)
        {
            if (value.kind == xmm::Value::Kind::Register
                && registers.Contains(value.reg))
                reads.push_back(value.reg);
        }

        [[nodiscard]] auto
        DataflowFunction(const xmm::Function &function,
                         const RegisterCatalog &registers) -> dataflow::Function
        {
            dataflow::Function model;
            model.entry = function.entry;
            model.declarations.reserve(registers.Size());
            registers.ForEach([&model](const xmm::VirtualRegister reg,
                                       const core::Type &type) {
                static_cast<void>(type);
                model.declarations.push_back(reg);
            });
            std::ranges::sort(model.declarations);
            model.initiallyInitialized.reserve(
                function.parameter_registers.size());
            for (const auto reg : function.parameter_registers)
                if (registers.Contains(reg))
                    model.initiallyInitialized.push_back(reg);

            model.blocks.reserve(function.blocks.size());
            for (const auto &block : function.blocks)
            {
                dataflow::Block flowBlock;
                flowBlock.id = block.id;
                flowBlock.successors = Successors(block.terminator);
                flowBlock.accesses.reserve(block.instructions.size() + 1U);
                for (std::size_t index = 0; index < block.instructions.size();
                     ++index)
                {
                    const auto &instruction = block.instructions[index];
                    dataflow::AccessPoint access;
                    access.instruction = index;
                    for (const auto &operand : instruction.operands)
                        AppendRegisterRead(operand, registers, access.reads);
                    if (instruction.has_result
                        && registers.Contains(instruction.destination))
                        access.write = instruction.destination;
                    flowBlock.accesses.push_back(std::move(access));
                }

                dataflow::AccessPoint terminatorAccess;
                terminatorAccess.instruction = block.instructions.size();
                terminatorAccess.terminator = true;
                if (block.terminator.kind == xmm::Terminator::Kind::Return
                    || block.terminator.kind == xmm::Terminator::Kind::Branch)
                    AppendRegisterRead(block.terminator.value,
                                       registers,
                                       terminatorAccess.reads);
                flowBlock.accesses.push_back(std::move(terminatorAccess));
                model.blocks.push_back(std::move(flowBlock));
            }
            return model;
        }

        void
        VerifyDefiniteInitialization(Context &context,
                                     const xmm::Function &function,
                                     const RegisterCatalog &registers)
        {
            const auto result
                = dataflow::Analyze(DataflowFunction(function, registers),
                                    { .materializeFacts = false });
            for (const auto &issue : result.issues)
            {
                if (issue.kind != dataflow::IssueKind::ReadBeforeInitialization)
                    continue;
                context.block = issue.block;
                context.instruction = issue.instruction;
                context.add(IssueKind::UninitializedRegister,
                            "VXL1045",
                            issue.terminator
                                ? "terminator reads a virtual register that is "
                                  "not initialized on every incoming path"
                                : "instruction reads a virtual register that "
                                  "is not initialized on every incoming path");
            }
        }
    } // namespace

    auto
    Verify(const xmm::Module &module) -> std::vector<VerificationIssue>
    {
        Context context;
        if (module.functions.empty())
            context.add(IssueKind::EmptyModule,
                        "VXL1001",
                        "Xmm module contains no functions");
        if (module.name.empty()
            || std::ranges::any_of(module.name, [](const std::u32string &part) {
                   return part.empty();
               }))
            context.add(IssueKind::InvalidModuleName,
                        "VXL1002",
                        "Xmm module name must contain non-empty components");

        FunctionCatalog functions;
        functions.Reserve(module.functions.size());
        // Build the complete symbol catalog before checking bodies. Forward
        // calls and recursion then validate exactly like calls to functions
        // declared earlier.
        for (const auto &function : module.functions)
        {
            // Parameter types seed the register storage table. Instruction
            // results may add registers or rewrite them with the same type;
            // every later read is checked against this table before LLVM
            // lowering assumes a matching slot exists.
            if (function.symbol.id == 0 || function.symbol.spelling.empty())
            {
                context.function = function.symbol.id;
                context.add(IssueKind::InvalidFunction,
                            "VXL1003",
                            "function requires a non-zero symbol and spelling");
            }
            if (!functions.TryEmplace(function.symbol.id, &function).inserted)
            {
                context.function = function.symbol.id;
                context.add(IssueKind::DuplicateFunction,
                            "VXL1004",
                            "function symbol is declared more than once");
            }
        }

        for (const auto &function : module.functions)
        {
            context.function = function.symbol.id;
            context.block = 0;
            context.instruction = 0;
            if (!SupportedType(function.return_type))
                context.add(IssueKind::UnsupportedType,
                            "VXL1005",
                            "function result type is not lowerable to LLVM");
            if (function.parameter_registers.size()
                != function.parameter_types.size())
                context.add(
                    IssueKind::ParameterShape,
                    "VXL1006",
                    "parameter registers and parameter types differ in length");

            RegisterCatalog registers;
            registers.Reserve(function.parameter_registers.size());
            const auto parameterCount
                = std::min(function.parameter_registers.size(),
                           function.parameter_types.size());
            for (std::size_t index = 0; index < parameterCount; ++index)
            {
                if (function.parameter_registers[index] == 0
                    || !registers
                            .TryEmplace(function.parameter_registers[index],
                                        function.parameter_types[index])
                            .inserted)
                    context.add(IssueKind::ParameterShape,
                                "VXL1007",
                                "parameter virtual registers must be unique "
                                "and non-zero");
                if (!SupportedType(function.parameter_types[index]))
                    context.add(IssueKind::UnsupportedType,
                                "VXL1005",
                                "parameter type is not lowerable to LLVM");
            }

            // Establish storage types independently of block presentation
            // order. Definite initialization is checked separately against CFG
            // paths; this catalog exists only to validate register identity and
            // type.
            for (const auto &block : function.blocks)
                for (const auto &instruction : block.instructions)
                    if (instruction.has_result && instruction.destination != 0)
                    {
                        const auto [found, inserted]
                            = registers.TryEmplace(instruction.destination,
                                                   instruction.result_type);
                        if (!inserted && *found != instruction.result_type)
                        {
                            context.block = block.id;
                            context.add(
                                IssueKind::RegisterRedefinition,
                                "VXL1026",
                                "virtual register is written with a type that "
                                "differs from its established storage type");
                        }
                    }

            BlockCatalog blocks;
            blocks.Reserve(function.blocks.size());
            for (const auto &block : function.blocks)
                if (!blocks.Insert(block.id))
                {
                    context.block = block.id;
                    context.add(IssueKind::DuplicateBlock,
                                "VXL1008",
                                "block id is declared more than once");
                }
            if (!blocks.Contains(function.entry))
                context.add(IssueKind::MissingEntry,
                            "VXL1009",
                            "function entry does not name a block");

            for (const auto &block : function.blocks)
            {
                context.block = block.id;
                for (std::size_t index = 0; index < block.instructions.size();
                     ++index)
                {
                    context.instruction = index;
                    VerifyInstruction(context,
                                      block.instructions[index],
                                      registers,
                                      functions);
                }
                context.instruction = block.instructions.size();
                VerifyTerminator(context,
                                 block.terminator,
                                 function,
                                 registers,
                                 functions,
                                 blocks);
            }
            VerifyDefiniteInitialization(context, function, registers);
            auto ownershipIssues = VerifyOwnership(function);
            context.issues.insert(
                context.issues.end(),
                std::make_move_iterator(ownershipIssues.begin()),
                std::make_move_iterator(ownershipIssues.end()));
        }
        return context.issues;
    }
} // namespace Visual::XSharp::Xmm
