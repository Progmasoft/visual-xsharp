/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "Visual/XSharp/mir/xlil_lowering.hh"
#include "model_internal.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static XsMirStatus set_error(XsMirError *error, XsMirStatus status, const char *message)
{
    if(error != nullptr)
    {
        error->status = status;
        snprintf(error->message, sizeof(error->message), "%s",
                 message == nullptr ? "MIR to XLIL lowering error" : message);
    }
    return status;
}

XsMirStatus xs_lil_module_add_mir_function_declarations(XsLilModule *module, const XsMirModule *mir, XsMirError *error)
{
    if(module == nullptr || mir == nullptr)
        return set_error(error, XS_MIR_INVALID_ARGUMENT, "valid XLIL and MIR modules are required");
    for(size_t i = 0; i < xs_mir_module_function_count(mir); ++i)
    {
        const XsMirFunction *function = xs_mir_module_function_at(mir, i);
        XsLilError lil_error = {0};
        XsLilStatus status = xs_lil_module_add_function(
            module, xs_mir_function_name(function), xs_mir_function_return_type(function),
            xs_mir_function_parameters(function), xs_mir_function_parameter_count(function), &lil_error);
        if(status == XS_LIL_ALLOCATION_FAILED)
            return set_error(error, XS_MIR_ALLOCATION_FAILED, lil_error.message);
        if(status != XS_LIL_OK)
            return set_error(error, XS_MIR_INVALID_ARGUMENT, lil_error.message);
    }
    return XS_MIR_OK;
}

static XsMirStatus map_lil_status(XsLilStatus status, const XsLilError *lil_error, XsMirError *error)
{
    if(status == XS_LIL_OK)
        return XS_MIR_OK;
    if(status == XS_LIL_ALLOCATION_FAILED)
        return set_error(error, XS_MIR_ALLOCATION_FAILED, lil_error->message);
    return set_error(error, XS_MIR_INVALID_ARGUMENT, lil_error->message);
}

static XsMirStatus lower_instruction(const XsMirFunction *function, const XsMirInstruction *instruction,
                                     XsLilBlock *block, XsLilValueId *values, const XsLilSlotId *slots,
                                     XsMirError *error)
{
    switch(instruction->kind)
    {
    case XS_MIR_INSTRUCTION_CONST_I64:
    {
        XsLilError lil_error = {0};
        XsLilStatus status =
            xs_lil_block_add_const_i64(block, instruction->immediate_i64, &values[instruction->result], &lil_error);
        return map_lil_status(status, &lil_error, error);
    }
    case XS_MIR_INSTRUCTION_CONST_I32:
    {
        XsLilError lil_error = {0};
        XsLilStatus status = xs_lil_block_add_const_i32(block, (int32_t)instruction->immediate_i64,
                                                        &values[instruction->result], &lil_error);
        return map_lil_status(status, &lil_error, error);
    }
    case XS_MIR_INSTRUCTION_CONST_BOOL:
    {
        XsLilError lil_error = {0};
        XsLilStatus status = xs_lil_block_add_const_bool(block, instruction->immediate_i64 != 0,
                                                         &values[instruction->result], &lil_error);
        return map_lil_status(status, &lil_error, error);
    }
    case XS_MIR_INSTRUCTION_NOT_BOOL:
    {
        XsLilError lil_error = {0};
        XsLilStatus status =
            xs_lil_block_not_bool(block, values[instruction->operand_left], &values[instruction->result], &lil_error);
        return map_lil_status(status, &lil_error, error);
    }
    case XS_MIR_INSTRUCTION_ADD_I64:
    {
        XsLilError lil_error = {0};
        XsLilStatus status =
            xs_lil_block_add_i64(block, values[instruction->operand_left], values[instruction->operand_right],
                                 &values[instruction->result], &lil_error);
        return map_lil_status(status, &lil_error, error);
    }
    case XS_MIR_INSTRUCTION_SUB_I64:
    {
        XsLilError lil_error = {0};
        XsLilStatus status =
            xs_lil_block_sub_i64(block, values[instruction->operand_left], values[instruction->operand_right],
                                 &values[instruction->result], &lil_error);
        return map_lil_status(status, &lil_error, error);
    }
    case XS_MIR_INSTRUCTION_MUL_I64:
    {
        XsLilError lil_error = {0};
        XsLilStatus status =
            xs_lil_block_mul_i64(block, values[instruction->operand_left], values[instruction->operand_right],
                                 &values[instruction->result], &lil_error);
        return map_lil_status(status, &lil_error, error);
    }
    case XS_MIR_INSTRUCTION_DIV_I64:
    {
        XsLilError lil_error = {0};
        XsLilStatus status =
            xs_lil_block_div_i64(block, values[instruction->operand_left], values[instruction->operand_right],
                                 &values[instruction->result], &lil_error);
        return map_lil_status(status, &lil_error, error);
    }
    case XS_MIR_INSTRUCTION_REM_I64:
    {
        XsLilError lil_error = {0};
        XsLilStatus status =
            xs_lil_block_rem_i64(block, values[instruction->operand_left], values[instruction->operand_right],
                                 &values[instruction->result], &lil_error);
        return map_lil_status(status, &lil_error, error);
    }
    case XS_MIR_INSTRUCTION_AND_I64:
    {
        XsLilError lil_error = {0};
        XsLilStatus status =
            xs_lil_block_and_i64(block, values[instruction->operand_left], values[instruction->operand_right],
                                 &values[instruction->result], &lil_error);
        return map_lil_status(status, &lil_error, error);
    }
    case XS_MIR_INSTRUCTION_OR_I64:
    {
        XsLilError lil_error = {0};
        XsLilStatus status =
            xs_lil_block_or_i64(block, values[instruction->operand_left], values[instruction->operand_right],
                                &values[instruction->result], &lil_error);
        return map_lil_status(status, &lil_error, error);
    }
    case XS_MIR_INSTRUCTION_XOR_I64:
    {
        XsLilError lil_error = {0};
        XsLilStatus status =
            xs_lil_block_xor_i64(block, values[instruction->operand_left], values[instruction->operand_right],
                                 &values[instruction->result], &lil_error);
        return map_lil_status(status, &lil_error, error);
    }
    case XS_MIR_INSTRUCTION_SHL_I64:
    {
        XsLilError lil_error = {0};
        XsLilStatus status =
            xs_lil_block_shl_i64(block, values[instruction->operand_left], values[instruction->operand_right],
                                 &values[instruction->result], &lil_error);
        return map_lil_status(status, &lil_error, error);
    }
    case XS_MIR_INSTRUCTION_SHR_I64:
    {
        XsLilError lil_error = {0};
        XsLilStatus status =
            xs_lil_block_shr_i64(block, values[instruction->operand_left], values[instruction->operand_right],
                                 &values[instruction->result], &lil_error);
        return map_lil_status(status, &lil_error, error);
    }
    case XS_MIR_INSTRUCTION_EQ_I64:
    {
        XsLilError lil_error = {0};
        XsLilStatus status =
            xs_lil_block_eq_i64(block, values[instruction->operand_left], values[instruction->operand_right],
                                &values[instruction->result], &lil_error);
        return map_lil_status(status, &lil_error, error);
    }
    case XS_MIR_INSTRUCTION_NE_I64:
    {
        XsLilError lil_error = {0};
        XsLilStatus status =
            xs_lil_block_ne_i64(block, values[instruction->operand_left], values[instruction->operand_right],
                                &values[instruction->result], &lil_error);
        return map_lil_status(status, &lil_error, error);
    }
    case XS_MIR_INSTRUCTION_LT_I64:
    {
        XsLilError lil_error = {0};
        XsLilStatus status =
            xs_lil_block_lt_i64(block, values[instruction->operand_left], values[instruction->operand_right],
                                &values[instruction->result], &lil_error);
        return map_lil_status(status, &lil_error, error);
    }
    case XS_MIR_INSTRUCTION_LE_I64:
    {
        XsLilError lil_error = {0};
        XsLilStatus status =
            xs_lil_block_le_i64(block, values[instruction->operand_left], values[instruction->operand_right],
                                &values[instruction->result], &lil_error);
        return map_lil_status(status, &lil_error, error);
    }
    case XS_MIR_INSTRUCTION_GT_I64:
    {
        XsLilError lil_error = {0};
        XsLilStatus status =
            xs_lil_block_gt_i64(block, values[instruction->operand_left], values[instruction->operand_right],
                                &values[instruction->result], &lil_error);
        return map_lil_status(status, &lil_error, error);
    }
    case XS_MIR_INSTRUCTION_GE_I64:
    {
        XsLilError lil_error = {0};
        XsLilStatus status =
            xs_lil_block_ge_i64(block, values[instruction->operand_left], values[instruction->operand_right],
                                &values[instruction->result], &lil_error);
        return map_lil_status(status, &lil_error, error);
    }
    case XS_MIR_INSTRUCTION_ADD_I32:
    {
        XsLilError lil_error = {0};
        XsLilStatus status =
            xs_lil_block_add_i32(block, values[instruction->operand_left], values[instruction->operand_right],
                                 &values[instruction->result], &lil_error);
        return map_lil_status(status, &lil_error, error);
    }
    case XS_MIR_INSTRUCTION_SUB_I32:
    {
        XsLilError lil_error = {0};
        XsLilStatus status =
            xs_lil_block_sub_i32(block, values[instruction->operand_left], values[instruction->operand_right],
                                 &values[instruction->result], &lil_error);
        return map_lil_status(status, &lil_error, error);
    }
    case XS_MIR_INSTRUCTION_MUL_I32:
    {
        XsLilError lil_error = {0};
        XsLilStatus status =
            xs_lil_block_mul_i32(block, values[instruction->operand_left], values[instruction->operand_right],
                                 &values[instruction->result], &lil_error);
        return map_lil_status(status, &lil_error, error);
    }
    case XS_MIR_INSTRUCTION_DIV_I32:
    {
        XsLilError lil_error = {0};
        XsLilStatus status =
            xs_lil_block_div_i32(block, values[instruction->operand_left], values[instruction->operand_right],
                                 &values[instruction->result], &lil_error);
        return map_lil_status(status, &lil_error, error);
    }
    case XS_MIR_INSTRUCTION_REM_I32:
    {
        XsLilError lil_error = {0};
        XsLilStatus status =
            xs_lil_block_rem_i32(block, values[instruction->operand_left], values[instruction->operand_right],
                                 &values[instruction->result], &lil_error);
        return map_lil_status(status, &lil_error, error);
    }
    case XS_MIR_INSTRUCTION_AND_I32:
    {
        XsLilError lil_error = {0};
        XsLilStatus status =
            xs_lil_block_and_i32(block, values[instruction->operand_left], values[instruction->operand_right],
                                 &values[instruction->result], &lil_error);
        return map_lil_status(status, &lil_error, error);
    }
    case XS_MIR_INSTRUCTION_OR_I32:
    {
        XsLilError lil_error = {0};
        XsLilStatus status =
            xs_lil_block_or_i32(block, values[instruction->operand_left], values[instruction->operand_right],
                                &values[instruction->result], &lil_error);
        return map_lil_status(status, &lil_error, error);
    }
    case XS_MIR_INSTRUCTION_XOR_I32:
    {
        XsLilError lil_error = {0};
        XsLilStatus status =
            xs_lil_block_xor_i32(block, values[instruction->operand_left], values[instruction->operand_right],
                                 &values[instruction->result], &lil_error);
        return map_lil_status(status, &lil_error, error);
    }
    case XS_MIR_INSTRUCTION_SHL_I32:
    {
        XsLilError lil_error = {0};
        XsLilStatus status =
            xs_lil_block_shl_i32(block, values[instruction->operand_left], values[instruction->operand_right],
                                 &values[instruction->result], &lil_error);
        return map_lil_status(status, &lil_error, error);
    }
    case XS_MIR_INSTRUCTION_SHR_I32:
    {
        XsLilError lil_error = {0};
        XsLilStatus status =
            xs_lil_block_shr_i32(block, values[instruction->operand_left], values[instruction->operand_right],
                                 &values[instruction->result], &lil_error);
        return map_lil_status(status, &lil_error, error);
    }
    case XS_MIR_INSTRUCTION_EQ_I32:
    {
        XsLilError lil_error = {0};
        XsLilStatus status =
            xs_lil_block_eq_i32(block, values[instruction->operand_left], values[instruction->operand_right],
                                &values[instruction->result], &lil_error);
        return map_lil_status(status, &lil_error, error);
    }
    case XS_MIR_INSTRUCTION_NE_I32:
    {
        XsLilError lil_error = {0};
        XsLilStatus status =
            xs_lil_block_ne_i32(block, values[instruction->operand_left], values[instruction->operand_right],
                                &values[instruction->result], &lil_error);
        return map_lil_status(status, &lil_error, error);
    }
    case XS_MIR_INSTRUCTION_LT_I32:
    {
        XsLilError lil_error = {0};
        XsLilStatus status =
            xs_lil_block_lt_i32(block, values[instruction->operand_left], values[instruction->operand_right],
                                &values[instruction->result], &lil_error);
        return map_lil_status(status, &lil_error, error);
    }
    case XS_MIR_INSTRUCTION_LE_I32:
    {
        XsLilError lil_error = {0};
        XsLilStatus status =
            xs_lil_block_le_i32(block, values[instruction->operand_left], values[instruction->operand_right],
                                &values[instruction->result], &lil_error);
        return map_lil_status(status, &lil_error, error);
    }
    case XS_MIR_INSTRUCTION_GT_I32:
    {
        XsLilError lil_error = {0};
        XsLilStatus status =
            xs_lil_block_gt_i32(block, values[instruction->operand_left], values[instruction->operand_right],
                                &values[instruction->result], &lil_error);
        return map_lil_status(status, &lil_error, error);
    }
    case XS_MIR_INSTRUCTION_GE_I32:
    {
        XsLilError lil_error = {0};
        XsLilStatus status =
            xs_lil_block_ge_i32(block, values[instruction->operand_left], values[instruction->operand_right],
                                &values[instruction->result], &lil_error);
        return map_lil_status(status, &lil_error, error);
    }
    case XS_MIR_INSTRUCTION_CALL:
    {
        XsLilValueId arguments[32] = {0};
        if(instruction->argument_count > sizeof(arguments) / sizeof(arguments[0]))
            return set_error(error, XS_MIR_UNSUPPORTED, "MIR to XLIL call lowering supports at most 32 arguments");
        for(size_t index = 0; index < instruction->argument_count; ++index)
            arguments[index] = values[instruction->arguments[index]];
        XsLilError lil_error = {0};
        XsLilType return_type = instruction->result == UINT32_MAX ? (XsLilType){.kind = XS_LIL_TYPE_VOID}
                                                                  : function->values[instruction->result].type;
        XsLilValueId result = 0;
        XsLilStatus status =
            xs_lil_block_add_call(block, instruction->callee, return_type, arguments, instruction->argument_count,
                                  instruction->result == UINT32_MAX ? nullptr : &result, &lil_error);
        if(status == XS_LIL_OK && instruction->result != UINT32_MAX)
            values[instruction->result] = result;
        return map_lil_status(status, &lil_error, error);
    }
    case XS_MIR_INSTRUCTION_LOAD:
    {
        if((size_t)instruction->place >= function->place_count || function->places[instruction->place] == nullptr)
            return set_error(error, XS_MIR_INVALID_ARGUMENT, "MIR load references an unknown place");
        const XsMirPlace *place = function->places[instruction->place];
        if((size_t)place->root_local >= function->local_count)
            return set_error(error, XS_MIR_INVALID_ARGUMENT, "MIR load place has an unknown root local");
        if(place->projection_count != 0)
            return set_error(error, XS_MIR_UNSUPPORTED, "MIR projected-place load lowering is not implemented yet");
        XsLilError lil_error = {0};
        XsLilStatus status =
            xs_lil_block_add_load(block, slots[place->root_local], &values[instruction->result], &lil_error);
        return map_lil_status(status, &lil_error, error);
    }
    case XS_MIR_INSTRUCTION_STORE:
    {
        if((size_t)instruction->place >= function->place_count || function->places[instruction->place] == nullptr)
            return set_error(error, XS_MIR_INVALID_ARGUMENT, "MIR store references an unknown place");
        const XsMirPlace *place = function->places[instruction->place];
        if((size_t)place->root_local >= function->local_count)
            return set_error(error, XS_MIR_INVALID_ARGUMENT, "MIR store place has an unknown root local");
        if(place->projection_count != 0)
            return set_error(error, XS_MIR_UNSUPPORTED, "MIR projected-place store lowering is not implemented yet");
        XsLilError lil_error = {0};
        XsLilStatus status =
            xs_lil_block_add_store(block, slots[place->root_local], values[instruction->operand_left], &lil_error);
        return map_lil_status(status, &lil_error, error);
    }
    }
    return set_error(error, XS_MIR_INVALID_ARGUMENT, "unknown MIR instruction kind while lowering to XLIL");
}

static XsMirStatus lower_terminator(const XsMirFunction *function, const XsMirBlock *mir_block, XsLilBlock *xlil_block,
                                    const XsLilValueId *values, XsMirError *error)
{
    (void)function;
    switch(mir_block->terminator.kind)
    {
    case XS_MIR_TERMINATOR_RETURN:
    {
        XsLilError lil_error = {0};
        XsLilStatus status =
            mir_block->terminator.has_value
                ? xs_lil_block_set_return_value(xlil_block, values[mir_block->terminator.value], &lil_error)
                : xs_lil_block_set_return(xlil_block, &lil_error);
        return map_lil_status(status, &lil_error, error);
    }
    case XS_MIR_TERMINATOR_NONE:
        return set_error(error, XS_MIR_INVALID_ARGUMENT, "MIR block is missing a terminator");
    case XS_MIR_TERMINATOR_GOTO:
    {
        XsLilError lil_error = {0};
        XsLilStatus status = xs_lil_block_set_branch(xlil_block, mir_block->terminator.target, &lil_error);
        return map_lil_status(status, &lil_error, error);
    }
    case XS_MIR_TERMINATOR_BRANCH:
    {
        XsLilError lil_error = {0};
        XsLilStatus status =
            xs_lil_block_set_branch_if(xlil_block, values[mir_block->terminator.value], mir_block->terminator.target,
                                       mir_block->terminator.else_target, &lil_error);
        return map_lil_status(status, &lil_error, error);
    }
    case XS_MIR_TERMINATOR_UNREACHABLE:
        return set_error(error, XS_MIR_UNSUPPORTED, "MIR unreachable lowering requires a distinct XLIL terminator");
    case XS_MIR_TERMINATOR_PANIC:
    {
        XsLilError lil_error = {0};
        XsLilStatus status = xs_lil_block_set_panic(xlil_block, &lil_error);
        return map_lil_status(status, &lil_error, error);
    }
    }
    return set_error(error, XS_MIR_INVALID_ARGUMENT, "unknown MIR terminator kind while lowering to XLIL");
}

static XsMirStatus lower_body(const XsMirFunction *function, XsLilFunction *xlil_function, XsMirError *error)
{
    XsLilValueId *values = nullptr;
    XsLilSlotId *slots = nullptr;
    XsLilBlock **blocks = nullptr;
    if(function->value_count != 0)
    {
        values = calloc(function->value_count, sizeof(*values));
        if(values == nullptr)
            return set_error(error, XS_MIR_ALLOCATION_FAILED, "out of memory while lowering MIR values to XLIL");
    }
    if(function->local_count != 0)
    {
        slots = calloc(function->local_count, sizeof(*slots));
        if(slots == nullptr)
        {
            free(values);
            return set_error(error, XS_MIR_ALLOCATION_FAILED, "out of memory while lowering MIR locals to XLIL slots");
        }
        for(size_t local = 0; local < function->local_count; ++local)
        {
            XsLilError lil_error = {0};
            XsLilStatus status =
                xs_lil_function_add_slot(xlil_function, function->locals[local].type, &slots[local], &lil_error);
            if(status != XS_LIL_OK)
            {
                free(slots);
                free(values);
                return map_lil_status(status, &lil_error, error);
            }
        }
    }
    if(function->block_count != 0)
    {
        blocks = calloc(function->block_count, sizeof(*blocks));
        if(blocks == nullptr)
        {
            free(slots);
            free(values);
            return set_error(error, XS_MIR_ALLOCATION_FAILED, "out of memory while lowering MIR blocks to XLIL");
        }
    }
    for(size_t index = 0; index < function->parameter_count && index < function->value_count; ++index)
        values[index] = (XsLilValueId)index;
    for(size_t block_index = 0; block_index < function->block_count; ++block_index)
    {
        const XsMirBlock *mir_block = function->blocks[block_index];
        XsLilError lil_error = {0};
        XsLilStatus lil_status =
            xs_lil_function_append_block(xlil_function, mir_block->label, &blocks[block_index], &lil_error);
        XsMirStatus status = map_lil_status(lil_status, &lil_error, error);
        if(status != XS_MIR_OK)
        {
            free(blocks);
            free(slots);
            free(values);
            return status;
        }
    }
    for(size_t block_index = 0; block_index < function->block_count; ++block_index)
    {
        const XsMirBlock *mir_block = function->blocks[block_index];
        XsLilBlock *xlil_block = blocks[block_index];
        for(size_t instruction_index = 0; instruction_index < mir_block->instruction_count; ++instruction_index)
        {
            XsMirStatus status = lower_instruction(function, &mir_block->instructions[instruction_index], xlil_block,
                                                   values, slots, error);
            if(status != XS_MIR_OK)
            {
                free(blocks);
                free(slots);
                free(values);
                return status;
            }
        }
        XsMirStatus status = lower_terminator(function, mir_block, xlil_block, values, error);
        if(status != XS_MIR_OK)
        {
            free(blocks);
            free(slots);
            free(values);
            return status;
        }
    }
    free(blocks);
    free(slots);
    free(values);
    return XS_MIR_OK;
}

XsMirStatus xs_lil_module_add_mir_function_bodies(XsLilModule *module, const XsMirModule *mir, XsMirError *error)
{
    if(module == nullptr || mir == nullptr)
        return set_error(error, XS_MIR_INVALID_ARGUMENT, "valid XLIL and MIR modules are required");
    for(size_t i = 0; i < xs_mir_module_function_count(mir); ++i)
    {
        const XsMirFunction *function = xs_mir_module_function_at(mir, i);
        XsLilError lil_error = {0};
        XsLilFunction *xlil_function = nullptr;
        XsLilStatus lil_status =
            function->is_definition
                ? xs_lil_module_add_function_definition(module, function->qualified_name, function->return_type,
                                                        function->parameters, function->parameter_count, &xlil_function,
                                                        &lil_error)
                : xs_lil_module_add_function(module, function->qualified_name, function->return_type,
                                             function->parameters, function->parameter_count, &lil_error);
        XsMirStatus status = map_lil_status(lil_status, &lil_error, error);
        if(status != XS_MIR_OK)
            return status;
        if(function->is_definition)
        {
            status = lower_body(function, xlil_function, error);
            if(status != XS_MIR_OK)
                return status;
        }
    }
    return XS_MIR_OK;
}
