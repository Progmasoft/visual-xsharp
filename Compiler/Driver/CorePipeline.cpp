// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fmt/format.h>
#include <fstream>
#include <iterator>
#include <optional>
#include <string_view>
#include <vector>

#include "Compiler/Driver/CorePipeline.hpp"
#include "Compiler/Linker/NativeLinker.hpp"
#include "Visual/XSharp/Pipeline.hpp"

#ifdef _WIN32
#    include <process.h>
#else
#    include <unistd.h>
#endif

namespace
{
    namespace Llvm = Visual::XSharp::Backend::LLVM;
    namespace Driver = Visual::XSharp::Driver;

    class TemporaryObject final
    {
    public:
        explicit TemporaryObject(const std::filesystem::path &artifactBase)
        {
            static std::atomic_uint64_t sequence{};
#ifdef _WIN32
            const auto process = static_cast<std::uint64_t>(_getpid());
#else
            const auto process = static_cast<std::uint64_t>(getpid());
#endif
            path_ = artifactBase.parent_path() / (artifactBase.filename().string() + ".vxs-link-" + std::to_string(process) + "-" + std::to_string(sequence.fetch_add(1U, std::memory_order_relaxed)) + ".o");
        }

        TemporaryObject(const TemporaryObject &) = delete;
        auto
        operator=(const TemporaryObject &) -> TemporaryObject & = delete;

        ~TemporaryObject()
        {
            // Cleanup is best-effort and covers every return path, including a failed
            // link or a later output-validation diagnostic.
            std::error_code ignored;
            std::filesystem::remove(path_, ignored);
        }

        [[nodiscard]] auto
        Path() const -> const std::filesystem::path &
        {
            return path_;
        }

    private:
        std::filesystem::path path_;
    };

    [[nodiscard]] auto
    ReadFile(const std::filesystem::path &path) -> std::optional<std::vector<std::uint8_t>>
    {
        std::ifstream stream(path, std::ios::binary);
        if (!stream)
            return std::nullopt;
        return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(stream), {});
    }

    [[nodiscard]] auto
    Optimization(const XsCompilerSettings &settings) -> Llvm::OptimizationLevel
    {
        switch (settings.llvm_opt_level)
        {
            case XS_LLVM_OPT_0:
            case XS_LLVM_OPT_G:
                return Llvm::OptimizationLevel::Debug;
            case XS_LLVM_OPT_1:
                return Llvm::OptimizationLevel::Less;
            case XS_LLVM_OPT_2:
                return Llvm::OptimizationLevel::Default;
            case XS_LLVM_OPT_3:
                return Llvm::OptimizationLevel::Aggressive;
        }
        return Llvm::OptimizationLevel::Default;
    }

    void
    PrintFailure(const visual_xsharp::PipelineResult &result)
    {
        if (result.coreWireError)
        {
            fmt::print(stderr, "vxs: Core artifact error at byte {} ({}): {}\n", result.coreWireError->offset, result.coreWireError->context, result.coreWireError->message);
            return;
        }
        if (result.xppWireError)
        {
            fmt::print(stderr, "vxs: Xpp artifact error at byte {} ({}): {}\n", result.xppWireError->offset, result.xppWireError->context, result.xppWireError->message);
            return;
        }
        if (result.xmmWireError)
        {
            fmt::print(stderr, "vxs: Xmm artifact error at byte {} ({}): {}\n", result.xmmWireError->offset, result.xmmWireError->context, result.xmmWireError->message);
            return;
        }
        for (const auto &issue : result.coreVerificationIssues)
            fmt::print(stderr, "vxs: {}: {} [function={}, symbol={}]\n", issue.code, issue.message, issue.function, issue.symbol);
        for (const auto &issue : result.verification_issues)
            fmt::print(stderr, "vxs: {}: {} [function={}, block={}]\n", issue.code, issue.message, issue.function, issue.block);
        for (const auto &issue : result.xppVerificationIssues)
            fmt::print(stderr, "vxs: {}: {} [Xpp function={}, block={}, instruction={}]\n", issue.code, issue.message, issue.function, issue.block, issue.instruction);
        for (const auto &issue : result.xmmVerificationIssues)
            fmt::print(stderr, "vxs: {}: {} [Xmm function={}, block={}, instruction={}]\n", issue.code, issue.message, issue.function, issue.block, issue.instruction);
        if (result.llvm_error)
            fmt::print(stderr, "vxs: {}: {}\n", result.llvm_error->code, result.llvm_error->message);
    }

    [[nodiscard]] auto
    ArtifactPath(const char *inputPath, std::string_view extension) -> std::filesystem::path
    {
        // Project mode deliberately supplies an artifact base unrelated to the
        // temporary Core path, keeping generated files in the configured build root.
        auto path = std::filesystem::path(inputPath);
        path.replace_extension(extension);
        return path;
    }

    [[nodiscard]] auto
    ReportWrite(const std::filesystem::path &path, const std::optional<Llvm::Error> &error) -> bool
    {
        if (error)
        {
            fmt::print(stderr, "vxs: {}: {}\n", error->code, error->message);
            return false;
        }
        fmt::print(stderr, "vxs: wrote '{}' from verified Core through CorePrep, Xpp, Xmm and LLVM\n", path.string());
        return true;
    }

    [[nodiscard]] auto
    WriteArtifact(const char *inputPath, XsBuildOutput output, const Llvm::Artifact &artifact) -> bool
    {
        if (output == XS_BUILD_OUTPUT_LLVM_LL)
        {
            const auto path = ArtifactPath(inputPath, ".ll");
            return ReportWrite(path, Llvm::WriteLlvmIr(path, artifact.llvm_ir));
        }
        if (output == XS_BUILD_OUTPUT_LLVM_BC)
        {
            const auto path = ArtifactPath(inputPath, ".bc");
            return ReportWrite(path, Llvm::WriteBitcode(path, artifact.bitcode));
        }
        if (output == XS_BUILD_OUTPUT_OBJECT)
        {
            const auto path = ArtifactPath(inputPath, ".o");
            return ReportWrite(path, Llvm::WriteObject(path, artifact.object));
        }
        if (output == XS_BUILD_OUTPUT_ASSEMBLY)
        {
            const auto path = ArtifactPath(inputPath, ".asm");
            return ReportWrite(path, Llvm::WriteAssembly(path, artifact.assembly));
        }
        return false;
    }

    [[nodiscard]] auto
    WriteBytes(const std::filesystem::path &path, std::span<const std::uint8_t> bytes, std::string_view stage) -> bool
    {
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            fmt::print(stderr, "vxs: could not open {} artifact '{}' for writing\n", stage, path.string());
            return false;
        }
        stream.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!stream)
        {
            fmt::print(stderr, "vxs: could not finish writing {} artifact '{}'\n", stage, path.string());
            return false;
        }
        fmt::print(stderr, "vxs: wrote verified {} artifact '{}'\n", stage, path.string());
        return true;
    }

    [[nodiscard]] auto
    WriteIntermediate(const char *artifactBasePath, XsBuildOutput output, const visual_xsharp::PipelineResult &result) -> bool
    {
        if (output == XS_BUILD_OUTPUT_XPP && result.xpp)
        {
            auto encoded = Visual::XSharp::Xpp::Wire::Encode(*result.xpp);
            if (!encoded)
            {
                fmt::print(stderr, "vxs: Xpp encode error at byte {} ({}): {}\n", encoded.error->offset, encoded.error->context, encoded.error->message);
                return false;
            }
            return WriteBytes(ArtifactPath(artifactBasePath, ".xpp"), encoded.bytes, "Xpp");
        }
        if (output == XS_BUILD_OUTPUT_XMM && result.xmm)
        {
            auto encoded = Visual::XSharp::Xmm::Wire::Encode(*result.xmm);
            if (!encoded)
            {
                fmt::print(stderr, "vxs: Xmm encode error at byte {} ({}): {}\n", encoded.error->offset, encoded.error->context, encoded.error->message);
                return false;
            }
            return WriteBytes(ArtifactPath(artifactBasePath, ".xmm"), encoded.bytes, "Xmm");
        }
        return false;
    }

    [[nodiscard]] auto
    WriteExecutable(const char *inputPath, const Llvm::Artifact &artifact) -> bool
    {
        const auto output = ArtifactPath(inputPath, ".vxse");
        TemporaryObject temporary(inputPath);
        // Binary is one user-visible artifact. The object exists only long enough
        // for LLD and is never confused with an explicit `-Emit object` request.
        if (const auto error = Llvm::WriteObject(temporary.Path(), artifact.object))
        {
            fmt::print(stderr, "vxs: {}: {}\n", error->code, error->message);
            return false;
        }
        const Driver::NativeLinkRequest request{ output, { temporary.Path() }, artifact.objectFormat };
        const auto linked = Driver::LinkNativeExecutable(request);
        if (!linked)
        {
            fmt::print(stderr, "vxs: native link failed: {}\n", linked.diagnostic);
            return false;
        }
        fmt::print(stderr, "vxs: linked native executable '{}'\n", output.string());
        return true;
    }
} // namespace

namespace
{
    enum class InputStage : std::uint8_t
    {
        Core,
        Xpp,
        Xmm
    };

    [[nodiscard]] auto
    ProcessArtifact(InputStage inputStage, const char *path, const char *artifactBasePath, XsCliCommand command, XsBuildOutput output, const XsCompilerSettings *settings, const char *targetTriple) -> bool
    {
        if (path == nullptr || artifactBasePath == nullptr || settings == nullptr)
            return false;
        std::error_code sizeError;
        const auto artifactSize = std::filesystem::file_size(path, sizeError);
        constexpr auto kMaximumArtifactBytes = std::uintmax_t{ 64U * 1024U * 1024U };
        if (!sizeError && artifactSize > kMaximumArtifactBytes)
        {
            fmt::print(stderr, "vxs: compiler artifact '{}' exceeds the 64 MiB input limit\n", path);
            return false;
        }
        const auto bytes = ReadFile(path);
        if (!bytes)
        {
            fmt::print(stderr, "vxs: could not read compiler artifact '{}'\n", path);
            return false;
        }

        visual_xsharp::PipelineOptions options;
        options.optimize_xpp = settings->xpp_optimization_passes;
        options.optimize_xmm = settings->xmm_optimization_passes;
        options.llvm.optimization = Optimization(*settings);
        options.llvm.target_triple = targetTriple == nullptr ? "" : targetTriple;
        if (output == XS_BUILD_OUTPUT_XPP)
            options.stop_after = visual_xsharp::PipelineStop::Xpp;
        else if (output == XS_BUILD_OUTPUT_XMM)
            options.stop_after = visual_xsharp::PipelineStop::Xmm;
        if (command != XS_CLI_COMMAND_CHECK)
        {
            if (output == XS_BUILD_OUTPUT_BINARY || output == XS_BUILD_OUTPUT_OBJECT)
                options.llvm.machineCode = Llvm::MachineCodeEmission::Object;
            else if (output == XS_BUILD_OUTPUT_ASSEMBLY)
                options.llvm.machineCode = Llvm::MachineCodeEmission::Assembly;
            options.llvm.executableEntry = output == XS_BUILD_OUTPUT_BINARY;
        }

        auto result = inputStage == InputStage::Core  ? Visual::XSharp::Pipeline::ConsumeCore(*bytes, options)
                      : inputStage == InputStage::Xpp ? Visual::XSharp::Pipeline::ConsumeXpp(*bytes, options)
                                                      : Visual::XSharp::Pipeline::ConsumeXmm(*bytes, options);
        if (!result)
        {
            PrintFailure(result);
            return false;
        }
        if (command == XS_CLI_COMMAND_CHECK)
        {
            fmt::print(stderr, "vxs: compiler artifact '{}' is valid through its requested pipeline boundary\n", path);
            return true;
        }
        if (output == XS_BUILD_OUTPUT_XPP || output == XS_BUILD_OUTPUT_XMM)
            return WriteIntermediate(artifactBasePath, output, result);
        if (output == XS_BUILD_OUTPUT_BINARY)
            return WriteExecutable(artifactBasePath, *result.llvm);
        if (output == XS_BUILD_OUTPUT_OBJECT || output == XS_BUILD_OUTPUT_ASSEMBLY
            || output == XS_BUILD_OUTPUT_LLVM_LL || output == XS_BUILD_OUTPUT_LLVM_BC)
            return WriteArtifact(artifactBasePath, output, *result.llvm);
        fmt::print(stderr, "vxs: requested artifact conversion is not supported from this input stage\n");
        return false;
    }
} // namespace

bool
xs_driver_process_core_artifact_as(const char *path, const char *artifactBasePath, XsCliCommand command, XsBuildOutput output, const XsCompilerSettings *settings, const char *targetTriple)
{
    return ProcessArtifact(InputStage::Core, path, artifactBasePath, command, output, settings, targetTriple);
}

bool
xs_driver_process_core_artifact(const char *path, XsCliCommand command, XsBuildOutput output, const XsCompilerSettings *settings, const char *targetTriple)
{
    return xs_driver_process_core_artifact_as(path, path, command, output, settings, targetTriple);
}

bool
xs_driver_process_xpp_artifact_as(const char *path, const char *artifactBasePath, XsCliCommand command, XsBuildOutput output, const XsCompilerSettings *settings, const char *targetTriple)
{
    return ProcessArtifact(InputStage::Xpp, path, artifactBasePath, command, output, settings, targetTriple);
}

bool
xs_driver_process_xmm_artifact_as(const char *path, const char *artifactBasePath, XsCliCommand command, XsBuildOutput output, const XsCompilerSettings *settings, const char *targetTriple)
{
    return ProcessArtifact(InputStage::Xmm, path, artifactBasePath, command, output, settings, targetTriple);
}
