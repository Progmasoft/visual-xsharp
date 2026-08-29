// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
#pragma once

#include "Visual/XSharp/Backend/LLVM.hpp"
#include "Visual/XSharp/Core/CorePrep/Prepare.hpp"
#include "Visual/XSharp/Core/CorePrep/Verifier.hpp"
#include "Visual/XSharp/Core/CorePrep/Wire.hpp"
#include "Visual/XSharp/Core/Verifier.hpp"
#include "Visual/XSharp/Core/Wire.hpp"
#include "Visual/XSharp/Xmm/IR.hpp"
#include "Visual/XSharp/Xpp/IR.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace visual_xsharp
{
struct PipelineOptions final
{
    bool optimize_xpp{true};
    bool optimize_xmm{true};
    core::wire::Limits wire_limits{};
    ::Visual::XSharp::Core::Wire::Limits coreWireLimits{};
    ::Visual::XSharp::Backend::LLVM::Options llvm{};
};

struct PipelineResult final
{
    // Successful intermediate stages remain available when a later stage fails. This
    // is intentional: diagnostics and compiler tests can inspect the last valid form
    // without rerunning or weakening the end-to-end pipeline contract.
    std::optional<::Visual::XSharp::Core::Module> core;
    std::optional<core::CorePrepModule> core_prep;
    std::optional<xpp::Module> xpp;
    std::optional<xmm::Module> xmm;
    std::optional<::Visual::XSharp::Backend::LLVM::Artifact> llvm;
    std::optional<::Visual::XSharp::Backend::LLVM::Error> llvm_error;
    std::optional<core::wire::Error> wire_error;
    std::optional<::Visual::XSharp::Core::Wire::Error> coreWireError;
    std::vector<::Visual::XSharp::Core::VerificationIssue> coreVerificationIssues;
    std::vector<core::VerificationIssue> verification_issues;

    // Pipeline success means a verified, owned LLVM artifact exists; merely reaching
    // CorePrep, Xpp or Xmm is a partial result rather than a successful compilation.
    [[nodiscard]] explicit operator bool() const noexcept
    {
        return llvm.has_value();
    }
};

[[nodiscard]] auto consume_coreprep(std::span<const std::uint8_t> bytes, const PipelineOptions &options = {})
    -> PipelineResult;
} // namespace visual_xsharp

namespace Visual::XSharp::Pipeline
{
using Options = ::visual_xsharp::PipelineOptions;
using Result = ::visual_xsharp::PipelineResult;

// ConsumeCore is the public native entry for a Haskell-produced VXCR document. It
// retains each successful stage in Result and stops before CorePrep on any Core wire
// or semantic issue.
[[nodiscard]] auto ConsumeCore(std::span<const std::uint8_t> bytes, const Options &options = {}) -> Result;
} // namespace Visual::XSharp::Pipeline
