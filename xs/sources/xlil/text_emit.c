/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

#include "model_internal.h"

#include <stdlib.h>

XsLilStatus xs_lil_module_emit_text(const XsLilModule *module, XsLilText *text, XsLilError *error)
{
    xs_lil_clear_error(error);
    if(text != nullptr)
        *text = (XsLilText){0};
    if(module == nullptr || text == nullptr)
        return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL module and text output are required");

    XsLilStatus status = xs_lil_module_verify(module, error);
    if(status != XS_LIL_OK)
        return status;

    FILE *stream = tmpfile();
    if(stream == nullptr)
        return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not create temporary XLIL text stream");
    status = xs_lil_module_write_text(module, stream, error);
    if(status != XS_LIL_OK || fflush(stream) != 0 || fseek(stream, 0, SEEK_END) != 0)
    {
        fclose(stream);
        return status != XS_LIL_OK ? status : xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not measure XLIL text");
    }
    long end = ftell(stream);
    if(end < 0 || (uintmax_t)end > SIZE_MAX - 1U || fseek(stream, 0, SEEK_SET) != 0)
    {
        fclose(stream);
        return xs_lil_set_error(error, XS_LIL_IO_ERROR, "XLIL text size is not representable");
    }

    size_t length = (size_t)end;
    char *data = malloc(length + 1U);
    if(data == nullptr)
    {
        fclose(stream);
        return xs_lil_set_error(error, XS_LIL_ALLOCATION_FAILED, "out of memory while emitting XLIL text");
    }
    size_t read = fread(data, 1, length, stream);
    bool failed = read != length || ferror(stream);
    fclose(stream);
    if(failed)
    {
        free(data);
        return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not read emitted XLIL text");
    }
    data[length] = '\0';
    *text = (XsLilText){.data = data, .length = length};
    return XS_LIL_OK;
}

void xs_lil_text_destroy(XsLilText *text)
{
    if(text == nullptr)
        return;
    free(text->data);
    *text = (XsLilText){0};
}

XsLilText *xs_lil_text_create(void)
{
    return calloc(1, sizeof(XsLilText));
}

void xs_lil_text_delete(XsLilText *text)
{
    if(text == nullptr)
        return;
    xs_lil_text_destroy(text);
    free(text);
}

const char *xs_lil_text_data(const XsLilText *text)
{
    return text == nullptr ? nullptr : text->data;
}

size_t xs_lil_text_length(const XsLilText *text)
{
    return text == nullptr ? 0 : text->length;
}
