// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

#include "Visual/XSharp/Backend/LLVM.hpp"

#include <algorithm>
#include <array>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace Visual::XSharp::Backend::LLVM
{
using namespace ::visual_xsharp;
namespace
{
// Context records the exact source location while validation walks the module. Keeping
// diagnostics as data (rather than printing here) lets the CLI, tests and embedding
// tools choose their own presentation without weakening the verifier.
struct Context final
{
    std::vector<Issue> issues;
    core::SymbolId function{};
    xmm::BlockId block{};
    std::size_t instruction{};

    void add(IssueKind kind, std::string code, std::string message)
    {
        issues.push_back(Issue{kind, std::move(code), std::move(message), function, block, instruction});
    }
};

[[nodiscard]] auto SupportedType(const core::Type &type) -> bool
{
    // Named and polymorphic types must be resolved before Xmm reaches LLVM. Guessing a
    // layout here would turn an incomplete front-end decision into a platform ABI.
    switch(type.kind)
    {
    case core::Type::Kind::Unit:
    case core::Type::Kind::Bool:
    case core::Type::Kind::Int64:
    case core::Type::Kind::Int32:
    case core::Type::Kind::String:
        return true;
    case core::Type::Kind::Function:
        return !type.components.empty() &&
               std::ranges::all_of(type.components, [](const core::Type &component) { return SupportedType(component); });
    case core::Type::Kind::Named:
    case core::Type::Kind::TypeVariable:
        return false;
    }
    return false;
}

[[nodiscard]] auto TypeName(const core::Type &type) -> std::string_view
{
    switch(type.kind)
    {
    case core::Type::Kind::Unit: return "Unit";
    case core::Type::Kind::Bool: return "Bool";
    case core::Type::Kind::Int64: return "Long";
    case core::Type::Kind::Int32: return "Int";
    case core::Type::Kind::String: return "String";
    case core::Type::Kind::Function: return "function";
    case core::Type::Kind::Named: return "named type";
    case core::Type::Kind::TypeVariable: return "type variable";
    }
    return "unknown";
}

[[nodiscard]] auto IsInteger(const core::Type &type) -> bool
{
    return type.kind == core::Type::Kind::Int64 || type.kind == core::Type::Kind::Int32;
}

[[nodiscard]] auto ExpectedOperandCount(xmm::Opcode opcode) -> std::size_t
{
    switch(opcode)
    {
    case xmm::Opcode::LoadImmediate:
    case xmm::Opcode::Move:
    case xmm::Opcode::NegateI64:
    case xmm::Opcode::NotBool:
        return 1;
    case xmm::Opcode::AddI64:
    case xmm::Opcode::SubI64:
    case xmm::Opcode::MulI64:
    case xmm::Opcode::DivI64:
    case xmm::Opcode::FloorDivI64:
    case xmm::Opcode::RemI64:
    case xmm::Opcode::CompareLessI64:
    case xmm::Opcode::CompareLessEqualI64:
    case xmm::Opcode::CompareGreaterI64:
    case xmm::Opcode::CompareGreaterEqualI64:
    case xmm::Opcode::CompareEqual:
    case xmm::Opcode::CompareNotEqual:
    case xmm::Opcode::AndBool:
    case xmm::Opcode::OrBool:
        return 2;
    case xmm::Opcode::Call:
        return 0;
    }
    return 0;
}

void VerifyLiteral(Context &context, const xmm::Value &value)
{
    if(value.kind != xmm::Value::Kind::Immediate)
        return;
    const bool matches =
        (value.type.kind == core::Type::Kind::Unit && std::holds_alternative<std::monostate>(value.immediate)) ||
        (value.type.kind == core::Type::Kind::Bool && std::holds_alternative<bool>(value.immediate)) ||
        (value.type.kind == core::Type::Kind::Int64 && std::holds_alternative<std::int64_t>(value.immediate)) ||
        (value.type.kind == core::Type::Kind::Int32 && std::holds_alternative<std::int32_t>(value.immediate)) ||
        (value.type.kind == core::Type::Kind::String && std::holds_alternative<std::u32string>(value.immediate));
    if(!matches)
        context.add(IssueKind::InvalidLiteral, "VXL1018", "immediate payload does not match its declared type");
}

void VerifyValue(Context &context, const xmm::Value &value,
                  const std::unordered_map<xmm::VirtualRegister, core::Type> &registers,
                  const std::unordered_map<core::SymbolId, const xmm::Function *> &functions)
{
    if(!SupportedType(value.type))
        context.add(IssueKind::UnsupportedType, "VXL1005",
                    "LLVM lowering does not support " + std::string(TypeName(value.type)) + " yet");
    if(value.kind == xmm::Value::Kind::Register)
    {
        const auto found = registers.find(value.reg);
        if(value.reg == 0 || found == registers.end())
            context.add(IssueKind::UndefinedRegister, "VXL1011", "operand reads an undefined virtual register");
        else if(found->second != value.type)
            context.add(IssueKind::OperandType, "VXL1012", "register operand type disagrees with its definition");
    }
    else if(value.kind == xmm::Value::Kind::Function)
    {
        const auto found = functions.find(value.symbol);
        if(value.symbol == 0 || found == functions.end())
            context.add(IssueKind::InvalidCall, "VXL1013", "call operand refers to an unknown function symbol");
        if(value.type.kind != core::Type::Kind::Function)
            context.add(IssueKind::OperandType, "VXL1014", "function operand must carry a function type");
    }
    else
        VerifyLiteral(context, value);
}

void VerifyInstruction(Context &context, const xmm::Instruction &instruction,
                        std::unordered_map<xmm::VirtualRegister, core::Type> &registers,
                        const std::unordered_map<core::SymbolId, const xmm::Function *> &functions)
{
    if(!SupportedType(instruction.result_type))
        context.add(IssueKind::UnsupportedType, "VXL1005", "instruction result has an unsupported LLVM type");
    for(const auto &operand : instruction.operands)
        VerifyValue(context, operand, registers, functions);

    if(instruction.opcode == xmm::Opcode::Call)
    {
        // A function type stores parameters followed by its result. The instruction
        // stores the function operand first, followed by actual arguments; therefore
        // operand count equals signature component count even though their roles differ.
        if(instruction.operands.empty() || instruction.operands.front().kind != xmm::Value::Kind::Function)
            context.add(IssueKind::InvalidCall, "VXL1015", "call must begin with a function-symbol operand");
        else if(instruction.operands.front().type.kind == core::Type::Kind::Function)
        {
            const auto &signature = instruction.operands.front().type.components;
            if(signature.empty() || instruction.operands.size() != signature.size())
                context.add(IssueKind::OperandCount, "VXL1016", "call argument count does not match its signature");
            else
            {
                for(std::size_t index = 1; index < instruction.operands.size(); ++index)
                    if(instruction.operands[index].type != signature[index - 1])
                        context.add(IssueKind::OperandType, "VXL1017", "call argument type does not match its signature");
                if(instruction.result_type != signature.back())
                    context.add(IssueKind::ResultType, "VXL1019", "call result type does not match its signature");
            }
        }
    }
    else
    {
        const auto expected = ExpectedOperandCount(instruction.opcode);
        if(instruction.operands.size() != expected)
            context.add(IssueKind::OperandCount, "VXL1020", "instruction has the wrong operand count");
        if(instruction.opcode == xmm::Opcode::Move || instruction.opcode == xmm::Opcode::LoadImmediate)
        {
            if(!instruction.operands.empty() && instruction.operands.front().type != instruction.result_type)
                context.add(IssueKind::ResultType, "VXL1021", "move result type differs from its operand");
        }
        else if(instruction.opcode == xmm::Opcode::AndBool || instruction.opcode == xmm::Opcode::OrBool ||
                instruction.opcode == xmm::Opcode::NotBool)
        {
            if(instruction.result_type.kind != core::Type::Kind::Bool ||
               std::ranges::any_of(instruction.operands, [](const xmm::Value &value) {
                   return value.type.kind != core::Type::Kind::Bool;
               }))
                context.add(IssueKind::OperandType, "VXL1022", "logical instruction requires Bool operands and result");
        }
        else if(instruction.opcode >= xmm::Opcode::CompareLessI64 &&
                instruction.opcode <= xmm::Opcode::CompareNotEqual)
        {
            if(instruction.result_type.kind != core::Type::Kind::Bool ||
               (instruction.operands.size() == 2 && instruction.operands[0].type != instruction.operands[1].type))
                context.add(IssueKind::OperandType, "VXL1023", "comparison requires equal operand types and Bool result");
        }
        else if(instruction.opcode != xmm::Opcode::Call)
        {
            if(!IsInteger(instruction.result_type) ||
               std::ranges::any_of(instruction.operands, [&instruction](const xmm::Value &value) {
                   return value.type != instruction.result_type;
               }))
                context.add(IssueKind::OperandType, "VXL1024", "integer instruction operand and result types must agree");
        }
    }

    if(instruction.has_result)
    {
        if(instruction.destination == 0)
            context.add(IssueKind::InvalidFunction, "VXL1025", "result-producing instruction has register zero");
        else if(const auto [found, inserted] = registers.emplace(instruction.destination, instruction.result_type);
                !inserted && found->second != instruction.result_type)
            // Rewriting a virtual register is legal because Xmm registers are storage.
            // Changing its established type is not: one LLVM alloca cannot safely serve
            // two unrelated layouts on different control-flow paths.
            context.add(IssueKind::RegisterRedefinition, "VXL1026",
                        "virtual register is written with a type that differs from its established storage type");
    }
    else if(instruction.result_type.kind != core::Type::Kind::Unit && instruction.opcode != xmm::Opcode::Call)
        context.add(IssueKind::ResultType, "VXL1027", "discarded non-call instruction must have Unit result");
}

void VerifyTerminator(Context &context, const xmm::Terminator &terminator, const xmm::Function &function,
                       const std::unordered_map<xmm::VirtualRegister, core::Type> &registers,
                       const std::unordered_map<core::SymbolId, const xmm::Function *> &functions,
                       const std::unordered_set<xmm::BlockId> &blocks)
{
    if(terminator.kind == xmm::Terminator::Kind::Return)
    {
        VerifyValue(context, terminator.value, registers, functions);
        if(terminator.value.type != function.return_type)
            context.add(IssueKind::InvalidReturn, "VXL1028", "return value type differs from the function result type");
    }
    else if(terminator.kind == xmm::Terminator::Kind::Branch)
    {
        VerifyValue(context, terminator.value, registers, functions);
        if(terminator.value.type.kind != core::Type::Kind::Bool)
            context.add(IssueKind::InvalidBranch, "VXL1029", "branch condition must be Bool");
        if(!blocks.contains(terminator.true_target) || !blocks.contains(terminator.false_target))
            context.add(IssueKind::InvalidTarget, "VXL1030", "branch target does not name a function block");
    }
    else if(terminator.kind == xmm::Terminator::Kind::Jump && !blocks.contains(terminator.true_target))
        context.add(IssueKind::InvalidTarget, "VXL1031", "jump target does not name a function block");
}
} // namespace

auto Verify(const Xmm::Module &module) -> std::vector<Issue>
{
    Context context;
    if(module.functions.empty())
        context.add(IssueKind::EmptyModule, "VXL1001", "Xmm module contains no functions");
    if(module.name.empty() || std::ranges::any_of(module.name, [](const std::u32string &part) { return part.empty(); }))
        context.add(IssueKind::InvalidModuleName, "VXL1002", "Xmm module name must contain non-empty components");

    std::unordered_map<core::SymbolId, const xmm::Function *> functions;
    // Build the complete symbol catalog before checking bodies. Forward calls and
    // recursion then validate exactly like calls to functions declared earlier.
    for(const auto &function : module.functions)
    {
        // Parameter types seed the register storage table. Instruction results may add
        // registers or rewrite them with the same type; every later read is checked
        // against this table before LLVM lowering assumes a matching slot exists.
        if(function.symbol.id == 0 || function.symbol.spelling.empty())
        {
            context.function = function.symbol.id;
            context.add(IssueKind::InvalidFunction, "VXL1003", "function requires a non-zero symbol and spelling");
        }
        if(!functions.emplace(function.symbol.id, &function).second)
        {
            context.function = function.symbol.id;
            context.add(IssueKind::DuplicateFunction, "VXL1004", "function symbol is declared more than once");
        }
    }

    for(const auto &function : module.functions)
    {
        context.function = function.symbol.id;
        context.block = 0;
        context.instruction = 0;
        if(!SupportedType(function.return_type))
            context.add(IssueKind::UnsupportedType, "VXL1005", "function result type is not lowerable to LLVM");
        if(function.parameter_registers.size() != function.parameter_types.size())
            context.add(IssueKind::ParameterShape, "VXL1006", "parameter registers and parameter types differ in length");

        std::unordered_map<xmm::VirtualRegister, core::Type> registers;
        const auto parameterCount = std::min(function.parameter_registers.size(), function.parameter_types.size());
        for(std::size_t index = 0; index < parameterCount; ++index)
        {
            if(function.parameter_registers[index] == 0 ||
               !registers.emplace(function.parameter_registers[index], function.parameter_types[index]).second)
                context.add(IssueKind::ParameterShape, "VXL1007", "parameter virtual registers must be unique and non-zero");
            if(!SupportedType(function.parameter_types[index]))
                context.add(IssueKind::UnsupportedType, "VXL1005", "parameter type is not lowerable to LLVM");
        }

        std::unordered_set<xmm::BlockId> blocks;
        for(const auto &block : function.blocks)
            if(!blocks.insert(block.id).second)
            {
                context.block = block.id;
                context.add(IssueKind::DuplicateBlock, "VXL1008", "block id is declared more than once");
            }
        if(!blocks.contains(function.entry))
            context.add(IssueKind::MissingEntry, "VXL1009", "function entry does not name a block");

        for(const auto &block : function.blocks)
        {
            context.block = block.id;
            for(std::size_t index = 0; index < block.instructions.size(); ++index)
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
} // namespace Visual::XSharp::Backend::LLVM
