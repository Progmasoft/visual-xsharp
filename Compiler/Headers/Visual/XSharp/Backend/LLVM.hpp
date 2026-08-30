// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
#pragma once

#include "Visual/XSharp/Xmm/IR.hpp"
#include "Visual/XSharp/Xmm/Verifier.hpp"

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

// This boundary deliberately accepts Xmm rather than an earlier language IR. Xmm has
// already made control flow, storage and call identity explicit, so the backend never
// has to reconstruct source-language meaning or silently invent an ABI decision.
enum class OptimizationLevel : std::uint8_t
{
    Debug,
    Less,
    Default,
    Aggressive
};

enum class MachineCodeEmission : std::uint8_t
{
    // IR and bitcode remain available without initializing target code generation.
    None,
    Object,
    Assembly
};

enum class ObjectFormat : std::uint8_t
{
    Unknown,
    Coff,
    Elf,
    MachO,
    Wasm
};

struct Options final
{
    OptimizationLevel optimization{OptimizationLevel::Default};
    std::string target_triple;
    bool verify_module{true};
    // Machine-code generation is opt-in so `check`, `.ll`, and `.bc` stay
    // target-independent until the driver explicitly asks for native bytes.
    MachineCodeEmission machineCode{MachineCodeEmission::None};
    // Reusable object files must not acquire an accidental process entry symbol.
    // The platform ABI bridge is therefore enabled only for final executables.
    bool executableEntry{};
};

using IssueKind = ::Visual::XSharp::Xmm::IssueKind;
using Issue = ::Visual::XSharp::Xmm::VerificationIssue;

enum class ErrorKind : std::uint8_t
{
    InvalidXmm,
    UnsupportedType,
    InvalidUnicode,
    LlvmConstruction,
    LlvmVerification,
    BitcodeEmission,
    TargetMachine,
    MachineCodeEmission,
    InvalidEntryPoint,
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
    // LLVM objects are owned only while Lower is running. Both representations below
    // are independent copies and therefore remain valid after the LLVM context dies.
    std::string llvm_ir;
    std::vector<std::uint8_t> bitcode;
    // Native payloads remain in memory until an explicit driver writer runs;
    // validation commands consequently cannot leave incidental files behind.
    std::vector<std::uint8_t> object;
    std::string assembly;
    std::string target_triple;
    ObjectFormat objectFormat{ObjectFormat::Unknown};
    std::size_t function_count{};

    [[nodiscard]] auto empty() const noexcept -> bool
    {
        return llvm_ir.empty() || bitcode.empty();
    }
};

struct Result final
{
    std::optional<Artifact> artifact;
    std::optional<Error> error;

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return artifact.has_value();
    }
};

// Verify is public so tools can diagnose an Xmm artifact without constructing LLVM
// state. Lower runs the same verifier again; callers cannot accidentally bypass the
// backend's structural and type-safety boundary.
[[nodiscard]] auto Verify(const Xmm::Module &module) -> std::vector<Issue>;
[[nodiscard]] auto Lower(const Xmm::Module &module, const Options &options = {}) -> Result;
[[nodiscard]] auto WriteLlvmIr(const std::filesystem::path &path, std::string_view llvmIr) -> std::optional<Error>;
[[nodiscard]] auto WriteBitcode(const std::filesystem::path &path, const std::vector<std::uint8_t> &bitcode)
    -> std::optional<Error>;
[[nodiscard]] auto WriteObject(const std::filesystem::path &path, const std::vector<std::uint8_t> &object)
    -> std::optional<Error>;
[[nodiscard]] auto WriteAssembly(const std::filesystem::path &path, std::string_view assembly) -> std::optional<Error>;
} // namespace Visual::XSharp::Backend::LLVM
