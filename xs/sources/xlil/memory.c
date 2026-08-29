/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

#include "model_internal.h"

#include <stdlib.h>

XsLilStatus xs_lil_function_add_slot(XsLilFunction *function, XsLilType type, XsLilSlotId *slot, XsLilError *error)
{
    xs_lil_clear_error(error);
    if(slot != nullptr)
        *slot = UINT32_MAX;
    if(function == nullptr || !function->is_definition || type.kind == XS_LIL_TYPE_VOID)
        return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT,
                                "valid XLIL function definition and non-void slot type are required");
    if(function->slot_count == function->slot_capacity)
    {
        size_t capacity = function->slot_capacity == 0 ? 4 : function->slot_capacity * 2;
        XsLilSlot *slots = realloc(function->slots, capacity * sizeof(*slots));
        if(slots == nullptr)
            return xs_lil_set_error(error, XS_LIL_ALLOCATION_FAILED, "out of memory while adding XLIL stack slot");
        function->slots = slots;
        function->slot_capacity = capacity;
    }
    XsLilSlotId id = (XsLilSlotId)function->slot_count;
    function->slots[function->slot_count++] = (XsLilSlot){.type = type};
    if(slot != nullptr)
        *slot = id;
    return XS_LIL_OK;
}

size_t xs_lil_function_slot_count(const XsLilFunction *function)
{
    return function == nullptr ? 0 : function->slot_count;
}

XsLilType xs_lil_function_slot_type(const XsLilFunction *function, XsLilSlotId slot)
{
    if(function == nullptr || (size_t)slot >= function->slot_count)
        return (XsLilType){.kind = XS_LIL_TYPE_VOID};
    return function->slots[slot].type;
}

XsLilStatus xs_lil_block_add_load(XsLilBlock *block, XsLilSlotId slot, XsLilValueId *result, XsLilError *error)
{
    xs_lil_clear_error(error);
    if(result != nullptr)
        *result = UINT32_MAX;
    if(block == nullptr || block->owner == nullptr || (size_t)slot >= block->owner->slot_count)
        return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL load references an unknown stack slot");
    XsLilInstruction instruction = {.kind = XS_LIL_INSTRUCTION_LOAD, .slot = slot};
    XsLilStatus status = xs_lil_add_value(block->owner, block->owner->slots[slot].type, &instruction.result, error);
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

XsLilStatus xs_lil_block_add_store(XsLilBlock *block, XsLilSlotId slot, XsLilValueId value, XsLilError *error)
{
    xs_lil_clear_error(error);
    if(block == nullptr || block->owner == nullptr || (size_t)slot >= block->owner->slot_count ||
       (size_t)value >= block->owner->value_count)
        return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL store references an unknown value or stack slot");
    if(!xs_lil_type_equal(block->owner->slots[slot].type, block->owner->values[value].type))
        return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL store value type does not match stack slot type");
    return xs_lil_append_instruction(
        block, (XsLilInstruction){.kind = XS_LIL_INSTRUCTION_STORE, .left = value, .slot = slot}, error);
}

XsLilSlotId xs_lil_block_instruction_slot(const XsLilBlock *block, size_t index)
{
    if(block == nullptr || index >= block->instruction_count)
        return UINT32_MAX;
    XsLilInstructionKind kind = block->instructions[index].kind;
    if(kind != XS_LIL_INSTRUCTION_LOAD && kind != XS_LIL_INSTRUCTION_STORE)
        return UINT32_MAX;
    return block->instructions[index].slot;
}
