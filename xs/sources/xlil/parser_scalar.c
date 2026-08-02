/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "model_internal.h"

#include <string.h>

typedef struct
{
    XsLilTypeKind type;
    const char *prefix;
    size_t prefix_length;
    size_t digits;
} IntegerRecord;

static const IntegerRecord integer_records[] = {
    {XS_LIL_TYPE_U8, "const.u8 0x", sizeof("const.u8 0x") - 1U, 2},
    {XS_LIL_TYPE_I8, "const.i8 0x", sizeof("const.i8 0x") - 1U, 2},
    {XS_LIL_TYPE_I16, "const.i16 0x", sizeof("const.i16 0x") - 1U, 4},
    {XS_LIL_TYPE_U32, "const.u32 0x", sizeof("const.u32 0x") - 1U, 8},
    {XS_LIL_TYPE_U64, "const.u64 0x", sizeof("const.u64 0x") - 1U, 16},
    {XS_LIL_TYPE_U128, "const.u128 0x", sizeof("const.u128 0x") - 1U, 32},
    {XS_LIL_TYPE_I128, "const.i128 0x", sizeof("const.i128 0x") - 1U, 32},
};

static XsLilStatus append_bits(XsLilBlock *block, XsLilType type, XsUInt128 bits, XsLilValueId expected_result,
                               XsLilError *error)
{
    XsLilValueId actual = 0;
    XsLilStatus status = xs_lil_add_const_integer_bits(block, type, bits, &actual, error);
    if(status != XS_LIL_OK)
        return status;
    return actual == expected_result
               ? XS_LIL_OK
               : xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL value ids must be sequential");
}

XsLilStatus xs_lil_parse_const_u16(XsLilBlock *block, XsLilType result_type, const char *operation,
                                   size_t operation_length, XsLilValueId expected_result, bool *matched,
                                   XsLilError *error)
{
    static const char prefix[] = "const.u16 0x";
    *matched = operation_length >= sizeof(prefix) - 1U && memcmp(operation, prefix, sizeof(prefix) - 1U) == 0;
    if(!*matched)
        return XS_LIL_OK;
    if(result_type.kind != XS_LIL_TYPE_U16 || operation_length != sizeof(prefix) - 1U + 4U)
        return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL const.u16 record is invalid");
    XsUInt128 bits = {0};
    if(!xs_uint128_parse_hex(operation + sizeof(prefix) - 1U, 4, &bits))
        return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL const.u16 immediate is invalid");
    return append_bits(block, result_type, bits, expected_result, error);
}

XsLilStatus xs_lil_parse_const_integer(XsLilBlock *block, XsLilType result_type, const char *operation,
                                       size_t operation_length, XsLilValueId expected_result, bool *matched,
                                       XsLilError *error)
{
    *matched = false;
    for(size_t index = 0; index < sizeof(integer_records) / sizeof(integer_records[0]); ++index)
    {
        const IntegerRecord *record = &integer_records[index];
        if(operation_length < record->prefix_length || memcmp(operation, record->prefix, record->prefix_length) != 0)
            continue;
        *matched = true;
        if(result_type.kind != record->type || operation_length != record->prefix_length + record->digits)
            return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL integer constant record is invalid");
        XsUInt128 bits = {0};
        if(!xs_uint128_parse_hex(operation + record->prefix_length, record->digits, &bits))
            return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL integer constant bit pattern is invalid");
        return append_bits(block, result_type, bits, expected_result, error);
    }
    return XS_LIL_OK;
}
