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
void print_artifact_error(const visual_xsharp::core::ArtifactError &error)
{
    std::fprintf(stderr, "vxs: could not read CorePrep artifact '%s': %s\n",
                 error.path.string().c_str(), error.message.c_str());
    if(error.wire_error)
        std::fprintf(stderr, "vxs: CorePrep wire error at byte %zu in %s: %s\n", error.wire_error->offset,
                     error.wire_error->context.c_str(), error.wire_error->message.c_str());
}

void print_verification_issue(const visual_xsharp::core::VerificationIssue &issue)
{
    std::fprintf(stderr, "vxs: %s: %s (function %llu, block %u)\n", issue.code.c_str(), issue.message.c_str(),
                 static_cast<unsigned long long>(issue.function), static_cast<unsigned int>(issue.block));
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
    if(options.output_override)
    {
        std::fprintf(stderr, "vxs: -Emit from CorePrep input is not connected to artifact writers yet\n");
        return 1;
    }

    auto loaded = visual_xsharp::core::read_coreprep_artifact(std::filesystem::path(options.file_path));
    if(!loaded)
    {
        print_artifact_error(*loaded.error);
        return 1;
    }
    const auto encoded = visual_xsharp::core::wire::encode(*loaded.module);
    if(!encoded)
    {
        std::fprintf(stderr, "vxs: decoded CorePrep module could not be retained in the RAM pipeline: %s\n",
                     encoded.error->message.c_str());
        return 1;
    }

    visual_xsharp::PipelineOptions pipeline_options;
    pipeline_options.optimize_xpp = options.compiler.xpp_optimization_passes;
    pipeline_options.optimize_xmm = options.compiler.xmm_optimization_passes;
    const auto result = visual_xsharp::consume_coreprep(encoded.bytes, pipeline_options);
    if(!result)
    {
        if(result.wire_error)
            std::fprintf(stderr, "vxs: CorePrep RAM pipeline error at byte %zu: %s\n",
                         result.wire_error->offset, result.wire_error->message.c_str());
        for(const auto &issue : result.verification_issues)
            print_verification_issue(issue);
        return 1;
    }

    if(std::string_view(options.command) == "build")
        std::printf("vxs: CorePrep verified and lowered in memory (%zu function(s), %zu Xpp block(s), %zu Xmm block(s))\n",
                    result.core_prep->functions.size(),
                    result.xpp->functions.empty() ? 0U : result.xpp->functions.front().blocks.size(),
                    result.xmm->functions.empty() ? 0U : result.xmm->functions.front().blocks.size());
    return 0;
}
