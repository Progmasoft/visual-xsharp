// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1
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
    /// Requested last verified intermediate stage in the native pipeline.
    enum class PipelineStop : std::uint8_t
    {
        Xpp, ///< Stop after verified CorePrep-to-Xpp lowering.
        Xmm, ///< Stop after verified Xpp-to-Xmm lowering.
        Llvm ///< Continue through LLVM IR generation and artifact creation.
    };

    /// Optimization, verification, resource, and pipeline-boundary settings.
    struct PipelineOptions final
    {
        /// Run semantics-preserving Xpp optimization before verification.
        bool optimize_xpp{ true };
        /// Run semantics-preserving Xmm optimization before verification.
        bool optimize_xmm{ true };
        /// Limits for the CorePrep wire document consumed by this pipeline.
        core::wire::Limits wire_limits{};
        /// Limits for Core wire validation and decoding.
        ::Visual::XSharp::Core::Wire::Limits coreWireLimits{};
        /// Limits for shared artifact wire validation and decoding.
        ::Visual::XSharp::Artifact::Wire::Limits artifactWireLimits{};
        /// Target and code-generation settings for the LLVM backend.
        ::Visual::XSharp::Backend::LLVM::Options llvm{};
        /// Stage at which successful lowering should stop.
        PipelineStop stop_after{ PipelineStop::Llvm };
    };

    /// Successful intermediate products and the first stage failure details.
    struct PipelineResult final
    {
        /// Verified source-level Core module when decoding succeeds.
        std::optional<::Visual::XSharp::Core::Module> core;
        /// Verified CorePrep module when lowering succeeds.
        std::optional<core::CorePrepModule> core_prep;
        /// Verified Xpp module when lowering succeeds.
        std::optional<xpp::Module> xpp;
        /// Verified Xmm module when lowering succeeds.
        std::optional<xmm::Module> xmm;
        /// Native LLVM artifact when the requested boundary reaches LLVM.
        std::optional<::Visual::XSharp::Backend::LLVM::Artifact> llvm;
        /// LLVM backend failure, if native artifact generation was attempted.
        std::optional<::Visual::XSharp::Backend::LLVM::Error> llvm_error;
        /// CorePrep wire failure, if the supplied payload could not be read.
        std::optional<core::wire::Error> wire_error;
        /// Source-level Core wire failure.
        std::optional<::Visual::XSharp::Core::Wire::Error> coreWireError;
        /// Xpp wire failure.
        std::optional<::Visual::XSharp::Xpp::Wire::Error> xppWireError;
        /// Xmm wire failure.
        std::optional<::Visual::XSharp::Xmm::Wire::Error> xmmWireError;
        /// Semantic issues found while verifying source-level Core.
        std::vector<::Visual::XSharp::Core::VerificationIssue>
            coreVerificationIssues;
        /// Semantic issues found while verifying CorePrep.
        std::vector<core::VerificationIssue> verification_issues;
        /// Semantic issues found while verifying Xpp.
        std::vector<::Visual::XSharp::Xpp::VerificationIssue>
            xppVerificationIssues;
        /// Semantic issues found while verifying Xmm.
        std::vector<::Visual::XSharp::Xmm::VerificationIssue>
            xmmVerificationIssues;
        /// Whether the requested stop boundary produced a verified artifact.
        bool succeeded{};

        /// Test whether the requested boundary completed successfully.
        /// @return The succeeded state of the pipeline result.
        [[nodiscard]] explicit
        operator bool() const noexcept
        {
            return succeeded;
        }
    };

    /// Verify and lower an encoded CorePrep module through the requested stage.
    /// @param bytes Complete CorePrep wire document.
    /// @param options Optimization, validation-limit, backend, and stop
    /// settings.
    /// @return Successful intermediate representations and failure diagnostics.
    [[nodiscard]] auto
    consume_coreprep(std::span<const std::uint8_t> bytes,
                     const PipelineOptions &options = {}) -> PipelineResult;
} // namespace visual_xsharp

namespace Visual::XSharp::Pipeline
{
    /// Public spelling of native pipeline options.
    using Options = ::visual_xsharp::PipelineOptions;
    /// Public spelling of the pipeline result and its partial-stage state.
    using Result = ::visual_xsharp::PipelineResult;
    /// Public spelling of the pipeline stop boundary.
    using Stop = ::visual_xsharp::PipelineStop;

    /// Decode and validate a Haskell-produced Core wire document, then lower
    /// it. Successfully completed stages remain available in Result if a later
    /// verifier or backend reports an error.
    /// @param bytes Complete Core wire document.
    /// @param options Optimization, resource-limit, and stop-boundary settings.
    /// @return Pipeline products and structured diagnostics.
    [[nodiscard]] auto
    ConsumeCore(std::span<const std::uint8_t> bytes,
                const Options &options = {}) -> Result;
    /// Decode a serialized Xpp module and continue lowering from that stage.
    /// @param bytes Complete Xpp wire document.
    /// @param options Optimization, resource-limit, and stop-boundary settings.
    /// @return Verified Xpp/Xmm modules or LLVM artifact, plus failures.
    [[nodiscard]] auto
    ConsumeXpp(std::span<const std::uint8_t> bytes, const Options &options = {})
        -> Result;
    [[nodiscard]] auto
    ConsumeXmm(std::span<const std::uint8_t> bytes, const Options &options = {})
        -> Result;
} // namespace Visual::XSharp::Pipeline
  /// Decode a serialized Xmm module and continue lowering to LLVM.
  /// @param bytes Complete Xmm wire document.
  /// @param options Backend settings and requested pipeline boundary.
  /// @return Verified Xmm module or LLVM artifact, plus failures.
