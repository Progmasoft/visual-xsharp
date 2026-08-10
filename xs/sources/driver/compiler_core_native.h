/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef XS_DRIVER_COMPILER_CORE_NATIVE_H
#define XS_DRIVER_COMPILER_CORE_NATIVE_H

#include "options.h"

#include "Visual/XSharp/compiler_core.hh"
#include "Visual/C23/diagnostic.hh"

bool xs_driver_compiler_core_native_available(const XsCompilerCoreSession *session);
bool xs_driver_append_compiler_core_diagnostics(const XsCompilerCoreSession *session, XsDiagnostics *diagnostics,
                                                XsSpan span);
bool xs_driver_build_compiler_core_native(const char *input_path, const XsCompilerCoreSession *session,
                                          XsDiagnostics *diagnostics, XsSpan span);

#endif
