/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef XS_DRIVER_NATIVE_ARTIFACT_H
#define XS_DRIVER_NATIVE_ARTIFACT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif
    char *xs_driver_native_artifact_path(const char *input_path, const char *extension);
    bool xs_driver_execute_native_artifact(const char *input_path, int *exit_code);
    int xs_driver_run_native_artifact(const char *input_path);
#ifdef __cplusplus
}
#endif

#endif
