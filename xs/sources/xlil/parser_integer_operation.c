/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "model_internal.h"

#include <string.h>

static bool parse_register(const char *text, size_t length, XsLilValueId *value)
{
    if(length < 3 || text[0] != '%' || text[1] != 'r')
        return false;
    uint32_t parsed = 0;
    for(size_t index = 2; index < length; ++index)
    {
        if(text[index] < '0' || text[index] > '9')
            return false;
        uint32_t digit = (uint32_t)(text[index] - '0');
        if(parsed > (UINT32_MAX - digit) / 10U)
            return false;
        parsed = parsed * 10U + digit;
    }
    *value = parsed;
    return true;
}

static bool parse_operands(const char *text, size_t length, XsLilValueId *left, XsLilValueId *right)
{
    const char *comma = memchr(text, ',', length);
    if(comma == nullptr)
        return false;
    size_t left_length = (size_t)(comma - text);
    const char *right_text = comma + 1;
    const char *end = text + length;
    while(right_text < end && *right_text == ' ')
        ++right_text;
    return parse_register(text, left_length, left) && parse_register(right_text, (size_t)(end - right_text), right);
}

static bool parse_name(const char *text, size_t length, XsLilIntegerBinaryOperation *operation, XsLilType *type)
{
    static const XsLilTypeKind types[] = {XS_LIL_TYPE_U8,  XS_LIL_TYPE_I8,  XS_LIL_TYPE_U16,  XS_LIL_TYPE_I16,
                                          XS_LIL_TYPE_U32, XS_LIL_TYPE_U64, XS_LIL_TYPE_U128, XS_LIL_TYPE_I128};
    for(unsigned candidate = XS_LIL_INTEGER_ADD; candidate <= XS_LIL_INTEGER_GREATER_EQUAL; ++candidate)
    {
        const char *stem = xs_lil_integer_operation_name((XsLilIntegerBinaryOperation)candidate);
        size_t stem_length = strlen(stem);
        for(size_t type_index = 0; type_index < sizeof(types) / sizeof(types[0]); ++type_index)
        {
            XsLilType candidate_type = {.kind = types[type_index]};
            const char *type_name = xs_lil_type_name(candidate_type);
            size_t type_length = strlen(type_name);
            if(length == stem_length + 1U + type_length && memcmp(text, stem, stem_length) == 0 &&
               text[stem_length] == '.' && memcmp(text + stem_length + 1U, type_name, type_length) == 0)
            {
                *operation = (XsLilIntegerBinaryOperation)candidate;
                *type = candidate_type;
                return true;
            }
        }
    }
    return false;
}

XsLilStatus xs_lil_parse_integer_operation(XsLilBlock *block, XsLilType result_type, const char *operation,
                                           size_t operation_length, XsLilValueId expected_result, bool *matched,
                                           XsLilError *error)
{
    *matched = false;
    const char *space = memchr(operation, ' ', operation_length);
    if(space == nullptr)
        return XS_LIL_OK;
    XsLilIntegerBinaryOperation kind = XS_LIL_INTEGER_ADD;
    XsLilType operand_type = {.kind = XS_LIL_TYPE_VOID};
    if(!parse_name(operation, (size_t)(space - operation), &kind, &operand_type))
        return XS_LIL_OK;
    *matched = true;
    XsLilType expected_type =
        xs_lil_integer_operation_is_comparison(kind) ? (XsLilType){.kind = XS_LIL_TYPE_BOOL} : operand_type;
    if(result_type.kind != expected_type.kind)
        return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL integer operation result type is invalid");
    XsLilValueId left = 0;
    XsLilValueId right = 0;
    if(!parse_operands(space + 1, operation_length - (size_t)(space + 1 - operation), &left, &right))
        return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL integer operation operands are invalid");
    XsLilValueId actual = 0;
    XsLilStatus status = xs_lil_block_binary_integer(block, kind, operand_type, left, right, &actual, error);
    if(status != XS_LIL_OK)
        return status;
    return actual == expected_result
               ? XS_LIL_OK
               : xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL value ids must be sequential");
}
