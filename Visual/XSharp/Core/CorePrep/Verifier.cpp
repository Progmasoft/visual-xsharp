// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

#include "Visual/XSharp/Core/CorePrep/Verifier.hpp"
#include "Visual/XSharp/Core/CorePrep/Verifier/Semantics.hpp"

#include <algorithm>
#include <unordered_set>

namespace visual_xsharp::core
{
namespace
{
auto issue(std::string code, std::string message, SymbolId function, BlockId block = 0) -> VerificationIssue
{
    return VerificationIssue{std::move(code), std::move(message), function, block};
}

auto atom_valid(const Atom &atom) -> bool
{
    if(atom.kind == Atom::Kind::Variable)
        return atom.symbol.id != 0;
    switch(atom.type.kind)
    {
    case Type::Kind::Unit: return std::holds_alternative<std::monostate>(atom.literal);
    case Type::Kind::Bool: return std::holds_alternative<bool>(atom.literal);
    case Type::Kind::Int64: return std::holds_alternative<std::int64_t>(atom.literal);
    case Type::Kind::Int32: return std::holds_alternative<std::int32_t>(atom.literal);
    case Type::Kind::String: return std::holds_alternative<std::u32string>(atom.literal);
    case Type::Kind::Function:
    case Type::Kind::Named:
    case Type::Kind::TypeVariable: return false;
    }
    return false;
}

auto expected_arity(Operation operation) -> std::size_t
{
    switch(operation)
    {
    case Operation::Copy: return 1;
    case Operation::Call: return 0;
    case Operation::Negate:
    case Operation::LogicalNot: return 1;
    default: return 2;
    }
}

void verify_instruction(const Instruction &instruction, SymbolId function, BlockId block,
                        std::vector<VerificationIssue> &issues)
{
    if(instruction.kind != Instruction::Kind::Evaluate && instruction.destination.id == 0)
        issues.push_back(issue("VXC1006", "defining instruction has no destination symbol", function, block));
    if(instruction.operation != Operation::Call && instruction.operands.size() != expected_arity(instruction.operation))
        issues.push_back(issue("VXC1007", "operation has the wrong operand count", function, block));
    if(instruction.operation == Operation::Call && instruction.operands.empty())
        issues.push_back(issue("VXC1008", "call operation has no callee operand", function, block));
    if(std::any_of(instruction.operands.begin(), instruction.operands.end(), [](const Atom &atom) { return !atom_valid(atom); }))
        issues.push_back(issue("VXC1009", "instruction contains an invalid typed atom", function, block));
}

void verify_terminator(const Terminator &terminator, const std::unordered_set<BlockId> &block_ids, SymbolId function,
                       BlockId block, std::vector<VerificationIssue> &issues)
{
    if(terminator.kind == Terminator::Kind::Return && !atom_valid(terminator.value))
        issues.push_back(issue("VXC1010", "return contains an invalid typed atom", function, block));
    if(terminator.kind == Terminator::Kind::Branch)
    {
        if(!atom_valid(terminator.value) || terminator.value.type.kind != Type::Kind::Bool)
            issues.push_back(issue("VXC1011", "branch condition must be a valid bool atom", function, block));
        if(!block_ids.contains(terminator.true_target) || !block_ids.contains(terminator.false_target))
            issues.push_back(issue("VXC1012", "branch targets a missing block", function, block));
    }
    if(terminator.kind == Terminator::Kind::Jump && !block_ids.contains(terminator.true_target))
        issues.push_back(issue("VXC1013", "jump targets a missing block", function, block));
}
} // namespace

auto verify(const CorePrepModule &module) -> std::vector<VerificationIssue>
{
    std::vector<VerificationIssue> issues;
    std::unordered_set<SymbolId> function_ids;
    for(const auto &function : module.functions)
    {
        if(function.symbol.id == 0 || !function_ids.insert(function.symbol.id).second)
            issues.push_back(issue("VXC1001", "function symbol is missing or duplicated", function.symbol.id));

        std::unordered_set<BlockId> block_ids;
        for(const auto &block : function.blocks)
            if(!block_ids.insert(block.id).second)
            issues.push_back(issue("VXC1002", "block id is duplicated", function.symbol.id, block.id));
        if(!block_ids.contains(function.entry))
            issues.push_back(issue("VXC1003", "entry block does not exist", function.symbol.id));

        std::unordered_set<SymbolId> definitions;
        for(const auto &parameter : function.parameters)
            if(parameter.symbol.id == 0 || !definitions.insert(parameter.symbol.id).second)
                issues.push_back(issue("VXC1004", "parameter symbol is missing or duplicated", function.symbol.id));
        for(const auto &block : function.blocks)
        {
            for(const auto &instruction : block.instructions)
            {
                if(instruction.kind == Instruction::Kind::Bind && !definitions.insert(instruction.destination.id).second)
                    issues.push_back(issue("VXC1005", "binding symbol is defined more than once", function.symbol.id, block.id));
                verify_instruction(instruction, function.symbol.id, block.id, issues);
            }
            verify_terminator(block.terminator, block_ids, function.symbol.id, block.id, issues);
        }
    }
    auto semantic_issues = verify_semantics(module);
    issues.insert(issues.end(), std::make_move_iterator(semantic_issues.begin()),
                  std::make_move_iterator(semantic_issues.end()));
    return issues;
}
} // namespace visual_xsharp::core
