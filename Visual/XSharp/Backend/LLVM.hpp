// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "Visual/XSharp/Xmm/IR.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Visual::XSharp::Backend::LLVM
{
namespace Core = ::visual_xsharp::core;
namespace Xmm = ::visual_xsharp::xmm;
enum class OptimizationLevel : std::uint8_t
{
    Debug,
    Less,
    Default,
    Aggressive
};

struct Options final
{
    OptimizationLevel optimization{OptimizationLevel::Default};
    std::string target_triple;
    bool verify_module{true};
};

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

struct Issue final
{
    IssueKind kind{IssueKind::InvalidFunction};
    std::string code;
    std::string message;
    Core::SymbolId function{};
    Xmm::BlockId block{};
    std::size_t instruction{};
};

enum class ErrorKind : std::uint8_t
{
    InvalidXmm,
    UnsupportedType,
    InvalidUnicode,
    LlvmConstruction,
    LlvmVerification,
    BitcodeEmission,
    FileSystem
};

struct Error final
{
    ErrorKind kind{ErrorKind::LlvmConstruction};
    std::string code;
    std::string message;
    std::vector<Issue> issues;
};

struct Artifact final
{
    std::string llvm_ir;
    std::vector<std::uint8_t> bitcode;
    std::string target_triple;
    std::size_t function_count{};

    [[nodiscard]] auto empty() const noexcept -> bool { return llvm_ir.empty() || bitcode.empty(); }
};

struct Result final
{
    std::optional<Artifact> artifact;
    std::optional<Error> error;

    [[nodiscard]] explicit operator bool() const noexcept { return artifact.has_value(); }
};

[[nodiscard]] auto Verify(const Xmm::Module &module) -> std::vector<Issue>;
[[nodiscard]] auto Lower(const Xmm::Module &module, const Options &options = {}) -> Result;
[[nodiscard]] auto WriteLlvmIr(const std::filesystem::path &path, std::string_view llvmIr)
    -> std::optional<Error>;
[[nodiscard]] auto WriteBitcode(const std::filesystem::path &path, const std::vector<std::uint8_t> &bitcode)
    -> std::optional<Error>;
} // namespace Visual::XSharp::Backend::LLVM
