/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 * Shared C and C++ ABI surface.
 *
 */

#ifndef XS_LIL_C_TEXT_H
#define XS_LIL_C_TEXT_H

#include <stdio.h>

#include "Visual/XSharp/Legacy/lil-c/module.hh"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct
    {
        char *data;
        size_t length;
    } XsLilText;

    XS_LIL_API const char *
    xs_lil_type_name(XsLilType type);
    XS_LIL_API XsLilStatus
    xs_lil_module_write_text(const XsLilModule *module, FILE *stream, XsLilError *error);
    XS_LIL_API XsLilStatus
    xs_lil_module_emit_text(const XsLilModule *module, XsLilText *text, XsLilError *error);
    XS_LIL_API XsLilText *
    xs_lil_text_create(void);
    XS_LIL_API void
    xs_lil_text_destroy(XsLilText *text);
    XS_LIL_API void
    xs_lil_text_delete(XsLilText *text);
    XS_LIL_API const char *
    xs_lil_text_data(const XsLilText *text);
    XS_LIL_API size_t
    xs_lil_text_length(const XsLilText *text);

#ifdef __cplusplus
}
#endif

#endif
