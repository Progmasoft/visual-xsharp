// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

#include "Visual/XSharp/Core/CorePrep/Verifier/Semantics.hpp"

#include <algorithm>
#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace visual_xsharp::core
{
namespace
{
struct Definition final
{
    Type type;
    bool mutable_binding{};
    bool callable{};
};

using Definitions = std::unordered_map<SymbolId, Definition>;

auto issue(std::string code, std::string message, const Function &function, BlockId block) -> VerificationIssue
{
    return VerificationIssue{std::move(code), std::move(message), function.symbol.id, block};
}

auto function_type(const Function &function) -> Type
{
    std::vector<Type> parameters;
    parameters.reserve(function.parameters.size());
    for(const auto &parameter : function.parameters)
        parameters.push_back(parameter.type);
    return Type::function(std::move(parameters), function.return_type);
}

void verify_symbol_spelling(const SymbolName &symbol, const Function &function, BlockId block,
                            std::unordered_map<SymbolId, std::u32string> &spellings,
                            std::vector<VerificationIssue> &issues)
{
    if(symbol.id == 0)
        return;
    if(symbol.spelling.empty())
        return;
    const auto [found, inserted] = spellings.emplace(symbol.id, symbol.spelling);
    if(!inserted && !found->second.empty() && found->second != symbol.spelling)
        issues.push_back(issue("VXC1014", "one symbol id carries conflicting spellings", function, block));
}

void verify_type(const Type &type, const Function &function, BlockId block, std::size_t depth,
                 std::unordered_map<SymbolId, std::u32string> &spellings, std::vector<VerificationIssue> &issues)
{
    if(depth > 128U)
    {
        issues.push_back(issue("VXC1015", "type nesting exceeds the native verifier limit", function, block));
        return;
    }
    switch(type.kind)
    {
    case Type::Kind::Unit:
    case Type::Kind::Bool:
    case Type::Kind::Int64:
    case Type::Kind::Int32:
    case Type::Kind::String:
        if(!type.name.empty() || !type.components.empty() || type.variable.id != 0)
            issues.push_back(issue("VXC1016", "primitive type contains unexpected payload", function, block));
        return;
    case Type::Kind::Function:
        if(type.components.empty())
            issues.push_back(issue("VXC1017", "function type has no result component", function, block));
        for(const auto &component : type.components)
            verify_type(component, function, block, depth + 1U, spellings, issues);
        return;
    case Type::Kind::Named:
        if(type.name.empty())
            issues.push_back(issue("VXC1018", "named type has an empty qualified name", function, block));
        for(const auto &part : type.name)
            if(part.empty())
                issues.push_back(issue("VXC1019", "named type contains an empty name part", function, block));
        for(const auto &argument : type.components)
            verify_type(argument, function, block, depth + 1U, spellings, issues);
        return;
    case Type::Kind::TypeVariable:
        verify_symbol_spelling(type.variable, function, block, spellings, issues);
        if(type.variable.id == 0)
            issues.push_back(issue("VXC1020", "type variable has no symbol", function, block));
        return;
    }
}

auto is_integer(const Type &type) -> bool
{
    return type.kind == Type::Kind::Int64 || type.kind == Type::Kind::Int32;
}

void verify_atom(const Atom &atom, const Function &function, BlockId block, const Definitions &definitions,
                 std::unordered_map<SymbolId, std::u32string> &spellings, std::vector<VerificationIssue> &issues)
{
    verify_type(atom.type, function, block, 0, spellings, issues);
    if(atom.kind == Atom::Kind::Variable)
    {
        verify_symbol_spelling(atom.symbol, function, block, spellings, issues);
        const auto found = definitions.find(atom.symbol.id);
        if(found == definitions.end())
            issues.push_back(issue("VXC1021", "atom references an undefined symbol", function, block));
        else if(found->second.type != atom.type)
            issues.push_back(issue("VXC1022", "atom type differs from its symbol definition", function, block));
    }
}

auto expected_primitive_result(Operation operation, const std::vector<Atom> &operands) -> std::optional<Type>
{
    switch(operation)
    {
    case Operation::Copy:
        return operands.empty() ? std::nullopt : std::optional<Type>(operands.front().type);
    case Operation::Call:
        if(operands.empty() || operands.front().type.kind != Type::Kind::Function ||
           operands.front().type.components.empty())
            return std::nullopt;
        return operands.front().type.components.back();
    case Operation::LessThan:
    case Operation::LessEqual:
    case Operation::GreaterThan:
    case Operation::GreaterEqual:
    case Operation::Equal:
    case Operation::NotEqual:
    case Operation::LogicalAnd:
    case Operation::LogicalOr:
    case Operation::LogicalNot:
        return Type::boolean();
    case Operation::Add:
    case Operation::Subtract:
    case Operation::Multiply:
    case Operation::Divide:
    case Operation::FloorDivide:
    case Operation::Remainder:
    case Operation::Negate:
        return operands.empty() ? std::nullopt : std::optional<Type>(operands.front().type);
    }
    return std::nullopt;
}

void verify_operation(const Instruction &instruction, const Function &function, BlockId block,
                      const Definitions &definitions, std::unordered_map<SymbolId, std::u32string> &spellings,
                      std::vector<VerificationIssue> &issues)
{
    for(const auto &operand : instruction.operands)
        verify_atom(operand, function, block, definitions, spellings, issues);

    const auto arity = instruction.operands.size();
    switch(instruction.operation)
    {
    case Operation::Copy:
        if(arity != 1U)
            issues.push_back(issue("VXC1023", "copy requires exactly one operand", function, block));
        break;
    case Operation::Call:
        if(arity == 0U)
        {
            issues.push_back(issue("VXC1024", "call requires a callee operand", function, block));
            break;
        }
        if(instruction.operands.front().type.kind != Type::Kind::Function ||
           instruction.operands.front().type.components.empty())
        {
            issues.push_back(issue("VXC1025", "call callee does not have a function type", function, block));
            break;
        }
        {
            const auto &signature = instruction.operands.front().type.components;
            const auto parameter_count = signature.size() - 1U;
            if(arity - 1U != parameter_count)
                issues.push_back(
                    issue("VXC1026", "call argument count differs from the callee signature", function, block));
            const auto comparable = std::min(parameter_count, arity - 1U);
            for(std::size_t index = 0; index < comparable; ++index)
                if(instruction.operands[index + 1U].type != signature[index])
                    issues.push_back(
                        issue("VXC1027", "call argument type differs from the callee signature", function, block));
        }
        break;
    case Operation::Negate:
        if(arity != 1U || !is_integer(instruction.operands.front().type))
            issues.push_back(issue("VXC1028", "negate requires one integer operand", function, block));
        break;
    case Operation::LogicalNot:
        if(arity != 1U || instruction.operands.front().type.kind != Type::Kind::Bool)
            issues.push_back(issue("VXC1029", "logical not requires one bool operand", function, block));
        break;
    case Operation::LogicalAnd:
    case Operation::LogicalOr:
        if(arity != 2U || instruction.operands.front().type.kind != Type::Kind::Bool ||
           instruction.operands.back().type.kind != Type::Kind::Bool)
            issues.push_back(issue("VXC1030", "logical operation requires two bool operands", function, block));
        break;
    case Operation::Equal:
    case Operation::NotEqual:
        if(arity != 2U || instruction.operands.front().type != instruction.operands.back().type)
            issues.push_back(issue("VXC1031", "equality requires two operands of the same type", function, block));
        break;
    case Operation::LessThan:
    case Operation::LessEqual:
    case Operation::GreaterThan:
    case Operation::GreaterEqual:
        if(arity != 2U || !is_integer(instruction.operands.front().type) ||
           instruction.operands.front().type != instruction.operands.back().type)
            issues.push_back(issue("VXC1032", "ordered comparison requires two equal integer types", function, block));
        break;
    default:
        if(arity != 2U || !is_integer(instruction.operands.front().type) ||
           instruction.operands.front().type != instruction.operands.back().type)
            issues.push_back(
                issue("VXC1033", "arithmetic operation requires two equal integer types", function, block));
        break;
    }

    if(instruction.kind == Instruction::Kind::Bind)
    {
        const auto expected = expected_primitive_result(instruction.operation, instruction.operands);
        if(expected && *expected != instruction.type)
            issues.push_back(issue("VXC1034", "binding type differs from the operation result", function, block));
    }
}

void collect_definitions(const CorePrepModule &module, const Function &function, Definitions &definitions,
                         std::unordered_map<SymbolId, std::u32string> &spellings,
                         std::vector<VerificationIssue> &issues)
{
    for(const auto &candidate : module.functions)
    {
        verify_symbol_spelling(candidate.symbol, function, 0, spellings, issues);
        definitions.emplace(candidate.symbol.id, Definition{function_type(candidate), false, true});
    }
    for(const auto &parameter : function.parameters)
    {
        verify_symbol_spelling(parameter.symbol, function, function.entry, spellings, issues);
        verify_type(parameter.type, function, function.entry, 0, spellings, issues);
        definitions.emplace(parameter.symbol.id, Definition{parameter.type, false, false});
    }
    for(const auto &block : function.blocks)
        for(const auto &instruction : block.instructions)
            if(instruction.kind == Instruction::Kind::Bind)
            {
                verify_symbol_spelling(instruction.destination, function, block.id, spellings, issues);
                verify_type(instruction.type, function, block.id, 0, spellings, issues);
                definitions.emplace(instruction.destination.id,
                                    Definition{instruction.type, instruction.mutable_binding, false});
            }
}

void verify_function(const CorePrepModule &module, const Function &function, std::vector<VerificationIssue> &issues)
{
    Definitions definitions;
    std::unordered_map<SymbolId, std::u32string> spellings;
    verify_symbol_spelling(function.symbol, function, function.entry, spellings, issues);
    verify_type(function.return_type, function, function.entry, 0, spellings, issues);
    collect_definitions(module, function, definitions, spellings, issues);

    for(const auto &block : function.blocks)
    {
        for(const auto &instruction : block.instructions)
        {
            if(instruction.kind == Instruction::Kind::Assign)
            {
                verify_symbol_spelling(instruction.destination, function, block.id, spellings, issues);
                const auto target = definitions.find(instruction.destination.id);
                if(target == definitions.end())
                    issues.push_back(issue("VXC1035", "assignment targets an undefined symbol", function, block.id));
                else if(!target->second.mutable_binding)
                    issues.push_back(issue("VXC1036", "assignment targets an immutable symbol", function, block.id));
                if(instruction.operands.size() == 1U && target != definitions.end() &&
                   target->second.type != instruction.operands.front().type)
                    issues.push_back(
                        issue("VXC1037", "assignment value type differs from its target", function, block.id));
            }
            verify_operation(instruction, function, block.id, definitions, spellings, issues);
        }

        switch(block.terminator.kind)
        {
        case Terminator::Kind::Return:
            verify_atom(block.terminator.value, function, block.id, definitions, spellings, issues);
            if(block.terminator.value.type != function.return_type)
                issues.push_back(
                    issue("VXC1038", "return atom type differs from the function result", function, block.id));
            break;
        case Terminator::Kind::Branch:
            verify_atom(block.terminator.value, function, block.id, definitions, spellings, issues);
            break;
        case Terminator::Kind::Jump:
        case Terminator::Kind::Unreachable:
            break;
        }
    }
}
} // namespace

auto verify_semantics(const CorePrepModule &module) -> std::vector<VerificationIssue>
{
    std::vector<VerificationIssue> issues;
    for(const auto &function : module.functions)
        verify_function(module, function, issues);
    return issues;
}
} // namespace visual_xsharp::core
