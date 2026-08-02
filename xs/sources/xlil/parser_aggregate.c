/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "model_internal.h"

#include <stdlib.h>
#include <string.h>

static bool parse_u32_prefix(const char *start, const char *end, uint32_t *value, const char **tail)
{
    if(start >= end || *start < '0' || *start > '9')
        return false;
    uint32_t result = 0;
    const char *cursor = start;
    while(cursor < end && *cursor >= '0' && *cursor <= '9')
    {
        uint32_t digit = (uint32_t)(*cursor - '0');
        if(result > (UINT32_MAX - digit) / 10U)
            return false;
        result = result * 10U + digit;
        ++cursor;
    }
    *value = result;
    *tail = cursor;
    return true;
}

static const char *find_record_separator(const char *start, const char *end)
{
    for(const char *cursor = start; end - cursor >= 4; ++cursor)
    {
        if(cursor[0] == ' ' && cursor[1] == ':' && cursor[2] == ' ' && cursor[3] == '(')
            return cursor;
    }
    return nullptr;
}

bool xs_lil_parse_type_name(const XsLilModule *module, const char *text, size_t length, XsLilType *type)
{
    for(unsigned kind = XS_LIL_TYPE_VOID; kind < XS_LIL_TYPE_AGGREGATE; ++kind)
    {
        XsLilType candidate = {.kind = (XsLilTypeKind)kind};
        const char *name = xs_lil_type_name(candidate);
        if(strlen(name) == length && strncmp(text, name, length) == 0)
        {
            *type = candidate;
            return true;
        }
    }
    if(length < 3 || text[0] != '%' || (text[1] != 't' && text[1] != 'a'))
        return false;
    uint32_t id = 0;
    for(size_t i = 2; i < length; ++i)
    {
        if(text[i] < '0' || text[i] > '9')
            return false;
        uint32_t digit = (uint32_t)(text[i] - '0');
        if(id > (UINT32_MAX - digit) / 10U)
            return false;
        id = id * 10U + digit;
    }
    bool aggregate = text[1] == 't';
    size_t count = aggregate ? xs_lil_module_aggregate_type_count(module) : xs_lil_module_array_type_count(module);
    if(module == nullptr || id >= count)
        return false;
    *type = aggregate ? xs_lil_aggregate_type(id) : xs_lil_array_type(id);
    return true;
}

static bool append_field(XsLilType **fields, size_t *count, XsLilType field)
{
    XsLilType *grown = realloc(*fields, (*count + 1) * sizeof(*grown));
    if(grown == nullptr)
        return false;
    *fields = grown;
    (*fields)[(*count)++] = field;
    return true;
}

XsLilStatus xs_lil_parse_aggregate_record(XsLilModule *module, const char *text, size_t length, XsLilError *error)
{
    const char *end = text + length;
    const char *cursor = text + strlen(".type %t");
    const char *id_end = nullptr;
    uint32_t id = 0;
    if(!parse_u32_prefix(cursor, end, &id, &id_end) || id != xs_lil_module_aggregate_type_count(module) ||
       id_end >= end || *id_end != ' ')
        return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL aggregate type id is invalid or non-sequential");
    cursor = id_end + 1;
    const char *separator = find_record_separator(cursor, end);
    if(separator == nullptr || separator == cursor || end <= separator + 4 || end[-1] != ')')
        return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL aggregate type record is invalid");
    char *name = xs_lil_copy_span(cursor, (size_t)(separator - cursor));
    XsLilType *fields = nullptr;
    size_t field_count = 0;
    cursor = separator + 4;
    while(cursor < end - 1)
    {
        const char *field_end = cursor;
        while(field_end < end - 1 && *field_end != ',')
            ++field_end;
        XsLilType field = {0};
        if(!xs_lil_parse_type_name(module, cursor, (size_t)(field_end - cursor), &field) ||
           !append_field(&fields, &field_count, field))
        {
            free(name);
            free(fields);
            return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL aggregate field type is invalid");
        }
        cursor = field_end == end - 1 ? field_end : field_end + 1;
        if(cursor < end - 1 && *cursor == ' ')
            ++cursor;
    }
    XsLilType result = {0};
    XsLilStatus status = xs_lil_module_add_aggregate_type(module, name, fields, field_count, &result, error);
    free(name);
    free(fields);
    return status;
}

static bool parse_value(const char **cursor, const char *end, XsLilValueId *value)
{
    if(end - *cursor < 3 || (*cursor)[0] != '%' || (*cursor)[1] != 'r')
        return false;
    const char *start = *cursor + 2;
    const char *number_end = nullptr;
    uint32_t parsed = 0;
    if(!parse_u32_prefix(start, end, &parsed, &number_end))
        return false;
    *cursor = number_end;
    *value = (XsLilValueId)parsed;
    return true;
}

static bool append_value(XsLilValueId **values, size_t *count, XsLilValueId value)
{
    XsLilValueId *grown = realloc(*values, (*count + 1) * sizeof(*grown));
    if(grown == nullptr)
        return false;
    *values = grown;
    (*values)[(*count)++] = value;
    return true;
}

static XsLilStatus parse_composite_values(XsLilBlock *block, XsLilType result_type, const char *cursor, const char *end,
                                          XsLilValueId expected, bool array, XsLilError *error)
{
    XsLilValueId *fields = nullptr;
    size_t field_count = 0;
    while(cursor < end)
    {
        XsLilValueId field = UINT32_MAX;
        if(!parse_value(&cursor, end, &field) || !append_value(&fields, &field_count, field))
        {
            free(fields);
            return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL aggregate field register is invalid");
        }
        if(cursor == end)
            break;
        if(end - cursor < 2 || cursor[0] != ',' || cursor[1] != ' ')
        {
            free(fields);
            return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL aggregate field list is invalid");
        }
        cursor += 2;
    }
    XsLilValueId result = UINT32_MAX;
    XsLilStatus status = array ? xs_lil_block_add_array(block, result_type, fields, field_count, &result, error)
                               : xs_lil_block_add_aggregate(block, result_type, fields, field_count, &result, error);
    free(fields);
    if(status != XS_LIL_OK)
        return status;
    return result == expected ? XS_LIL_OK
                              : xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL value ids must be sequential");
}

XsLilStatus xs_lil_parse_aggregate_instruction(XsLilBlock *block, XsLilType result_type, const char *operation,
                                               size_t operation_length, XsLilValueId expected_result, bool *matched,
                                               XsLilError *error)
{
    const char *end = operation + operation_length;
    if(operation_length >= 10 && strncmp(operation, "len.array ", 10) == 0)
    {
        *matched = true;
        const char *cursor = operation + 10;
        XsLilValueId array = UINT32_MAX;
        if(result_type.kind != XS_LIL_TYPE_I64 || !parse_value(&cursor, end, &array) || cursor != end)
            return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL array length instruction is invalid");
        XsLilValueId result = UINT32_MAX;
        XsLilStatus status = xs_lil_block_add_array_length(block, array, &result, error);
        if(status != XS_LIL_OK)
            return status;
        return result == expected_result
                   ? XS_LIL_OK
                   : xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL value ids must be sequential");
    }
    bool array_get = operation_length >= 10 && strncmp(operation, "array.get ", 10) == 0;
    bool array_set = operation_length >= 10 && strncmp(operation, "array.set ", 10) == 0;
    if(array_get || array_set)
    {
        *matched = true;
        const char *cursor = operation + 10;
        XsLilValueId array = UINT32_MAX;
        XsLilValueId index = UINT32_MAX;
        XsLilValueId replacement = UINT32_MAX;
        if(!parse_value(&cursor, end, &array) || end - cursor < 3 || cursor[0] != ',' || cursor[1] != ' ')
            return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL dynamic array source is invalid");
        cursor += 2;
        if(!parse_value(&cursor, end, &index))
            return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL dynamic array index is invalid");
        if(array_set)
        {
            if(end - cursor < 3 || cursor[0] != ',' || cursor[1] != ' ')
                return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL array.set value is missing");
            cursor += 2;
            if(!parse_value(&cursor, end, &replacement))
                return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL array.set value is invalid");
        }
        if(cursor != end)
            return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL dynamic array operands are malformed");
        XsLilValueId result = UINT32_MAX;
        XsLilStatus status = array_get ? xs_lil_block_add_array_get(block, array, index, result_type, &result, error)
                                       : xs_lil_block_add_array_set(block, array, index, replacement, &result, error);
        if(status != XS_LIL_OK)
            return status;
        return result == expected_result
                   ? XS_LIL_OK
                   : xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL value ids must be sequential");
    }
    if(operation_length >= 10 && strncmp(operation, "aggregate ", 10) == 0)
    {
        *matched = true;
        if(result_type.kind != XS_LIL_TYPE_AGGREGATE)
            return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL aggregate result type is invalid");
        return parse_composite_values(block, result_type, operation + 10, end, expected_result, false, error);
    }
    if(operation_length >= 6 && strncmp(operation, "array ", 6) == 0)
    {
        *matched = true;
        if(result_type.kind != XS_LIL_TYPE_ARRAY)
            return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL array result type is invalid");
        return parse_composite_values(block, result_type, operation + 6, end, expected_result, true, error);
    }
    const char *extract_prefix = nullptr;
    size_t extract_prefix_length = 0;
    if(operation_length >= 14 && strncmp(operation, "extract.array ", 14) == 0)
    {
        extract_prefix = "extract.array ";
        extract_prefix_length = 14;
    }
    else if(operation_length >= 8 && strncmp(operation, "extract ", 8) == 0)
    {
        extract_prefix = "extract ";
        extract_prefix_length = 8;
    }
    if(extract_prefix != nullptr)
    {
        *matched = true;
        const char *cursor = operation + extract_prefix_length;
        XsLilValueId aggregate = UINT32_MAX;
        if(!parse_value(&cursor, end, &aggregate) || end - cursor < 3 || cursor[0] != ',' || cursor[1] != ' ')
            return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL extract source is invalid");
        cursor += 2;
        const char *field_end = nullptr;
        uint32_t field = 0;
        if(!parse_u32_prefix(cursor, end, &field, &field_end) || field_end != end)
            return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL extract field index is invalid");
        XsLilValueId result = UINT32_MAX;
        XsLilStatus status = xs_lil_block_add_extract(block, aggregate, (uint32_t)field, result_type, &result, error);
        if(status != XS_LIL_OK)
            return status;
        return result == expected_result
                   ? XS_LIL_OK
                   : xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL value ids must be sequential");
    }
    *matched = false;
    return XS_LIL_OK;
}
