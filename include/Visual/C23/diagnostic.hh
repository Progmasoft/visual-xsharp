/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 * Shared C and C++ ABI surface.
 *
 */

#ifndef XS_DIAGNOSTIC_H
#define XS_DIAGNOSTIC_H

#include "Visual/C23/source.hh"

#include <stddef.h>
#include <stdio.h>

typedef enum
{
    XS_DIAGNOSTIC_ERROR,
    XS_DIAGNOSTIC_WARNING,
    XS_DIAGNOSTIC_NOTE,
} XsDiagnosticSeverity;

typedef enum
{
    XS_WARNING_NONE,
    XS_WARNING_LOW,
    XS_WARNING_MEDIUM,
    XS_WARNING_ALL,
} XsWarningLevel;

typedef struct
{
    XsDiagnosticSeverity severity;
    XsWarningLevel warning_level;
    XsSpan span;
    char *message;
} XsDiagnostic;

typedef struct
{
    XsDiagnostic *items;
    size_t count;
    size_t capacity;
    XsWarningLevel warning_level;
    bool warnings_as_errors;
    bool allocation_failed;
} XsDiagnostics;

#ifdef __cplusplus
extern "C" {
#endif

void xs_diagnostics_init(XsDiagnostics *diagnostics);
void xs_diagnostics_set_warning_policy(XsDiagnostics *diagnostics, XsWarningLevel level, bool warnings_as_errors);
void xs_diagnostics_free(XsDiagnostics *diagnostics);
bool xs_diagnostics_add(XsDiagnostics *diagnostics, XsDiagnosticSeverity severity, XsSpan span, const char *message);
bool xs_diagnostics_add_warning(XsDiagnostics *diagnostics, XsWarningLevel level, XsSpan span, const char *message);
bool xs_diagnostics_has_error(const XsDiagnostics *diagnostics);
void xs_diagnostics_print(const XsDiagnostics *diagnostics, const XsSource *source, FILE *stream);

#ifdef __cplusplus
}
#endif

#endif
