// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "Options.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace Visual::XSharp::Driver
{
struct ResolvedTestSuite final
{
    std::string name;
    std::optional<std::string> framework;
    std::filesystem::path root;
    std::vector<std::string> excludes;
};

struct ResolvedProject final
{
    std::vector<std::filesystem::path> sourceRoots;
    std::vector<std::string> sourceExcludes;
    std::vector<ResolvedTestSuite> testSuites;
    std::vector<std::string> targets;
    std::string entry;
    std::string compilerVersion;
    std::string standard;
    std::filesystem::path outputDirectory;
    XsBuildOutput output{};
    XsCompilerSettings settings{};
};

[[nodiscard]] std::optional<ResolvedProject> ResolveProject(bool requireSources);
[[nodiscard]] bool RefreshProjectLock();
} // namespace Visual::XSharp::Driver
