// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

#include "CorePipeline.hpp"
#include "Visual/XSharp/Pipeline.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string_view>
#include <vector>

namespace
{
namespace Llvm = Visual::XSharp::Backend::LLVM;

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
        std::fprintf(stderr, "vxs: Core artifact error at byte %zu (%s): %s\n", result.coreWireError->offset,
                     result.coreWireError->context.c_str(), result.coreWireError->message.c_str());
        return;
    }
    for(const auto &issue : result.coreVerificationIssues)
        std::fprintf(stderr, "vxs: %s: %s [function=%llu, symbol=%llu]\n", issue.code.c_str(), issue.message.c_str(),
                     static_cast<unsigned long long>(issue.function), static_cast<unsigned long long>(issue.symbol));
    for(const auto &issue : result.verification_issues)
        std::fprintf(stderr, "vxs: %s: %s [function=%llu, block=%u]\n", issue.code.c_str(), issue.message.c_str(),
                     static_cast<unsigned long long>(issue.function), static_cast<unsigned>(issue.block));
    if(result.llvm_error)
        std::fprintf(stderr, "vxs: %s: %s\n", result.llvm_error->code.c_str(), result.llvm_error->message.c_str());
}

[[nodiscard]] auto WriteArtifact(const char *inputPath, XsBuildOutput output, const Llvm::Artifact &artifact) -> bool
{
    const char *extension = output == XS_BUILD_OUTPUT_LLVM_LL ? ".ll" : ".bc";
    auto path = std::filesystem::path(inputPath);
    path.replace_extension(extension);
    const auto error = output == XS_BUILD_OUTPUT_LLVM_LL ? Llvm::WriteLlvmIr(path, artifact.llvm_ir)
                                                         : Llvm::WriteBitcode(path, artifact.bitcode);
    if(error)
    {
        std::fprintf(stderr, "vxs: %s: %s\n", error->code.c_str(), error->message.c_str());
        return false;
    }
    std::fprintf(stderr, "vxs: wrote '%s' from verified Core through CorePrep, Xpp, Xmm and LLVM\n",
                 path.string().c_str());
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
        std::fprintf(stderr, "vxs: Core artifact '%s' exceeds the %zu-byte input limit\n", path,
                     coreLimits.maximumWireBytes);
        return false;
    }
    const auto bytes = ReadFile(path);
    if(!bytes)
    {
        std::fprintf(stderr, "vxs: could not read Core artifact '%s'\n", path);
        return false;
    }

    visual_xsharp::PipelineOptions options;
    options.optimize_xpp = settings->xpp_optimization_passes;
    options.optimize_xmm = settings->xmm_optimization_passes;
    options.llvm.optimization = Optimization(*settings);
    // An empty target deliberately delegates to LLVM's host-dependent default.
    // Explicit CLI targets have already passed parser and project-catalog checks.
    options.llvm.target_triple = targetTriple == nullptr ? "" : targetTriple;
    const auto result = Visual::XSharp::Pipeline::ConsumeCore(*bytes, options);
    if(!result)
    {
        PrintFailure(result);
        return false;
    }
    if(command == XS_CLI_COMMAND_CHECK)
    {
        std::fprintf(stderr, "vxs: Core artifact '%s' is valid through the LLVM boundary\n", path);
        return true;
    }
    if(output == XS_BUILD_OUTPUT_LLVM_LL || output == XS_BUILD_OUTPUT_LLVM_BC)
        return WriteArtifact(artifactBasePath, output, *result.llvm);
    std::fprintf(stderr,
                 "vxs: Core input currently emits llvmll or llvmbc; native object/link and Xpp/Xmm artifact writers "
                 "are separate pipeline work\n");
    return false;
}

bool xs_driver_process_core_artifact(const char *path, XsCliCommand command, XsBuildOutput output,
                                     const XsCompilerSettings *settings, const char *targetTriple)
{
    return xs_driver_process_core_artifact_as(path, path, command, output, settings, targetTriple);
}
