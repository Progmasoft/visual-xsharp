// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "Visual/XSharp/Core/CorePrep/Wire.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace visual_xsharp::core
{
enum class ArtifactErrorKind : std::uint8_t { InvalidExtension, OpenFailed, ReadFailed, WriteFailed, WireError };

struct ArtifactError final
{
    ArtifactErrorKind kind{ArtifactErrorKind::OpenFailed};
    std::filesystem::path path;
    std::string message;
    std::optional<wire::Error> wire_error;
};

struct ArtifactBytesResult final
{
    std::vector<std::uint8_t> bytes;
    std::optional<ArtifactError> error;
    [[nodiscard]] explicit operator bool() const noexcept { return !error.has_value(); }
};

struct ArtifactModuleResult final
{
    std::optional<CorePrepModule> module;
    std::optional<ArtifactError> error;
    [[nodiscard]] explicit operator bool() const noexcept { return module.has_value(); }
};

[[nodiscard]] auto read_coreprep_artifact(const std::filesystem::path &path, const wire::Limits &limits = {})
    -> ArtifactModuleResult;
[[nodiscard]] auto write_coreprep_artifact(const std::filesystem::path &path, const CorePrepModule &module,
                                           const wire::Limits &limits = {}) -> std::optional<ArtifactError>;
} // namespace visual_xsharp::core
