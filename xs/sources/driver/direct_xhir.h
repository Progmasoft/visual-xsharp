/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef XS_DRIVER_DIRECT_XHIR_H
#define XS_DRIVER_DIRECT_XHIR_H

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

  bool xs_driver_build_direct_xhir(const char *input_path, const char *text, size_t length);

#ifdef __cplusplus
}
#endif

#endif
