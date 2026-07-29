/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef XS_LIL_C_API_H
#define XS_LIL_C_API_H

#include <stdint.h>

#define XS_LIL_C_API_VERSION_MAJOR 1U
#define XS_LIL_C_API_VERSION_MINOR 0U
#define XS_LIL_C_API_VERSION ((XS_LIL_C_API_VERSION_MAJOR << 16U) | XS_LIL_C_API_VERSION_MINOR)

#if defined(_WIN32) && defined(XS_LIL_SHARED)
#if defined(XS_LIL_BUILDING_LIBRARY)
#define XS_LIL_API __declspec(dllexport)
#else
#define XS_LIL_API __declspec(dllimport)
#endif
#elif defined(XS_LIL_BUILDING_LIBRARY) && defined(__has_attribute)
#if __has_attribute(visibility)
#define XS_LIL_API __attribute__((visibility("default")))
#else
#define XS_LIL_API
#endif
#else
#define XS_LIL_API
#endif

XS_LIL_API uint32_t xs_lil_c_api_version(void);

#endif
