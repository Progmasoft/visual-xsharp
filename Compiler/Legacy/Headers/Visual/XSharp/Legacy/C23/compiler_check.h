/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 *
 */

#ifndef XS_COMPILER_CHECK_H
#define XS_COMPILER_CHECK_H

#if defined(__GNUC__) && !defined(__clang__)
#    error "GCC is not supported; use ClangCL."
#endif

#if defined(__GNUG__)
#    error "GNU C++ mode is not supported; this project is strict C23."
#endif

#if !defined(__clang__)
#    error "Clang is required."
#endif

#if !defined(_MSC_VER) && !defined(__STRICT_ANSI__)
#    error "GNU C extensions are enabled; compile with strict ISO C mode, e.g. -std=c23, not -std=gnu23."
#endif

#if !defined(__cplusplus) && (!defined(__STDC_VERSION__) || __STDC_VERSION__ < 202311L)
#    error "C23 mode is required."
#endif

#if defined(_GNU_SOURCE)
#    error "_GNU_SOURCE is not supported; use portable C/POSIX interfaces only."
#endif

#endif
