/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

#include "model_internal.h"

#include <stdlib.h>
#include <string.h>

static bool valid_utf32(const uint32_t *units, size_t length)
{
    for(size_t index = 0; index < length; ++index)
    {
        uint32_t unit = units[index];
        if(unit > 0x10FFFFU || (unit >= 0xD800U && unit <= 0xDFFFU))
            return false;
    }
    return true;
}

XsLilStatus xs_lil_block_add_const_str(XsLilBlock *block, XsLilUtf32Encoding encoding, const uint32_t *units,
                                       size_t unit_count, XsLilValueId *result, XsLilError *error)
{
    xs_lil_clear_error(error);
    if(result != nullptr)
        *result = 0;
    if(block == nullptr || block->owner == nullptr || (unit_count != 0 && units == nullptr) ||
       (encoding != XS_LIL_UTF32_LE && encoding != XS_LIL_UTF32_BE))
        return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "valid XLIL const.str arguments are required");
    if(!valid_utf32(units, unit_count))
        return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL const.str must contain well-formed UTF-32");
    uint32_t *copy = nullptr;
    if(unit_count != 0)
    {
        copy = malloc(unit_count * sizeof(*copy));
        if(copy == nullptr)
            return xs_lil_set_error(error, XS_LIL_ALLOCATION_FAILED, "out of memory while adding XLIL const.str");
        memcpy(copy, units, unit_count * sizeof(*copy));
    }
    XsLilValueId value = 0;
    XsLilStatus status = xs_lil_add_value(block->owner, (XsLilType){.kind = XS_LIL_TYPE_STR}, &value, error);
    if(status == XS_LIL_OK)
        status = xs_lil_append_instruction(block,
                                           (XsLilInstruction){.kind = XS_LIL_INSTRUCTION_CONST_STR,
                                                              .result = value,
                                                              .utf32_encoding = encoding,
                                                              .utf32_units = copy,
                                                              .utf32_length = unit_count},
                                           error);
    if(status != XS_LIL_OK)
    {
        free(copy);
        return status;
    }
    if(result != nullptr)
        *result = value;
    return XS_LIL_OK;
}

XsLilUtf32Encoding xs_lil_block_instruction_utf32_encoding(const XsLilBlock *block, size_t index)
{
    if(block == nullptr || index >= block->instruction_count ||
       block->instructions[index].kind != XS_LIL_INSTRUCTION_CONST_STR)
        return XS_LIL_UTF32_LE;
    return block->instructions[index].utf32_encoding;
}

size_t xs_lil_block_instruction_utf32_length(const XsLilBlock *block, size_t index)
{
    if(block == nullptr || index >= block->instruction_count ||
       block->instructions[index].kind != XS_LIL_INSTRUCTION_CONST_STR)
        return 0;
    return block->instructions[index].utf32_length;
}

uint32_t xs_lil_block_instruction_utf32_unit(const XsLilBlock *block, size_t index, size_t unit)
{
    if(block == nullptr || index >= block->instruction_count ||
       block->instructions[index].kind != XS_LIL_INSTRUCTION_CONST_STR ||
       unit >= block->instructions[index].utf32_length)
        return 0;
    return block->instructions[index].utf32_units[unit];
}
