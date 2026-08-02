/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "model_internal.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool append_unit(uint32_t **units, size_t *length, size_t *capacity, uint32_t unit)
{
    if(*length == *capacity)
    {
        size_t next = *capacity == 0 ? 8 : *capacity * 2;
        uint32_t *grown = realloc(*units, next * sizeof(*grown));
        if(grown == nullptr)
            return false;
        *units = grown;
        *capacity = next;
    }
    (*units)[(*length)++] = unit;
    return true;
}

XsLilStatus xs_lil_parse_str_comparison(XsLilBlock *block, XsLilType result_type, const char *operation,
                                        size_t operation_length, XsLilValueId expected_result, bool *matched,
                                        XsLilError *error)
{
    static const char equal_prefix[] = "eq.str ";
    static const char not_equal_prefix[] = "ne.str ";
    *matched = false;
    XsLilStrComparisonOperation comparison = XS_LIL_STR_EQ;
    size_t prefix_length = 0;
    if(operation_length > sizeof(equal_prefix) - 1U && memcmp(operation, equal_prefix, sizeof(equal_prefix) - 1U) == 0)
        prefix_length = sizeof(equal_prefix) - 1U;
    else if(operation_length > sizeof(not_equal_prefix) - 1U &&
            memcmp(operation, not_equal_prefix, sizeof(not_equal_prefix) - 1U) == 0)
    {
        comparison = XS_LIL_STR_NE;
        prefix_length = sizeof(not_equal_prefix) - 1U;
    }
    else
        return XS_LIL_OK;
    *matched = true;
    if(result_type.kind != XS_LIL_TYPE_BOOL || operation_length - prefix_length > (size_t)INT_MAX)
        return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL Str comparison record is invalid");
    size_t operand_length = operation_length - prefix_length;
    char *operands = xs_lil_copy_span(operation + prefix_length, operand_length);
    if(operands == nullptr)
        return xs_lil_set_error(error, XS_LIL_ALLOCATION_FAILED, "out of memory while parsing XLIL Str comparison");
    unsigned left = 0;
    unsigned right = 0;
    int consumed = 0;
    int fields = sscanf(operands, "%%r%u, %%r%u%n", &left, &right, &consumed);
    free(operands);
    if(fields != 2 || consumed < 0 || (size_t)consumed != operand_length)
        return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL Str comparison operands are invalid");
    XsLilValueId actual = 0;
    XsLilStatus status = xs_lil_block_compare_str(block, comparison, left, right, &actual, error);
    if(status != XS_LIL_OK)
        return status;
    return actual == expected_result
               ? XS_LIL_OK
               : xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL value ids must be sequential");
}

XsLilStatus xs_lil_parse_const_str(XsLilBlock *block, XsLilType result_type, const char *operation,
                                   size_t operation_length, XsLilValueId expected_result, bool *matched,
                                   XsLilError *error)
{
    static const char le[] = "const.str utf32le [";
    static const char be[] = "const.str utf32be [";
    *matched = false;
    XsLilUtf32Encoding encoding = XS_LIL_UTF32_LE;
    size_t prefix = 0;
    if(operation_length >= sizeof(le) - 1U && memcmp(operation, le, sizeof(le) - 1U) == 0)
        prefix = sizeof(le) - 1U;
    else if(operation_length >= sizeof(be) - 1U && memcmp(operation, be, sizeof(be) - 1U) == 0)
    {
        prefix = sizeof(be) - 1U;
        encoding = XS_LIL_UTF32_BE;
    }
    else
        return XS_LIL_OK;
    *matched = true;
    if(result_type.kind != XS_LIL_TYPE_STR || operation_length <= prefix || operation[operation_length - 1] != ']')
        return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL const.str record is invalid");
    const char *cursor = operation + prefix;
    const char *end = operation + operation_length - 1;
    uint32_t *units = nullptr;
    size_t length = 0;
    size_t capacity = 0;
    while(cursor < end)
    {
        if(end - cursor < 10 || cursor[0] != '0' || cursor[1] != 'x')
            goto invalid;
        char digits[9] = {cursor[2], cursor[3], cursor[4], cursor[5], cursor[6], cursor[7], cursor[8], cursor[9], '\0'};
        char *tail = nullptr;
        errno = 0;
        unsigned long value = strtoul(digits, &tail, 16);
        if(errno != 0 || tail == digits || *tail != '\0')
            goto invalid;
        if(value > UINT32_MAX || !append_unit(&units, &length, &capacity, (uint32_t)value))
        {
            free(units);
            return xs_lil_set_error(error, XS_LIL_ALLOCATION_FAILED, "out of memory while parsing XLIL const.str");
        }
        cursor += 10;
        while(cursor < end && (*cursor == ' ' || *cursor == '\t'))
            ++cursor;
        if(cursor == end)
            break;
        if(*cursor != ',')
            goto invalid;
        ++cursor;
        if(cursor >= end || *cursor != ' ')
            goto invalid;
        while(cursor < end && *cursor == ' ')
            ++cursor;
        if(cursor == end)
            goto invalid;
    }
    XsLilValueId actual = 0;
    XsLilStatus status = xs_lil_block_add_const_str(block, encoding, units, length, &actual, error);
    free(units);
    if(status != XS_LIL_OK)
        return status;
    return actual == expected_result
               ? XS_LIL_OK
               : xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL value ids must be sequential");
invalid:
    free(units);
    return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL const.str UTF-32 code points are invalid");
}
