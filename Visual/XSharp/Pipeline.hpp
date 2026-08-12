// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "Visual/XSharp/Backend/LLVM.hpp"
#include "Visual/XSharp/Core/CorePrep/Verifier.hpp"
#include "Visual/XSharp/Core/CorePrep/Wire.hpp"
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
    ::Visual::XSharp::Backend::LLVM::Options llvm{};
};

struct PipelineResult final
{
    std::optional<core::CorePrepModule> core_prep;
    std::optional<xpp::Module> xpp;
    std::optional<xmm::Module> xmm;
    std::optional<::Visual::XSharp::Backend::LLVM::Artifact> llvm;
    std::optional<::Visual::XSharp::Backend::LLVM::Error> llvm_error;
    std::optional<core::wire::Error> wire_error;
    std::vector<core::VerificationIssue> verification_issues;

    [[nodiscard]] explicit operator bool() const noexcept { return llvm.has_value(); }
};

[[nodiscard]] auto consume_coreprep(std::span<const std::uint8_t> bytes, const PipelineOptions &options = {})
    -> PipelineResult;
} // namespace visual_xsharp
