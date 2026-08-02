/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "model_internal.h"

static XsLilInstructionKind integer_instruction_kind(XsLilTypeKind type)
{
    switch(type)
    {
    case XS_LIL_TYPE_U8:
        return XS_LIL_INSTRUCTION_CONST_U8;
    case XS_LIL_TYPE_I8:
        return XS_LIL_INSTRUCTION_CONST_I8;
    case XS_LIL_TYPE_U16:
        return XS_LIL_INSTRUCTION_CONST_U16;
    case XS_LIL_TYPE_I16:
        return XS_LIL_INSTRUCTION_CONST_I16;
    case XS_LIL_TYPE_U32:
        return XS_LIL_INSTRUCTION_CONST_U32;
    case XS_LIL_TYPE_U64:
        return XS_LIL_INSTRUCTION_CONST_U64;
    case XS_LIL_TYPE_U128:
        return XS_LIL_INSTRUCTION_CONST_U128;
    case XS_LIL_TYPE_I128:
        return XS_LIL_INSTRUCTION_CONST_I128;
    default:
        return XS_LIL_INSTRUCTION_CONST_I64;
    }
}

static unsigned integer_width(XsLilTypeKind type)
{
    switch(type)
    {
    case XS_LIL_TYPE_U8:
    case XS_LIL_TYPE_I8:
        return 8;
    case XS_LIL_TYPE_U16:
    case XS_LIL_TYPE_I16:
        return 16;
    case XS_LIL_TYPE_U32:
        return 32;
    case XS_LIL_TYPE_U64:
        return 64;
    case XS_LIL_TYPE_U128:
    case XS_LIL_TYPE_I128:
        return 128;
    default:
        return 0;
    }
}

XsLilStatus xs_lil_add_const_integer_bits(XsLilBlock *block, XsLilType type, XsUInt128 bits, XsLilValueId *result,
                                          XsLilError *error)
{
    xs_lil_clear_error(error);
    if(result != nullptr)
        *result = 0;
    if(block == nullptr || block->owner == nullptr)
        return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "valid XLIL block is required");
    unsigned width = integer_width(type.kind);
    if(width == 0 || (width < 64 && (bits.high != 0 || bits.low >> width != 0)) || (width == 64 && bits.high != 0))
        return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL integer constant does not fit its type");
    XsLilValueId value_id = 0;
    XsLilStatus status = xs_lil_add_value(block->owner, type, &value_id, error);
    if(status == XS_LIL_OK)
        status = xs_lil_append_instruction(block,
                                           (XsLilInstruction){.kind = integer_instruction_kind(type.kind),
                                                              .result = value_id,
                                                              .immediate_integer_bits = bits},
                                           error);
    if(status != XS_LIL_OK)
        return status;
    if(result != nullptr)
        *result = value_id;
    return XS_LIL_OK;
}

XsLilStatus xs_lil_block_add_const_u8(XsLilBlock *block, uint8_t value, XsLilValueId *result, XsLilError *error)
{
    return xs_lil_add_const_integer_bits(block, (XsLilType){.kind = XS_LIL_TYPE_U8}, xs_uint128_from_u64(value), result,
                                         error);
}

XsLilStatus xs_lil_block_add_const_i8(XsLilBlock *block, int8_t value, XsLilValueId *result, XsLilError *error)
{
    return xs_lil_add_const_integer_bits(block, (XsLilType){.kind = XS_LIL_TYPE_I8},
                                         xs_uint128_from_u64((uint8_t)value), result, error);
}

XsLilStatus xs_lil_block_add_const_u16(XsLilBlock *block, uint16_t value, XsLilValueId *result, XsLilError *error)
{
    return xs_lil_add_const_integer_bits(block, (XsLilType){.kind = XS_LIL_TYPE_U16}, xs_uint128_from_u64(value),
                                         result, error);
}

XsLilStatus xs_lil_block_add_const_i16(XsLilBlock *block, int16_t value, XsLilValueId *result, XsLilError *error)
{
    return xs_lil_add_const_integer_bits(block, (XsLilType){.kind = XS_LIL_TYPE_I16},
                                         xs_uint128_from_u64((uint16_t)value), result, error);
}

XsLilStatus xs_lil_block_add_const_u32(XsLilBlock *block, uint32_t value, XsLilValueId *result, XsLilError *error)
{
    return xs_lil_add_const_integer_bits(block, (XsLilType){.kind = XS_LIL_TYPE_U32}, xs_uint128_from_u64(value),
                                         result, error);
}

XsLilStatus xs_lil_block_add_const_u64(XsLilBlock *block, uint64_t value, XsLilValueId *result, XsLilError *error)
{
    return xs_lil_add_const_integer_bits(block, (XsLilType){.kind = XS_LIL_TYPE_U64}, xs_uint128_from_u64(value),
                                         result, error);
}

XsLilStatus xs_lil_block_add_const_u128(XsLilBlock *block, XsUInt128 value, XsLilValueId *result, XsLilError *error)
{
    return xs_lil_add_const_integer_bits(block, (XsLilType){.kind = XS_LIL_TYPE_U128}, value, result, error);
}

XsLilStatus xs_lil_block_add_const_i128(XsLilBlock *block, XsInt128 value, XsLilValueId *result, XsLilError *error)
{
    return xs_lil_add_const_integer_bits(block, (XsLilType){.kind = XS_LIL_TYPE_I128}, xs_int128_as_bits(value), result,
                                         error);
}

uint16_t xs_lil_block_instruction_u16(const XsLilBlock *block, size_t index)
{
    if(block == nullptr || index >= block->instruction_count ||
       block->instructions[index].kind != XS_LIL_INSTRUCTION_CONST_U16)
        return 0;
    return (uint16_t)block->instructions[index].immediate_integer_bits.low;
}

XsUInt128 xs_lil_block_instruction_integer_bits(const XsLilBlock *block, size_t index)
{
    if(block == nullptr || index >= block->instruction_count)
        return (XsUInt128){0};
    switch(block->instructions[index].kind)
    {
    case XS_LIL_INSTRUCTION_CONST_U8:
    case XS_LIL_INSTRUCTION_CONST_I8:
    case XS_LIL_INSTRUCTION_CONST_U16:
    case XS_LIL_INSTRUCTION_CONST_I16:
    case XS_LIL_INSTRUCTION_CONST_U32:
    case XS_LIL_INSTRUCTION_CONST_U64:
    case XS_LIL_INSTRUCTION_CONST_U128:
    case XS_LIL_INSTRUCTION_CONST_I128:
        return block->instructions[index].immediate_integer_bits;
    default:
        return (XsUInt128){0};
    }
}
