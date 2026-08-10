// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

#include "Visual/C23/diagnostic.hh"

#include <cstdlib>
#include <cstring>

void xs_diagnostics_init(XsDiagnostics *diagnostics)
{
    *diagnostics = XsDiagnostics{};
    diagnostics->warning_level = XS_WARNING_MEDIUM;
}

void xs_diagnostics_set_warning_policy(XsDiagnostics *diagnostics, XsWarningLevel level, bool warnings_as_errors)
{
    diagnostics->warning_level = level;
    diagnostics->warnings_as_errors = warnings_as_errors;
}

void xs_diagnostics_free(XsDiagnostics *diagnostics)
{
    for(std::size_t index = 0; index < diagnostics->count; ++index)
        std::free(diagnostics->items[index].message);
    std::free(diagnostics->items);
    *diagnostics = XsDiagnostics{};
}

namespace
{
auto add_diagnostic(XsDiagnostics *diagnostics, XsDiagnosticSeverity severity, XsWarningLevel level, XsSpan span,
                    const char *message) -> bool
{
    if(diagnostics->count == diagnostics->capacity)
    {
        const std::size_t capacity = diagnostics->capacity == 0 ? 8 : diagnostics->capacity * 2;
        auto *items = static_cast<XsDiagnostic *>(std::realloc(diagnostics->items, capacity * sizeof(XsDiagnostic)));
        if(items == nullptr)
        {
            diagnostics->allocation_failed = true;
            return false;
        }
        diagnostics->items = items;
        diagnostics->capacity = capacity;
    }

    const std::size_t length = std::strlen(message);
    auto *copy = static_cast<char *>(std::malloc(length + 1));
    if(copy == nullptr)
    {
        diagnostics->allocation_failed = true;
        return false;
    }
    std::memcpy(copy, message, length + 1);
    diagnostics->items[diagnostics->count++] = XsDiagnostic{severity, level, span, copy};
    return true;
}

auto warning_enabled(const XsDiagnostics *diagnostics, const XsDiagnostic *diagnostic) -> bool
{
    return diagnostic->severity != XS_DIAGNOSTIC_WARNING ||
           (diagnostics->warning_level != XS_WARNING_NONE && diagnostics->warning_level >= diagnostic->warning_level);
}
} // namespace

bool xs_diagnostics_add(XsDiagnostics *diagnostics, XsDiagnosticSeverity severity, XsSpan span, const char *message)
{
    const XsWarningLevel level = severity == XS_DIAGNOSTIC_WARNING ? XS_WARNING_LOW : XS_WARNING_NONE;
    return add_diagnostic(diagnostics, severity, level, span, message);
}

bool xs_diagnostics_add_warning(XsDiagnostics *diagnostics, XsWarningLevel level, XsSpan span, const char *message)
{
    if(level < XS_WARNING_LOW || level > XS_WARNING_ALL)
        return false;
    return add_diagnostic(diagnostics, XS_DIAGNOSTIC_WARNING, level, span, message);
}

bool xs_diagnostics_has_error(const XsDiagnostics *diagnostics)
{
    for(std::size_t index = 0; index < diagnostics->count; ++index)
    {
        const auto &diagnostic = diagnostics->items[index];
        if(diagnostic.severity == XS_DIAGNOSTIC_ERROR ||
           (diagnostic.severity == XS_DIAGNOSTIC_WARNING && diagnostics->warnings_as_errors &&
            warning_enabled(diagnostics, &diagnostic)))
            return true;
    }
    return diagnostics->allocation_failed;
}

void xs_diagnostics_print(const XsDiagnostics *diagnostics, const XsSource *source, FILE *stream)
{
    static constexpr const char *severities[] = {"error", "warning", "note"};
    for(std::size_t index = 0; index < diagnostics->count; ++index)
    {
        const auto &diagnostic = diagnostics->items[index];
        if(!warning_enabled(diagnostics, &diagnostic))
            continue;
        std::size_t line = 1;
        std::size_t column = 1;
        const std::size_t limit = diagnostic.span.start < source->length ? diagnostic.span.start : source->length;
        for(std::size_t offset = 0; offset < limit; ++offset)
        {
            if(source->text[offset] == '\n')
            {
                ++line;
                column = 1;
            }
            else
                ++column;
        }
        fprintf(stream, "%s:%zu:%zu: %s: %s\n", source->path, line, column, severities[diagnostic.severity],
                diagnostic.message);
    }
    if(diagnostics->allocation_failed)
        fprintf(stream, "%s: error: compiler ran out of memory while recording diagnostics\n", source->path);
}
