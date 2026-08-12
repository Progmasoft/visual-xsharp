// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

#include "coreprep_driver.h"

#include "Visual/XSharp/Core/CorePrep/Artifact.hpp"
#include "Visual/XSharp/Pipeline.hpp"

#include <cstdio>
#include <filesystem>
#include <string_view>

namespace
{
void PrintArtifactError(const visual_xsharp::core::ArtifactError &error)
{
    std::fprintf(stderr, "vxs: could not read CorePrep artifact '%s': %s\n",
                 error.path.string().c_str(), error.message.c_str());
    if(error.wire_error)
        std::fprintf(stderr, "vxs: CorePrep wire error at byte %zu in %s: %s\n", error.wire_error->offset,
                     error.wire_error->context.c_str(), error.wire_error->message.c_str());
}

void PrintVerificationIssue(const visual_xsharp::core::VerificationIssue &issue)
{
    std::fprintf(stderr, "vxs: %s: %s (function %llu, block %u)\n", issue.code.c_str(), issue.message.c_str(),
                 static_cast<unsigned long long>(issue.function), static_cast<unsigned int>(issue.block));
}

void PrintLlvmError(const Visual::XSharp::Backend::LLVM::Error &error)
{
    std::fprintf(stderr, "vxs: %s: %s\n", error.code.c_str(), error.message.c_str());
    for(const auto &issue : error.issues)
        std::fprintf(stderr, "vxs: %s: %s (function %llu, block %u, instruction %zu)\n",
                     issue.code.c_str(), issue.message.c_str(), static_cast<unsigned long long>(issue.function),
                     static_cast<unsigned int>(issue.block), issue.instruction);
}

auto LlvmOptions(const XsCompilerSettings &settings) -> Visual::XSharp::Backend::LLVM::Options
{
    Visual::XSharp::Backend::LLVM::Options options;
    switch(settings.llvm_opt_level)
    {
    case XS_LLVM_OPT_0:
    case XS_LLVM_OPT_G: options.optimization = Visual::XSharp::Backend::LLVM::OptimizationLevel::Debug; break;
    case XS_LLVM_OPT_1: options.optimization = Visual::XSharp::Backend::LLVM::OptimizationLevel::Less; break;
    case XS_LLVM_OPT_2: options.optimization = Visual::XSharp::Backend::LLVM::OptimizationLevel::Default; break;
    case XS_LLVM_OPT_3: options.optimization = Visual::XSharp::Backend::LLVM::OptimizationLevel::Aggressive; break;
    }
    return options;
}

auto OutputPath(const std::filesystem::path &input, XsBuildOutput output) -> std::filesystem::path
{
    auto path = input;
    path.replace_extension(output == XS_BUILD_OUTPUT_LLVM_LL ? ".ll" : ".bc");
    return path;
}
} // namespace

extern "C" int xs_driver_build_coreprep(const XsCliOptions *options_pointer)
{
    const auto &options = *options_pointer;
    if(options.file_path == nullptr)
    {
        std::fprintf(stderr, "vxs: -Build core requires -File PATH.core\n");
        return 2;
    }
    if(std::string_view(options.command) != "build" && std::string_view(options.command) != "check")
    {
        std::fprintf(stderr, "vxs: CorePrep artifact input supports only build and check commands\n");
        return 2;
    }
    if(options.output_override && options.output != XS_BUILD_OUTPUT_LLVM_LL && options.output != XS_BUILD_OUTPUT_LLVM_BC)
    {
        std::fprintf(stderr, "vxs: CorePrep input currently emits only llvmll or llvmbc\n");
        return 1;
    }
    if(std::string_view(options.command) == "check" && options.output_override)
    {
        std::fprintf(stderr, "vxs: check does not emit artifacts\n");
        return 2;
    }

    auto loaded = visual_xsharp::core::read_coreprep_artifact(std::filesystem::path(options.file_path));
    if(!loaded)
    {
        PrintArtifactError(*loaded.error);
        return 1;
    }
    const auto encoded = visual_xsharp::core::wire::encode(*loaded.module);
    if(!encoded)
    {
        std::fprintf(stderr, "vxs: decoded CorePrep module could not be retained in the RAM pipeline: %s\n",
                     encoded.error->message.c_str());
        return 1;
    }

    visual_xsharp::PipelineOptions pipelineOptions;
    pipelineOptions.optimize_xpp = options.compiler.xpp_optimization_passes;
    pipelineOptions.optimize_xmm = options.compiler.xmm_optimization_passes;
    pipelineOptions.llvm = LlvmOptions(options.compiler);
    const auto result = visual_xsharp::consume_coreprep(encoded.bytes, pipelineOptions);
    if(!result)
    {
        if(result.wire_error)
            std::fprintf(stderr, "vxs: CorePrep RAM pipeline error at byte %zu: %s\n",
                         result.wire_error->offset, result.wire_error->message.c_str());
        for(const auto &issue : result.verification_issues)
            PrintVerificationIssue(issue);
        if(result.llvm_error)
            PrintLlvmError(*result.llvm_error);
        return 1;
    }

    if(std::string_view(options.command) == "build")
    {
        if(options.output_override)
        {
            const auto path = OutputPath(std::filesystem::path(options.file_path), options.output);
            const auto error = options.output == XS_BUILD_OUTPUT_LLVM_LL
                                   ? Visual::XSharp::Backend::LLVM::WriteLlvmIr(path, result.llvm->llvm_ir)
                                   : Visual::XSharp::Backend::LLVM::WriteBitcode(path, result.llvm->bitcode);
            if(error)
            {
                PrintLlvmError(*error);
                return 1;
            }
            std::printf("vxs: wrote %s\n", path.string().c_str());
        }
        std::printf("vxs: CorePrep verified and lowered in memory (%zu function(s), %zu Xpp block(s), %zu Xmm block(s), %zu LLVM bitcode byte(s))\n",
                    result.core_prep->functions.size(),
                    result.xpp->functions.empty() ? 0U : result.xpp->functions.front().blocks.size(),
                    result.xmm->functions.empty() ? 0U : result.xmm->functions.front().blocks.size(),
                    result.llvm->bitcode.size());
    }
    return 0;
}
