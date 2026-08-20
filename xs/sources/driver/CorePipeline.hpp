// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "Options.hpp"

// This is an internal C++20 boundary despite the stable symbol spellings. The
// typed command prevents the Core pipeline from reparsing CLI strings.
[[nodiscard]] bool xs_driver_process_core_artifact(const char *path, XsCliCommand command, XsBuildOutput output,
                                                   const XsCompilerSettings *settings, const char *targetTriple);
[[nodiscard]] bool xs_driver_process_core_artifact_as(const char *path, const char *artifactBasePath,
                                                      XsCliCommand command, XsBuildOutput output,
                                                      const XsCompilerSettings *settings, const char *targetTriple);
