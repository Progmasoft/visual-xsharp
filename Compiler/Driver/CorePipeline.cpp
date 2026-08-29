// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

#include "Compiler/Driver/CorePipeline.hpp"
#include "Compiler/Linker/NativeLinker.hpp"
#include "Visual/XSharp/Pipeline.hpp"

#include <fmt/format.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string_view>
#include <vector>

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
        path_ = artifactBase.parent_path() /
                (artifactBase.filename().string() + ".vxs-link-" + std::to_string(process) + "-" +
                 std::to_string(sequence.fetch_add(1U, std::memory_order_relaxed)) + ".o");
    }

    TemporaryObject(const TemporaryObject &) = delete;
    auto operator=(const TemporaryObject &) -> TemporaryObject & = delete;

    ~TemporaryObject()
    {
        // Cleanup is best-effort and covers every return path, including a failed
        // link or a later output-validation diagnostic.
        std::error_code ignored;
        std::filesystem::remove(path_, ignored);
    }

    [[nodiscard]] auto Path() const -> const std::filesystem::path &
    {
        return path_;
    }

private:
    std::filesystem::path path_;
};

[[nodiscard]] auto ReadFile(const std::filesystem::path &path) -> std::optional<std::vector<std::uint8_t>>
{
    std::ifstream stream(path, std::ios::binary);
    if(!stream)
        return std::nullopt;
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(stream), {});
}

[[nodiscard]] auto Optimization(const XsCompilerSettings &settings) -> Llvm::OptimizationLevel
{
    switch(settings.llvm_opt_level)
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

void PrintFailure(const visual_xsharp::PipelineResult &result)
{
    if(result.coreWireError)
    {
        fmt::print(stderr, "vxs: Core artifact error at byte {} ({}): {}\n", result.coreWireError->offset,
                   result.coreWireError->context, result.coreWireError->message);
        return;
    }
    for(const auto &issue : result.coreVerificationIssues)
        fmt::print(stderr, "vxs: {}: {} [function={}, symbol={}]\n", issue.code, issue.message, issue.function,
                   issue.symbol);
    for(const auto &issue : result.verification_issues)
        fmt::print(stderr, "vxs: {}: {} [function={}, block={}]\n", issue.code, issue.message, issue.function,
                   issue.block);
    for(const auto &issue : result.xppVerificationIssues)
        fmt::print(stderr, "vxs: {}: {} [Xpp function={}, block={}, instruction={}]\n", issue.code, issue.message,
                   issue.function, issue.block, issue.instruction);
    for(const auto &issue : result.xmmVerificationIssues)
        fmt::print(stderr, "vxs: {}: {} [Xmm function={}, block={}, instruction={}]\n", issue.code, issue.message,
                   issue.function, issue.block, issue.instruction);
    if(result.llvm_error)
        fmt::print(stderr, "vxs: {}: {}\n", result.llvm_error->code, result.llvm_error->message);
}

[[nodiscard]] auto ArtifactPath(const char *inputPath, std::string_view extension) -> std::filesystem::path
{
    // Project mode deliberately supplies an artifact base unrelated to the
    // temporary Core path, keeping generated files in the configured build root.
    auto path = std::filesystem::path(inputPath);
    path.replace_extension(extension);
    return path;
}

[[nodiscard]] auto ReportWrite(const std::filesystem::path &path, const std::optional<Llvm::Error> &error) -> bool
{
    if(error)
    {
        fmt::print(stderr, "vxs: {}: {}\n", error->code, error->message);
        return false;
    }
    fmt::print(stderr, "vxs: wrote '{}' from verified Core through CorePrep, Xpp, Xmm and LLVM\n", path.string());
    return true;
}

[[nodiscard]] auto WriteArtifact(const char *inputPath, XsBuildOutput output, const Llvm::Artifact &artifact) -> bool
{
    if(output == XS_BUILD_OUTPUT_LLVM_LL)
    {
        const auto path = ArtifactPath(inputPath, ".ll");
        return ReportWrite(path, Llvm::WriteLlvmIr(path, artifact.llvm_ir));
    }
    if(output == XS_BUILD_OUTPUT_LLVM_BC)
    {
        const auto path = ArtifactPath(inputPath, ".bc");
        return ReportWrite(path, Llvm::WriteBitcode(path, artifact.bitcode));
    }
    if(output == XS_BUILD_OUTPUT_OBJECT)
    {
        const auto path = ArtifactPath(inputPath, ".o");
        return ReportWrite(path, Llvm::WriteObject(path, artifact.object));
    }
    if(output == XS_BUILD_OUTPUT_ASSEMBLY)
    {
        const auto path = ArtifactPath(inputPath, ".asm");
        return ReportWrite(path, Llvm::WriteAssembly(path, artifact.assembly));
    }
    return false;
}

[[nodiscard]] auto WriteExecutable(const char *inputPath, const Llvm::Artifact &artifact) -> bool
{
    const auto output = ArtifactPath(inputPath, ".vxse");
    TemporaryObject temporary(inputPath);
    // Binary is one user-visible artifact. The object exists only long enough
    // for LLD and is never confused with an explicit `-Emit object` request.
    if(const auto error = Llvm::WriteObject(temporary.Path(), artifact.object))
    {
        fmt::print(stderr, "vxs: {}: {}\n", error->code, error->message);
        return false;
    }
    const Driver::NativeLinkRequest request{output, {temporary.Path()}, artifact.objectFormat};
    const auto linked = Driver::LinkNativeExecutable(request);
    if(!linked)
    {
        fmt::print(stderr, "vxs: native link failed: {}\n", linked.diagnostic);
        return false;
    }
    fmt::print(stderr, "vxs: linked native executable '{}'\n", output.string());
    return true;
}
} // namespace

bool xs_driver_process_core_artifact_as(const char *path, const char *artifactBasePath, XsCliCommand command,
                                        XsBuildOutput output, const XsCompilerSettings *settings,
                                        const char *targetTriple)
{
    if(path == nullptr || artifactBasePath == nullptr || settings == nullptr)
        return false;
    std::error_code sizeError;
    const auto fileSize = std::filesystem::file_size(path, sizeError);
    const Visual::XSharp::Core::Wire::Limits coreLimits;
    if(!sizeError && fileSize > coreLimits.maximumWireBytes)
    {
        fmt::print(stderr, "vxs: Core artifact '{}' exceeds the {}-byte input limit\n", path,
                   coreLimits.maximumWireBytes);
        return false;
    }
    const auto bytes = ReadFile(path);
    if(!bytes)
    {
        fmt::print(stderr, "vxs: could not read Core artifact '{}'\n", path);
        return false;
    }

    visual_xsharp::PipelineOptions options;
    options.optimize_xpp = settings->xpp_optimization_passes;
    options.optimize_xmm = settings->xmm_optimization_passes;
    options.llvm.optimization = Optimization(*settings);
    // An empty target deliberately delegates to LLVM's host-dependent default.
    // Explicit CLI targets have already passed parser and project-catalog checks.
    options.llvm.target_triple = targetTriple == nullptr ? "" : targetTriple;
    if(command != XS_CLI_COMMAND_CHECK)
    {
        // Translate the typed emit kind once at the driver/backend boundary;
        // lower layers never reinterpret command-line strings.
        if(output == XS_BUILD_OUTPUT_BINARY || output == XS_BUILD_OUTPUT_OBJECT)
            options.llvm.machineCode = Llvm::MachineCodeEmission::Object;
        else if(output == XS_BUILD_OUTPUT_ASSEMBLY)
            options.llvm.machineCode = Llvm::MachineCodeEmission::Assembly;
        options.llvm.executableEntry = output == XS_BUILD_OUTPUT_BINARY;
    }
    const auto result = Visual::XSharp::Pipeline::ConsumeCore(*bytes, options);
    if(!result)
    {
        PrintFailure(result);
        return false;
    }
    if(command == XS_CLI_COMMAND_CHECK)
    {
        fmt::print(stderr, "vxs: Core artifact '{}' is valid through the LLVM boundary\n", path);
        return true;
    }
    if(output == XS_BUILD_OUTPUT_BINARY)
        return WriteExecutable(artifactBasePath, *result.llvm);
    if(output == XS_BUILD_OUTPUT_OBJECT || output == XS_BUILD_OUTPUT_ASSEMBLY || output == XS_BUILD_OUTPUT_LLVM_LL ||
       output == XS_BUILD_OUTPUT_LLVM_BC)
        return WriteArtifact(artifactBasePath, output, *result.llvm);
    fmt::print(stderr,
               "vxs: Core input does not yet serialize Xpp or Xmm artifacts; choose binary, object, assembly, llvmll, "
               "or llvmbc\n");
    return false;
}

bool xs_driver_process_core_artifact(const char *path, XsCliCommand command, XsBuildOutput output,
                                     const XsCompilerSettings *settings, const char *targetTriple)
{
    return xs_driver_process_core_artifact_as(path, path, command, output, settings, targetTriple);
}
