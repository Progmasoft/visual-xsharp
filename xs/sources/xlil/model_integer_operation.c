/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "model_internal.h"

static bool is_integer_type(XsLilTypeKind type)
{
    return type == XS_LIL_TYPE_U8 || type == XS_LIL_TYPE_I8 || type == XS_LIL_TYPE_U16 || type == XS_LIL_TYPE_I16 ||
           type == XS_LIL_TYPE_U32 || type == XS_LIL_TYPE_I32 || type == XS_LIL_TYPE_U64 || type == XS_LIL_TYPE_I64 ||
           type == XS_LIL_TYPE_U128 || type == XS_LIL_TYPE_I128;
}

bool xs_lil_integer_operation_is_comparison(XsLilIntegerBinaryOperation operation)
{
    return operation >= XS_LIL_INTEGER_EQUAL && operation <= XS_LIL_INTEGER_GREATER_EQUAL;
}

const char *xs_lil_integer_operation_name(XsLilIntegerBinaryOperation operation)
{
    static const char *names[] = {"add", "sub", "mul", "div", "rem", "and", "or", "xor",
                                  "shl", "shr", "eq",  "ne",  "lt",  "le",  "gt", "ge"};
    return (unsigned)operation < sizeof(names) / sizeof(names[0]) ? names[operation] : nullptr;
}

XsLilStatus xs_lil_block_binary_integer(XsLilBlock *block, XsLilIntegerBinaryOperation operation, XsLilType type,
                                        XsLilValueId left, XsLilValueId right, XsLilValueId *result, XsLilError *error)
{
    xs_lil_clear_error(error);
    if(result != nullptr)
        *result = 0;
    if(block == nullptr || block->owner == nullptr || !is_integer_type(type.kind) ||
       xs_lil_integer_operation_name(operation) == nullptr || (size_t)left >= block->owner->value_count ||
       (size_t)right >= block->owner->value_count || block->owner->values[left].type.kind != type.kind ||
       block->owner->values[right].type.kind != type.kind)
        return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT,
                                "XLIL integer operation requires matching integer operands");
    XsLilType result_type =
        xs_lil_integer_operation_is_comparison(operation) ? (XsLilType){.kind = XS_LIL_TYPE_BOOL} : type;
    XsLilInstruction instruction = {.kind = XS_LIL_INSTRUCTION_BINARY_INTEGER,
                                    .integer_operation = operation,
                                    .integer_type = type,
                                    .left = left,
                                    .right = right};
    XsLilStatus status = xs_lil_add_value(block->owner, result_type, &instruction.result, error);
    if(status != XS_LIL_OK)
        return status;
    status = xs_lil_append_instruction(block, instruction, error);
    if(status != XS_LIL_OK)
    {
        --block->owner->value_count;
        return status;
    }
    if(result != nullptr)
        *result = instruction.result;
    return XS_LIL_OK;
}

XsLilIntegerBinaryOperation xs_lil_block_instruction_integer_operation(const XsLilBlock *block, size_t index)
{
    if(block == nullptr || index >= block->instruction_count ||
       block->instructions[index].kind != XS_LIL_INSTRUCTION_BINARY_INTEGER)
        return XS_LIL_INTEGER_ADD;
    return block->instructions[index].integer_operation;
}

XsLilType xs_lil_block_instruction_integer_type(const XsLilBlock *block, size_t index)
{
    if(block == nullptr || index >= block->instruction_count ||
       block->instructions[index].kind != XS_LIL_INSTRUCTION_BINARY_INTEGER)
        return (XsLilType){.kind = XS_LIL_TYPE_VOID};
    return block->instructions[index].integer_type;
}
