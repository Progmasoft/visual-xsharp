// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

#pragma once

#include "Compiler/Cli/Arguments/Options.hpp"

// This is an internal C++20 boundary despite the stable symbol spellings. The
// typed command prevents the Core pipeline from reparsing CLI strings.
[[nodiscard]] bool xs_driver_process_core_artifact(const char *path, XsCliCommand command, XsBuildOutput output,
                                                   const XsCompilerSettings *settings, const char *targetTriple);
[[nodiscard]] bool xs_driver_process_core_artifact_as(const char *path, const char *artifactBasePath,
                                                      XsCliCommand command, XsBuildOutput output,
                                                      const XsCompilerSettings *settings, const char *targetTriple);
