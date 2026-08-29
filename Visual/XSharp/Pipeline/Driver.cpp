// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

#include "Visual/XSharp/Pipeline.hpp"

namespace visual_xsharp
{
auto consume_coreprep(std::span<const std::uint8_t> bytes, const PipelineOptions &options) -> PipelineResult
{
    PipelineResult result;
    // Decode from bounded wire bytes even for in-process callers. A single entry path
    // keeps size/depth limits identical for files, Haskell output and embedded hosts.
    auto decoded = core::wire::decode(bytes, options.wire_limits);
    if(!decoded)
    {
        result.wire_error = std::move(decoded.error);
        return result;
    }

    result.core_prep = std::move(decoded.module);
    // Semantic verification precedes every lowering stage. Partial state is retained in
    // result, but no downstream IR is fabricated from an invalid CorePrep module.
    result.verification_issues = core::verify(*result.core_prep);
    if(!result.verification_issues.empty())
        return result;

    auto lowered_xpp = xpp::lower(*result.core_prep);
    // Optimization toggles select identity-vs-optimized forms; they never skip a stage.
    // Xmm therefore receives the same typed Xpp contract in debug and release modes.
    result.xpp = options.optimize_xpp ? xpp::optimize(std::move(lowered_xpp)) : std::move(lowered_xpp);
    auto lowered_xmm = xmm::lower(*result.xpp);
    result.xmm = options.optimize_xmm ? xmm::optimize(std::move(lowered_xmm)) : std::move(lowered_xmm);
    auto loweredLlvm = ::Visual::XSharp::Backend::LLVM::Lower(*result.xmm, options.llvm);
    // LLVM failures stay structured instead of being flattened into a pipeline boolean.
    // Frontends can render VXL diagnostics while still inspecting the verified Xmm.
    if(!loweredLlvm)
    {
        result.llvm_error = std::move(loweredLlvm.error);
        return result;
    }
    result.llvm = std::move(loweredLlvm.artifact);
    return result;
}
} // namespace visual_xsharp

namespace Visual::XSharp::Pipeline
{
auto ConsumeCore(std::span<const std::uint8_t> bytes, const Options &options) -> Result
{
    Result result;
    auto decoded = Core::Wire::Decode(bytes, options.coreWireLimits);
    if(!decoded)
    {
        result.coreWireError = std::move(decoded.error);
        return result;
    }

    result.core = std::move(decoded.module);
    result.coreVerificationIssues = Core::Verify(*result.core);
    if(!result.coreVerificationIssues.empty())
        return result;

    // CorePrep remains an internal adapting stage. Re-encoding to VXCP here would add
    // a redundant memory copy, so native Core input hands the verified model directly
    // to the existing CorePrep verifier and post-CorePrep owners.
    result.core_prep = Core::CorePrep::Prepare(*result.core);
    result.verification_issues = ::visual_xsharp::core::verify(*result.core_prep);
    if(!result.verification_issues.empty())
        return result;

    auto loweredXpp = ::visual_xsharp::xpp::lower(*result.core_prep);
    result.xpp = options.optimize_xpp ? ::visual_xsharp::xpp::optimize(std::move(loweredXpp)) : std::move(loweredXpp);
    auto loweredXmm = ::visual_xsharp::xmm::lower(*result.xpp);
    result.xmm = options.optimize_xmm ? ::visual_xsharp::xmm::optimize(std::move(loweredXmm)) : std::move(loweredXmm);
    auto loweredLlvm = Backend::LLVM::Lower(*result.xmm, options.llvm);
    if(!loweredLlvm)
    {
        result.llvm_error = std::move(loweredLlvm.error);
        return result;
    }
    result.llvm = std::move(loweredLlvm.artifact);
    return result;
}
} // namespace Visual::XSharp::Pipeline
