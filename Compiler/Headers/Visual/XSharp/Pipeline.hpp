// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "Visual/XSharp/Backend/LLVM.hpp"
#include "Visual/XSharp/Core/CorePrep/Prepare.hpp"
#include "Visual/XSharp/Core/CorePrep/Verifier.hpp"
#include "Visual/XSharp/Core/CorePrep/Wire.hpp"
#include "Visual/XSharp/Core/Verifier.hpp"
#include "Visual/XSharp/Core/Wire.hpp"
#include "Visual/XSharp/Xmm/IR.hpp"
#include "Visual/XSharp/Xmm/Verifier.hpp"
#include "Visual/XSharp/Xmm/Wire.hpp"
#include "Visual/XSharp/Xpp/IR.hpp"
#include "Visual/XSharp/Xpp/Verifier.hpp"
#include "Visual/XSharp/Xpp/Wire.hpp"

namespace visual_xsharp
{
    enum class PipelineStop : std::uint8_t
    {
        Xpp,
        Xmm,
        Llvm
    };

    struct PipelineOptions final
    {
        bool optimize_xpp{ true };
        bool optimize_xmm{ true };
        core::wire::Limits wire_limits{};
        ::Visual::XSharp::Core::Wire::Limits coreWireLimits{};
        ::Visual::XSharp::Artifact::Wire::Limits artifactWireLimits{};
        ::Visual::XSharp::Backend::LLVM::Options llvm{};
        PipelineStop stop_after{ PipelineStop::Llvm };
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
        std::optional<::Visual::XSharp::Xpp::Wire::Error> xppWireError;
        std::optional<::Visual::XSharp::Xmm::Wire::Error> xmmWireError;
        std::vector<::Visual::XSharp::Core::VerificationIssue> coreVerificationIssues;
        std::vector<core::VerificationIssue> verification_issues;
        std::vector<::Visual::XSharp::Xpp::VerificationIssue> xppVerificationIssues;
        std::vector<::Visual::XSharp::Xmm::VerificationIssue> xmmVerificationIssues;
        bool succeeded{};

        // Success means the requested boundary owns a verified artifact. Callers that stop
        // at Xpp or Xmm intentionally succeed without manufacturing an LLVM module.
        [[nodiscard]] explicit
        operator bool() const noexcept
        {
            return succeeded;
        }
    };

    [[nodiscard]] auto
    consume_coreprep(std::span<const std::uint8_t> bytes, const PipelineOptions &options = {})
        -> PipelineResult;
} // namespace visual_xsharp

namespace Visual::XSharp::Pipeline
{
    using Options = ::visual_xsharp::PipelineOptions;
    using Result = ::visual_xsharp::PipelineResult;
    using Stop = ::visual_xsharp::PipelineStop;

    // ConsumeCore is the public native entry for a Haskell-produced VXCR document. It
    // retains each successful stage in Result and stops before CorePrep on any Core wire
    // or semantic issue.
    [[nodiscard]] auto
    ConsumeCore(std::span<const std::uint8_t> bytes, const Options &options = {}) -> Result;
    [[nodiscard]] auto
    ConsumeXpp(std::span<const std::uint8_t> bytes, const Options &options = {}) -> Result;
    [[nodiscard]] auto
    ConsumeXmm(std::span<const std::uint8_t> bytes, const Options &options = {}) -> Result;
} // namespace Visual::XSharp::Pipeline
