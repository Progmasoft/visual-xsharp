// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
#pragma once

#include "Visual/XSharp/Xmm/IR.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Visual::XSharp::Xmm
{
enum class IssueKind : std::uint8_t
{
    EmptyModule,
    InvalidModuleName,
    DuplicateFunction,
    InvalidFunction,
    UnsupportedType,
    ParameterShape,
    DuplicateBlock,
    MissingEntry,
    InvalidTarget,
    RegisterRedefinition,
    UndefinedRegister,
    OperandCount,
    OperandType,
    ResultType,
    InvalidCall,
    InvalidReturn,
    InvalidBranch,
    InvalidLiteral
};

struct VerificationIssue final
{
    IssueKind kind{IssueKind::InvalidFunction};
    std::string code;
    std::string message;
    ::visual_xsharp::core::SymbolId function{};
    ::visual_xsharp::xmm::BlockId block{};
    std::size_t instruction{};

    [[nodiscard]] auto operator==(const VerificationIssue &) const -> bool = default;
};

[[nodiscard]] auto Verify(const ::visual_xsharp::xmm::Module &module) -> std::vector<VerificationIssue>;
} // namespace Visual::XSharp::Xmm
