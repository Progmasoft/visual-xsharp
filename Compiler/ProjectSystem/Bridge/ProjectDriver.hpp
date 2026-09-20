// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "Compiler/Cli/Arguments/Options.hpp"

namespace Visual::XSharp::Driver
{
    struct ResolvedTestSuite final
    {
        std::string name;
        std::optional<std::string> framework;
        std::filesystem::path root;
        std::vector<std::string> excludes;
    };

    struct ResolvedSourceTarget final
    {
        std::string name;
        std::optional<std::string> entry;
        std::optional<std::string> namespaceName;
        std::filesystem::path root;
        std::vector<std::string> excludes;
        std::vector<ViPkgType> viPkgTypes;
    };

    struct ResolvedProject final
    {
        std::vector<std::filesystem::path> sourceRoots;
        std::vector<std::string> sourceExcludes;
        std::vector<ResolvedTestSuite> testSuites;
        std::vector<ResolvedSourceTarget> executables;
        std::vector<ResolvedSourceTarget> libraries;
        std::vector<std::string> targets;
        std::string entry;
        std::string compilerVersion;
        std::string standard;
        std::filesystem::path outputDirectory;
        BuildOutput output{};
        CompilerSettings settings{};
    };

    [[nodiscard]] std::optional<ResolvedProject>
    ResolveProject(bool requireSources);
    [[nodiscard]] bool
    RefreshProjectLock();
} // namespace Visual::XSharp::Driver
