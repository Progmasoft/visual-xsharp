/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 * Shared C
 * and C++ driver boundary.
 */
#ifndef XS_DRIVER_CORE_PIPELINE_H
#define XS_DRIVER_CORE_PIPELINE_H

#include "options.h"

#ifdef __cplusplus
extern "C"
{
#endif
    bool xs_driver_process_core_artifact(const char *path, const char *command, XsBuildOutput output,
                                         const XsCompilerSettings *settings);
#ifdef __cplusplus
}
#endif

#endif
