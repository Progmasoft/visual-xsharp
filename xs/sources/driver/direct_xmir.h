/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef XS_DRIVER_DIRECT_XMIR_H
#define XS_DRIVER_DIRECT_XMIR_H

#include <stddef.h>

bool xs_driver_build_direct_xmir(const char *input_path, const char *text, size_t length);

#endif
