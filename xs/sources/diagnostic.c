/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "xs/diagnostic.h"

#include <stdlib.h>
#include <string.h>

void xs_diagnostics_init(XsDiagnostics *diagnostics)
{
    *diagnostics = (XsDiagnostics){.warning_level = XS_WARNING_MEDIUM};
}

void xs_diagnostics_set_warning_policy(XsDiagnostics *diagnostics, XsWarningLevel level, bool warnings_as_errors)
{
    diagnostics->warning_level = level;
    diagnostics->warnings_as_errors = warnings_as_errors;
}

void xs_diagnostics_free(XsDiagnostics *diagnostics)
{
    for(size_t i = 0; i < diagnostics->count; ++i)
        free(diagnostics->items[i].message);
    free(diagnostics->items);
    *diagnostics = (XsDiagnostics){0};
}

static bool add_diagnostic(XsDiagnostics *diagnostics, XsDiagnosticSeverity severity, XsWarningLevel level, XsSpan span,
                           const char *message)
{
    if(diagnostics->count == diagnostics->capacity)
    {
        size_t capacity = diagnostics->capacity == 0 ? 8 : diagnostics->capacity * 2;
        XsDiagnostic *items = realloc(diagnostics->items, capacity * sizeof(*items));
        if(items == nullptr)
        {
            diagnostics->allocation_failed = true;
            return false;
        }
        diagnostics->items = items;
        diagnostics->capacity = capacity;
    }

    size_t length = strlen(message);
    char *copy = malloc(length + 1);
    if(copy == nullptr)
    {
        diagnostics->allocation_failed = true;
        return false;
    }
    memcpy(copy, message, length + 1);
    diagnostics->items[diagnostics->count++] =
        (XsDiagnostic){.severity = severity, .warning_level = level, .span = span, .message = copy};
    return true;
}

bool xs_diagnostics_add(XsDiagnostics *diagnostics, XsDiagnosticSeverity severity, XsSpan span, const char *message)
{
    XsWarningLevel level = severity == XS_DIAGNOSTIC_WARNING ? XS_WARNING_LOW : XS_WARNING_NONE;
    return add_diagnostic(diagnostics, severity, level, span, message);
}

bool xs_diagnostics_add_warning(XsDiagnostics *diagnostics, XsWarningLevel level, XsSpan span, const char *message)
{
    if(level < XS_WARNING_LOW || level > XS_WARNING_ALL)
        return false;
    return add_diagnostic(diagnostics, XS_DIAGNOSTIC_WARNING, level, span, message);
}

static bool warning_enabled(const XsDiagnostics *diagnostics, const XsDiagnostic *diagnostic)
{
    return diagnostic->severity != XS_DIAGNOSTIC_WARNING ||
           (diagnostics->warning_level != XS_WARNING_NONE && diagnostics->warning_level >= diagnostic->warning_level);
}

bool xs_diagnostics_has_error(const XsDiagnostics *diagnostics)
{
    for(size_t i = 0; i < diagnostics->count; ++i)
    {
        if(diagnostics->items[i].severity == XS_DIAGNOSTIC_ERROR ||
           (diagnostics->items[i].severity == XS_DIAGNOSTIC_WARNING && diagnostics->warnings_as_errors &&
            warning_enabled(diagnostics, &diagnostics->items[i])))
            return true;
    }
    return diagnostics->allocation_failed;
}

void xs_diagnostics_print(const XsDiagnostics *diagnostics, const XsSource *source, FILE *stream)
{
    static const char *const severities[] = {"error", "warning", "note"};
    for(size_t i = 0; i < diagnostics->count; ++i)
    {
        XsDiagnostic diagnostic = diagnostics->items[i];
        if(!warning_enabled(diagnostics, &diagnostic))
            continue;
        size_t line = 1;
        size_t column = 1;
        size_t limit = diagnostic.span.start < source->length ? diagnostic.span.start : source->length;
        for(size_t offset = 0; offset < limit; ++offset)
        {
            if(source->text[offset] == '\n')
            {
                ++line;
                column = 1;
            }
            else
            {
                ++column;
            }
        }
        fprintf(stream, "%s:%zu:%zu: %s: %s\n", source->path, line, column, severities[diagnostic.severity],
                diagnostic.message);
    }
    if(diagnostics->allocation_failed)
        fprintf(stream, "%s: error: compiler ran out of memory while recording diagnostics\n", source->path);
}
