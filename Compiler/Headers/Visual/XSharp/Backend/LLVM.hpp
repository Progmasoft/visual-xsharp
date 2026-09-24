// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "Visual/XSharp/Xmm/IR.hpp"
#include "Visual/XSharp/Xmm/Verifier.hpp"

namespace Visual::XSharp::Backend::LLVM
{
    namespace Core = ::visual_xsharp::core;
    namespace Xmm = ::visual_xsharp::xmm;

    // This boundary deliberately accepts Xmm rather than an earlier language
    // IR. Xmm has already made control flow, storage and call identity
    // explicit, so the backend never has to reconstruct source-language meaning
    // or silently invent an ABI decision.
    enum class OptimizationLevel : std::uint8_t
    {
        Debug,
        Less,
        Default,
        Aggressive
    };

    enum class MachineCodeEmission : std::uint8_t
    {
        // IR and bitcode remain available without initializing target code
        // generation.
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
        OptimizationLevel optimization{ OptimizationLevel::Default };
        std::string target_triple;
        bool verify_module{ true };
        // Machine-code generation is opt-in so `check`, `.ll`, and `.bc` stay
        // target-independent until the driver explicitly asks for native bytes.
        MachineCodeEmission machineCode{ MachineCodeEmission::None };
        // Reusable object files must not acquire an accidental process entry
        // symbol. The platform ABI bridge is therefore enabled only for final
        // executables.
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
        ErrorKind kind{ ErrorKind::LlvmConstruction };
        std::string code;
        std::string message;
        std::vector<Issue> issues;
    };

    struct Artifact final
    {
        // LLVM objects are owned only while Lower is running. Both
        // representations below are independent copies and therefore remain
        // valid after the LLVM context dies.
        std::string llvm_ir;
        std::vector<std::uint8_t> bitcode;
        // Native payloads remain in memory until an explicit driver writer
        // runs; validation commands consequently cannot leave incidental files
        // behind.
        std::vector<std::uint8_t> object;
        std::string assembly;
        std::string target_triple;
        ObjectFormat objectFormat{ ObjectFormat::Unknown };
        std::size_t function_count{};

        [[nodiscard]] auto
        empty() const noexcept -> bool
        {
            return llvm_ir.empty() || bitcode.empty();
        }
    };

    struct Result final
    {
        std::optional<Artifact> artifact;
        std::optional<Error> error;

        [[nodiscard]] explicit
        operator bool() const noexcept
        {
            return artifact.has_value();
        }
    };

    /**
     * A scalar value returned by a native ORC invocation.
     *
     *
     * Only scalar alternatives whose host ABI is explicitly supported by this

     * * interface are represented. The source type remains attached so REPLs
     * and
     * debuggers can retain exact signedness and width when printing
     * or feeding
     * the value into the next compilation unit.
     */
    struct JitValue final
    {
        Core::Type type{ Core::Type::unit() };
        std::variant<std::monostate,
                     bool,
                     char32_t,
                     std::int64_t,
                     std::uint64_t,
                     double>
            payload;
    };

    enum class JitErrorKind : std::uint8_t
    {
        Initialization,
        InvalidBitcode,
        ModuleAddition,
        SymbolLookup,
        UnsupportedResult,
        Invocation,
        SignatureMismatch
    };

    struct JitError final
    {
        JitErrorKind kind{ JitErrorKind::Initialization };
        std::string code;
        std::string message;
    };

    struct JitResult final
    {
        std::optional<JitValue> value;
        std::optional<JitError> error;

        [[nodiscard]] explicit
        operator bool() const noexcept
        {
            return value.has_value();
        }
    };

    /**
     * Owns one process-local LLVM ORC LLJIT and all modules added to
     * it.
     *
     * Each module is independently verified before insertion.
     * A lookup and
     * invocation is serialized with insertion, so callers
     * can safely submit
     * work from several frontend workers without
     * racing LLJIT mutation. The
     * callable ABI is intentionally
     * zero-argument and scalar-result only;
     * richer function arguments
     * require an explicit language ABI rather than
     * host-side guesses.

     */
    class JitSession final
    {
    public:
        JitSession();
        ~JitSession();
        JitSession(const JitSession &) = delete;
        auto
        operator=(const JitSession &) -> JitSession & = delete;
        JitSession(JitSession &&) noexcept;
        auto
        operator=(JitSession &&) noexcept -> JitSession &;

        /** Add verified bitcode and register its zero-argument entry with its
         * exact X# result type. */
        [[nodiscard]] auto
        AddModule(std::span<const std::uint8_t> bitcode,
                  std::string_view identifier,
                  std::string_view entrySymbol,
                  const Core::Type &entryResultType) -> std::optional<JitError>;

        /** Remove every module and symbol previously registered in this
         * session. */
        [[nodiscard]] auto
        Reset() -> std::optional<JitError>;

        /** Find and invoke one zero-argument function using its Visual X#
         * result type. */
        [[nodiscard]] auto
        InvokeScalar(std::string_view symbol, const Core::Type &resultType)
            -> JitResult;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };

    // Verify is public so tools can diagnose an Xmm artifact without
    // constructing LLVM state. Lower runs the same verifier again; callers
    // cannot accidentally bypass the backend's structural and type-safety
    // boundary.
    [[nodiscard]] auto
    Verify(const Xmm::Module &module) -> std::vector<Issue>;
    [[nodiscard]] auto
    Lower(const Xmm::Module &module, const Options &options = {}) -> Result;
    [[nodiscard]] auto
    WriteLlvmIr(const std::filesystem::path &path, std::string_view llvmIr)
        -> std::optional<Error>;
    [[nodiscard]] auto
    WriteBitcode(const std::filesystem::path &path,
                 const std::vector<std::uint8_t> &bitcode)
        -> std::optional<Error>;
    [[nodiscard]] auto
    WriteObject(const std::filesystem::path &path,
                const std::vector<std::uint8_t> &object)
        -> std::optional<Error>;
    [[nodiscard]] auto
    WriteAssembly(const std::filesystem::path &path, std::string_view assembly)
        -> std::optional<Error>;
} // namespace Visual::XSharp::Backend::LLVM
