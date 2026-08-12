// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

#include "Visual/XSharp/Pipeline.hpp"

namespace visual_xsharp
{
auto consume_coreprep(std::span<const std::uint8_t> bytes, const PipelineOptions &options) -> PipelineResult
{
    PipelineResult result;
    auto decoded = core::wire::decode(bytes, options.wire_limits);
    if(!decoded)
    {
        result.wire_error = std::move(decoded.error);
        return result;
    }

    result.core_prep = std::move(decoded.module);
    result.verification_issues = core::verify(*result.core_prep);
    if(!result.verification_issues.empty())
        return result;

    auto lowered_xpp = xpp::lower(*result.core_prep);
    result.xpp = options.optimize_xpp ? xpp::optimize(std::move(lowered_xpp)) : std::move(lowered_xpp);
    auto lowered_xmm = xmm::lower(*result.xpp);
    result.xmm = options.optimize_xmm ? xmm::optimize(std::move(lowered_xmm)) : std::move(lowered_xmm);
    auto loweredLlvm = ::Visual::XSharp::Backend::LLVM::Lower(*result.xmm, options.llvm);
    if(!loweredLlvm)
    {
        result.llvm_error = std::move(loweredLlvm.error);
        return result;
    }
    result.llvm = std::move(loweredLlvm.artifact);
    return result;
}
} // namespace visual_xsharp
