/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "model_internal.h"

XsLilStatus xs_lil_block_compare_str(XsLilBlock *block, XsLilStrComparisonOperation operation, XsLilValueId left,
                                     XsLilValueId right, XsLilValueId *result, XsLilError *error)
{
    xs_lil_clear_error(error);
    if(result != nullptr)
        *result = UINT32_MAX;
    if(block == nullptr || block->owner == nullptr || (unsigned)operation > XS_LIL_STR_NE ||
       (size_t)left >= block->owner->value_count || (size_t)right >= block->owner->value_count ||
       block->owner->values[left].type.kind != XS_LIL_TYPE_STR ||
       block->owner->values[right].type.kind != XS_LIL_TYPE_STR)
        return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL Str comparison requires two str operands");

    XsLilInstruction instruction = {.kind = operation == XS_LIL_STR_EQ ? XS_LIL_INSTRUCTION_EQ_STR
                                                                       : XS_LIL_INSTRUCTION_NE_STR,
                                    .left = left,
                                    .right = right};
    XsLilStatus status =
        xs_lil_add_value(block->owner, (XsLilType){.kind = XS_LIL_TYPE_BOOL}, &instruction.result, error);
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
