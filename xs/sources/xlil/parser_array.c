/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "model_internal.h"

#include <string.h>

static bool parse_u64_prefix(const char *start, const char *end, uint64_t *value, const char **tail)
{
    if(start >= end || *start < '0' || *start > '9')
        return false;
    uint64_t result = 0;
    const char *cursor = start;
    while(cursor < end && *cursor >= '0' && *cursor <= '9')
    {
        uint64_t digit = (uint64_t)(*cursor - '0');
        if(result > (UINT64_MAX - digit) / 10U)
            return false;
        result = result * 10U + digit;
        ++cursor;
    }
    *value = result;
    *tail = cursor;
    return true;
}

static const char *find_separator(const char *start, const char *end)
{
    for(const char *cursor = start; end - cursor >= 3; ++cursor)
    {
        if(cursor[0] == ' ' && cursor[1] == 'x' && cursor[2] == ' ')
            return cursor;
    }
    return nullptr;
}

XsLilStatus xs_lil_parse_array_record(XsLilModule *module, const char *text, size_t length, XsLilError *error)
{
    const char *end = text + length;
    const char *cursor = text + strlen(".array %a");
    const char *id_end = nullptr;
    uint64_t id = 0;
    if(!parse_u64_prefix(cursor, end, &id, &id_end) || id > UINT32_MAX ||
       id != xs_lil_module_array_type_count(module) || end - id_end < 4 || strncmp(id_end, " : ", 3) != 0)
        return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL array type id is invalid or non-sequential");
    cursor = id_end + 3;
    const char *separator = find_separator(cursor, end);
    XsLilType element_type = {0};
    const char *element_end = separator == nullptr ? end : separator;
    if(element_end == cursor || !xs_lil_parse_type_name(module, cursor, (size_t)(element_end - cursor), &element_type))
        return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL array element type is invalid");
    XsLilType result = {0};
    if(separator == nullptr)
        return xs_lil_module_add_dynamic_array_type(module, element_type, &result, error);
    if(separator + 3 >= end)
        return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL array type record is invalid");
    const char *length_end = nullptr;
    uint64_t array_length = 0;
    if(!parse_u64_prefix(separator + 3, end, &array_length, &length_end) || length_end != end || array_length == 0)
        return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL array length is invalid");
    return xs_lil_module_add_array_type(module, element_type, (uint64_t)array_length, &result, error);
}
