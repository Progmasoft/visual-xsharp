/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef XS_DRIVER_PROJECT_DRIVER_H
#define XS_DRIVER_PROJECT_DRIVER_H

#include "options.h"

#include <stddef.h>

typedef struct
{
    char **paths;
    char **module_names;
    size_t path_count;
    char **test_paths;
    size_t test_path_count;
    XsCompilerSettings settings;
} XsResolvedProject;

bool xs_driver_resolve_project(const char *module_path, XsResolvedProject *project);
bool xs_driver_resolve_project_tests(const char *module_path, XsResolvedProject *project);
bool xs_driver_refresh_lock(void);
void xs_driver_free_project(XsResolvedProject *project);

#endif
