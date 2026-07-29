/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef XS_DRIVER_DIRECT_XLIL_H
#define XS_DRIVER_DIRECT_XLIL_H

#include "xs/lil.h"

#include <stddef.h>

bool xs_driver_build_lil_module_native(const char *input_path, const XsLilModule *module);
bool xs_driver_build_direct_xlil(const char *input_path, const char *text, size_t length);

#endif
