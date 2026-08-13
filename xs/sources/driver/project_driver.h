/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef XS_DRIVER_PROJECT_DRIVER_H
#define XS_DRIVER_PROJECT_DRIVER_H

#include "options.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct
    {
        char **paths;
        size_t path_count;
        char **test_paths;
        size_t test_path_count;
        char *entry;
        char *compiler_version;
        char *standard;
        XsBuildOutput output;
        XsCompilerSettings settings;
    } XsResolvedProject;

    bool xs_driver_resolve_project(XsResolvedProject *project);
    bool xs_driver_resolve_project_tests(XsResolvedProject *project);
    bool xs_driver_refresh_lock(void);
    void xs_driver_free_project(XsResolvedProject *project);

#ifdef __cplusplus
}
#endif

#endif
