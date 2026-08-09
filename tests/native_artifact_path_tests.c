/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "../xs/sources/driver/native_artifact.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

static void expect_path(const char *input, const char *extension, const char *expected)
{
    char *actual = xs_driver_native_artifact_path(input, extension);
    if(actual == nullptr || strcmp(actual, expected) != 0)
    {
        fprintf(stderr, "artifact path for '%s': expected '%s', got '%s'\n", input, expected,
                actual == nullptr ? "<null>" : actual);
        ++failures;
    }
    free(actual);
}

int main(void)
{
    expect_path("Sources/Main.vxs", ".vxse", "Sources/Main.vxse");
    expect_path("build/Typed.core", ".vxse", "build/Typed.vxse");
    expect_path("build/Prepared.xpp", ".vxse", "build/Prepared.vxse");
    expect_path("build/Lowered.xmm", ".vxse", "build/Lowered.vxse");

    /* Legacy IR names are ordinary basenames, not public pipeline extensions. */
    expect_path("legacy/Module.xhir", ".vxse", "legacy/Module.xhir.vxse");
    expect_path("legacy/Module.xmir", ".vxse", "legacy/Module.xmir.vxse");
    expect_path("legacy/Module.xlil", ".vxse", "legacy/Module.xlil.vxse");
    return failures == 0 ? 0 : 1;
}
