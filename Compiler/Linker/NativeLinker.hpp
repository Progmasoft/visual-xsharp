// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "Visual/XSharp/Backend/LLVM.hpp"

namespace Visual::XSharp::Driver
{
    struct NativeLinkRequest final
    {
        std::filesystem::path outputPath;
        std::vector<std::filesystem::path> objectPaths;
        Backend::LLVM::ObjectFormat objectFormat{ Backend::LLVM::ObjectFormat::Unknown };
    };

    struct NativeLinkResult final
    {
        int exitCode{ -1 };
        std::string diagnostic;

        [[nodiscard]] explicit
        operator bool() const noexcept
        {
            return exitCode == 0 && diagnostic.empty();
        }
    };

    // LinkNativeExecutable invokes the platform linker directly with a typed argv.
    // No command shell, response-file parser, or user-controlled command string sits
    // between the compiler model and LLD.
    [[nodiscard]] auto
    LinkNativeExecutable(const NativeLinkRequest &request) -> NativeLinkResult;
} // namespace Visual::XSharp::Driver
