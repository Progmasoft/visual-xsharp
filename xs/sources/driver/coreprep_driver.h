/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */
#ifndef VISUAL_XSHARP_COREPREP_DRIVER_H
#define VISUAL_XSHARP_COREPREP_DRIVER_H

#include "options.h"

#ifdef __cplusplus
extern "C"
{
#endif
    int xs_driver_build_coreprep(const XsCliOptions *options);
#ifdef __cplusplus
}
#endif

#endif
