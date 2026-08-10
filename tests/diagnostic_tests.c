/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "Visual/C23/diagnostic.hh"

#include <stdio.h>

static int failures = 0;

#define CHECK(condition)                                                                                               \
    do                                                                                                                 \
    {                                                                                                                  \
        if(!(condition))                                                                                               \
        {                                                                                                              \
            fprintf(stderr, "check failed at %s:%d: %s\n", __FILE__, __LINE__, #condition);                            \
            ++failures;                                                                                                \
        }                                                                                                              \
    } while(false)

static void test_warning_policy(void)
{
    XsDiagnostics diagnostics;
    xs_diagnostics_init(&diagnostics);
    xs_diagnostics_set_warning_policy(&diagnostics, XS_WARNING_MEDIUM, true);
    CHECK(xs_diagnostics_add_warning(&diagnostics, XS_WARNING_ALL, (XsSpan){0}, "pedantic warning"));
    CHECK(!xs_diagnostics_has_error(&diagnostics));
    CHECK(xs_diagnostics_add_warning(&diagnostics, XS_WARNING_MEDIUM, (XsSpan){0}, "regular warning"));
    CHECK(xs_diagnostics_has_error(&diagnostics));
    xs_diagnostics_set_warning_policy(&diagnostics, XS_WARNING_NONE, true);
    CHECK(!xs_diagnostics_has_error(&diagnostics));
    xs_diagnostics_free(&diagnostics);
}

static void test_note_is_not_an_error(void)
{
    XsDiagnostics diagnostics;
    xs_diagnostics_init(&diagnostics);
    CHECK(xs_diagnostics_add(&diagnostics, XS_DIAGNOSTIC_NOTE, (XsSpan){0}, "context"));
    CHECK(!xs_diagnostics_has_error(&diagnostics));
    CHECK(diagnostics.items[0].severity == XS_DIAGNOSTIC_NOTE);
    xs_diagnostics_free(&diagnostics);
}

int main(void)
{
    test_warning_policy();
    test_note_is_not_an_error();
    return failures == 0 ? 0 : 1;
}
