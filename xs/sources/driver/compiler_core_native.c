/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "compiler_core_native.h"

#include "direct_xlil.h"

#include "xs/lil.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool xs_driver_compiler_core_native_available(const XsCompilerCoreSession *session)
{
    uint64_t length = 0;
    return xslang_compiler_core_session_xlil_text(session, &length) != nullptr && length != 0;
}

bool xs_driver_append_compiler_core_diagnostics(const XsCompilerCoreSession *session, XsDiagnostics *diagnostics,
                                                XsSpan span)
{
    bool added = false;
    uint64_t count = xslang_compiler_core_session_diagnostic_count(session);
    for(uint64_t index = 0; index < count; ++index)
    {
        uint64_t raw_length = 0;
        const uint8_t *text = xslang_compiler_core_session_diagnostic_text(session, index, &raw_length);
        if(text == nullptr || raw_length > SIZE_MAX - 1)
            continue;
        size_t length = (size_t)raw_length;
        char *message = malloc(length + 1);
        if(message == nullptr)
            continue;
        memcpy(message, text, length);
        message[length] = '\0';
        added = xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, span, message) || added;
        free(message);
    }
    return added;
}

bool xs_driver_emit_compiler_core_output(const char *output_path, const XsCompilerCoreSession *session,
                                         XsBuildOutput output, XsDiagnostics *diagnostics, XsSpan span)
{
    uint64_t raw_length = 0;
    const uint8_t *text = nullptr;
    const char *kind = nullptr;
    switch(output)
    {
    case XS_BUILD_OUTPUT_HIR:
        text = xslang_compiler_core_session_xhir_text(session, &raw_length);
        kind = "XHIR";
        break;
    case XS_BUILD_OUTPUT_MIR:
        text = xslang_compiler_core_session_xmir_text(session, &raw_length);
        kind = "XMIR";
        break;
    case XS_BUILD_OUTPUT_XLIL:
        text = xslang_compiler_core_session_xlil_text(session, &raw_length);
        kind = "XLIL";
        break;
    case XS_BUILD_OUTPUT_NONE:
        return true;
    }
    if(text == nullptr || raw_length == 0 || raw_length > SIZE_MAX)
    {
        if(!xs_driver_append_compiler_core_diagnostics(session, diagnostics, span))
            (void)xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, span,
                                     "compiler core did not produce the requested intermediate output");
        return false;
    }
    FILE *file = fopen(output_path, "wb");
    if(file == nullptr)
        return xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, span,
                                  "intermediate output file could not be opened") &&
               false;
    size_t length = (size_t)raw_length;
    bool success = fwrite(text, 1, length, file) == length;
    if(fclose(file) != 0)
        success = false;
    if(!success)
        return xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, span,
                                  "intermediate output file could not be written") &&
               false;
    printf("xs: wrote %s to %s\n", kind, output_path);
    return true;
}

bool xs_driver_build_compiler_core_native(const char *input_path, const XsCompilerCoreSession *session,
                                          XsDiagnostics *diagnostics, XsSpan span)
{
    uint64_t raw_length = 0;
    const uint8_t *text = xslang_compiler_core_session_xlil_text(session, &raw_length);
    if(text == nullptr || raw_length == 0 || raw_length > SIZE_MAX)
        return xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, span,
                                  "Rust compiler core did not produce a complete XLIL module") &&
               false;
    XsLilModule *module = nullptr;
    XsLilError error = {0};
    if(xs_lil_module_parse_text(input_path, (const char *)text, (size_t)raw_length, &module, &error) != XS_LIL_OK)
        return xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, span,
                                  error.message[0] == '\0' ? "Rust compiler-core XLIL could not be parsed"
                                                           : error.message) &&
               false;
    bool success = xs_lil_module_verify(module, &error) == XS_LIL_OK;
    if(!success)
        xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, span,
                           error.message[0] == '\0' ? "Rust compiler-core XLIL verification failed" : error.message);
    if(success)
        success = xs_driver_build_lil_module_native(input_path, module);
    if(!success && !xs_diagnostics_has_error(diagnostics))
        xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, span, "Rust compiler-core XLIL native backend failed");
    xs_lil_module_destroy(module);
    return success;
}
