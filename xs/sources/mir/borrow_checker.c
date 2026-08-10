/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "Visual/XSharp/mir/borrow_checker.hh"
#include "model_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static bool type_equal(XsMirType left, XsMirType right)
{
    return left.kind == right.kind;
}

static bool value_exists(const XsMirFunction *function, XsMirValueId value)
{
    return (size_t)value < function->value_count;
}

static XsMirStatus check_value_exists(const XsMirFunction *function, XsMirValueId value, const char *message,
                                      XsMirError *error)
{
    if(!value_exists(function, value))
        return xs_mir_set_error(error, XS_MIR_INVALID_ARGUMENT, message);
    return XS_MIR_OK;
}

static XsMirStatus find_place(const XsMirFunction *function, XsMirPlaceId place_id, const XsMirPlace **place,
                              XsMirError *error)
{
    if((size_t)place_id >= function->place_count || function->places[place_id] == nullptr)
        return xs_mir_set_error(error, XS_MIR_INVALID_ARGUMENT, "MIR instruction references an unknown place");
    *place = function->places[place_id];
    if((size_t)(*place)->root_local >= function->local_count)
        return xs_mir_set_error(error, XS_MIR_INVALID_ARGUMENT, "MIR place references an unknown local");
    return XS_MIR_OK;
}

static XsMirStatus check_i64_operand(const XsMirFunction *function, XsMirValueId value, const char *message,
                                     XsMirError *error)
{
    XsMirStatus status = check_value_exists(function, value, message, error);
    if(status != XS_MIR_OK)
        return status;
    if(!type_equal(function->values[value].type, (XsMirType){.kind = XS_LIL_TYPE_I64}))
        return xs_mir_set_error(error, XS_MIR_INVALID_ARGUMENT, "MIR i64 instruction operand is not i64");
    return XS_MIR_OK;
}

static XsMirStatus check_i32_operand(const XsMirFunction *function, XsMirValueId value, const char *message,
                                     XsMirError *error)
{
    XsMirStatus status = check_value_exists(function, value, message, error);
    if(status != XS_MIR_OK)
        return status;
    if(!type_equal(function->values[value].type, (XsMirType){.kind = XS_LIL_TYPE_I32}))
        return xs_mir_set_error(error, XS_MIR_INVALID_ARGUMENT, "MIR i32 instruction operand is not i32");
    return XS_MIR_OK;
}

static XsMirStatus check_i32_binary(const XsMirFunction *function, const XsMirInstruction *instruction,
                                    XsMirType result_type, XsMirError *error)
{
    XsMirStatus status = check_value_exists(function, instruction->result, "MIR i32 result is unknown", error);
    if(status != XS_MIR_OK)
        return status;
    if(!type_equal(function->values[instruction->result].type, result_type))
        return xs_mir_set_error(error, XS_MIR_INVALID_ARGUMENT, "MIR i32 instruction result has the wrong type");
    status = check_i32_operand(function, instruction->operand_left, "MIR i32 left operand is unknown", error);
    if(status != XS_MIR_OK)
        return status;
    return check_i32_operand(function, instruction->operand_right, "MIR i32 right operand is unknown", error);
}

static XsMirStatus check_i64_binary(const XsMirFunction *function, const XsMirInstruction *instruction,
                                    XsMirType result_type, XsMirError *error)
{
    XsMirStatus status = check_value_exists(function, instruction->result, "MIR i64 result is unknown", error);
    if(status != XS_MIR_OK)
        return status;
    if(!type_equal(function->values[instruction->result].type, result_type))
        return xs_mir_set_error(error, XS_MIR_INVALID_ARGUMENT, "MIR i64 instruction result has the wrong type");
    status = check_i64_operand(function, instruction->operand_left, "MIR i64 left operand is unknown", error);
    if(status != XS_MIR_OK)
        return status;
    return check_i64_operand(function, instruction->operand_right, "MIR i64 right operand is unknown", error);
}

static XsMirStatus check_instruction(const XsMirFunction *function, const XsMirInstruction *instruction,
                                     XsMirError *error)
{
    switch(instruction->kind)
    {
    case XS_MIR_INSTRUCTION_CONST_I64:
        return check_value_exists(function, instruction->result, "MIR const.i64 result is unknown", error);
    case XS_MIR_INSTRUCTION_CONST_I32:
        return check_value_exists(function, instruction->result, "MIR const.i32 result is unknown", error);
    case XS_MIR_INSTRUCTION_CONST_BOOL:
        return check_value_exists(function, instruction->result, "MIR const.bool result is unknown", error);
    case XS_MIR_INSTRUCTION_ADD_I64:
    case XS_MIR_INSTRUCTION_SUB_I64:
    case XS_MIR_INSTRUCTION_MUL_I64:
    case XS_MIR_INSTRUCTION_DIV_I64:
    case XS_MIR_INSTRUCTION_REM_I64:
    case XS_MIR_INSTRUCTION_AND_I64:
    case XS_MIR_INSTRUCTION_OR_I64:
    case XS_MIR_INSTRUCTION_XOR_I64:
    case XS_MIR_INSTRUCTION_SHL_I64:
    case XS_MIR_INSTRUCTION_SHR_I64:
        return check_i64_binary(function, instruction, (XsMirType){.kind = XS_LIL_TYPE_I64}, error);
    case XS_MIR_INSTRUCTION_EQ_I64:
    case XS_MIR_INSTRUCTION_NE_I64:
    case XS_MIR_INSTRUCTION_LT_I64:
    case XS_MIR_INSTRUCTION_LE_I64:
    case XS_MIR_INSTRUCTION_GT_I64:
    case XS_MIR_INSTRUCTION_GE_I64:
        return check_i64_binary(function, instruction, (XsMirType){.kind = XS_LIL_TYPE_BOOL}, error);
    case XS_MIR_INSTRUCTION_ADD_I32:
    case XS_MIR_INSTRUCTION_SUB_I32:
    case XS_MIR_INSTRUCTION_MUL_I32:
    case XS_MIR_INSTRUCTION_DIV_I32:
    case XS_MIR_INSTRUCTION_REM_I32:
    case XS_MIR_INSTRUCTION_AND_I32:
    case XS_MIR_INSTRUCTION_OR_I32:
    case XS_MIR_INSTRUCTION_XOR_I32:
    case XS_MIR_INSTRUCTION_SHL_I32:
    case XS_MIR_INSTRUCTION_SHR_I32:
        return check_i32_binary(function, instruction, (XsMirType){.kind = XS_LIL_TYPE_I32}, error);
    case XS_MIR_INSTRUCTION_EQ_I32:
    case XS_MIR_INSTRUCTION_NE_I32:
    case XS_MIR_INSTRUCTION_LT_I32:
    case XS_MIR_INSTRUCTION_LE_I32:
    case XS_MIR_INSTRUCTION_GT_I32:
    case XS_MIR_INSTRUCTION_GE_I32:
        return check_i32_binary(function, instruction, (XsMirType){.kind = XS_LIL_TYPE_BOOL}, error);
    case XS_MIR_INSTRUCTION_NOT_BOOL:
        if(!value_exists(function, instruction->operand_left) || !value_exists(function, instruction->result) ||
           !type_equal(function->values[instruction->operand_left].type, (XsMirType){.kind = XS_LIL_TYPE_BOOL}) ||
           !type_equal(function->values[instruction->result].type, (XsMirType){.kind = XS_LIL_TYPE_BOOL}))
            return xs_mir_set_error(error, XS_MIR_INVALID_ARGUMENT, "MIR not.bool instruction has invalid operands");
        return XS_MIR_OK;
    case XS_MIR_INSTRUCTION_CALL:
        if(instruction->callee == nullptr || instruction->callee[0] == '\0')
            return xs_mir_set_error(error, XS_MIR_INVALID_ARGUMENT, "MIR call target is missing");
        if(instruction->result != UINT32_MAX && !value_exists(function, instruction->result))
            return xs_mir_set_error(error, XS_MIR_INVALID_ARGUMENT, "MIR call result is unknown");
        for(size_t index = 0; index < instruction->argument_count; ++index)
        {
            XsMirStatus status =
                check_value_exists(function, instruction->arguments[index], "MIR call argument is unknown", error);
            if(status != XS_MIR_OK)
                return status;
        }
        return XS_MIR_OK;
    case XS_MIR_INSTRUCTION_LOAD:
    {
        const XsMirPlace *place = nullptr;
        XsMirStatus status = check_value_exists(function, instruction->result, "MIR load result is unknown", error);
        if(status != XS_MIR_OK)
            return status;
        status = find_place(function, instruction->place, &place, error);
        if(status != XS_MIR_OK)
            return status;
        if(!type_equal(function->values[instruction->result].type, function->locals[place->root_local].type))
            return xs_mir_set_error(error, XS_MIR_INVALID_ARGUMENT, "MIR load result type does not match place type");
        return XS_MIR_OK;
    }
    case XS_MIR_INSTRUCTION_STORE:
    {
        const XsMirPlace *place = nullptr;
        XsMirStatus status =
            check_value_exists(function, instruction->operand_left, "MIR store value is unknown", error);
        if(status != XS_MIR_OK)
            return status;
        status = find_place(function, instruction->place, &place, error);
        if(status != XS_MIR_OK)
            return status;
        if(!type_equal(function->values[instruction->operand_left].type, function->locals[place->root_local].type))
            return xs_mir_set_error(error, XS_MIR_INVALID_ARGUMENT, "MIR store value type does not match place type");
        return XS_MIR_OK;
    }
    }
    return xs_mir_set_error(error, XS_MIR_INVALID_ARGUMENT, "MIR instruction has an unknown kind");
}

static XsMirStatus check_return_type(const XsMirFunction *function, const XsMirBlock *block, XsMirError *error)
{
    if(block->terminator.kind != XS_MIR_TERMINATOR_RETURN)
        return XS_MIR_OK;
    if(!block->terminator.has_value)
        return type_equal(function->return_type, (XsMirType){.kind = XS_LIL_TYPE_VOID})
                   ? XS_MIR_OK
                   : xs_mir_set_error(error, XS_MIR_INVALID_ARGUMENT, "MIR return is missing a value");
    if((size_t)block->terminator.value >= function->value_count)
        return xs_mir_set_error(error, XS_MIR_INVALID_ARGUMENT, "MIR return references an unknown value");
    if(!type_equal(function->values[block->terminator.value].type, function->return_type))
        return xs_mir_set_error(error, XS_MIR_INVALID_ARGUMENT,
                                "MIR return value type does not match function return type");
    return XS_MIR_OK;
}

static XsMirStatus check_terminator(const XsMirFunction *function, const XsMirBlock *block, XsMirError *error)
{
    switch(block->terminator.kind)
    {
    case XS_MIR_TERMINATOR_NONE:
        return xs_mir_set_error(error, XS_MIR_INVALID_ARGUMENT, "MIR block is missing a terminator");
    case XS_MIR_TERMINATOR_RETURN:
        return check_return_type(function, block, error);
    case XS_MIR_TERMINATOR_GOTO:
        if((size_t)block->terminator.target >= function->block_count)
            return xs_mir_set_error(error, XS_MIR_INVALID_ARGUMENT, "MIR goto references an unknown block");
        return XS_MIR_OK;
    case XS_MIR_TERMINATOR_BRANCH:
    {
        XsMirStatus status =
            check_value_exists(function, block->terminator.value, "MIR branch condition is unknown", error);
        if(status != XS_MIR_OK)
            return status;
        if(!type_equal(function->values[block->terminator.value].type, (XsMirType){.kind = XS_LIL_TYPE_BOOL}))
            return xs_mir_set_error(error, XS_MIR_INVALID_ARGUMENT, "MIR branch condition is not bool");
        if((size_t)block->terminator.target >= function->block_count ||
           (size_t)block->terminator.else_target >= function->block_count)
            return xs_mir_set_error(error, XS_MIR_INVALID_ARGUMENT, "MIR branch references an unknown block");
        return XS_MIR_OK;
    }
    case XS_MIR_TERMINATOR_UNREACHABLE:
    case XS_MIR_TERMINATOR_PANIC:
        return XS_MIR_OK;
    }
    return xs_mir_set_error(error, XS_MIR_INVALID_ARGUMENT, "MIR terminator has an unknown kind");
}

static XsMirStatus check_block(const XsMirFunction *function, const XsMirBlock *block, XsMirError *error)
{
    for(size_t i = 0; i < block->instruction_count; ++i)
    {
        XsMirStatus status = check_instruction(function, &block->instructions[i], error);
        if(status != XS_MIR_OK)
            return status;
    }
    return check_terminator(function, block, error);
}

static size_t successor_ids(const XsMirBlock *block, XsMirBlockId targets[2])
{
    if(block->terminator.kind == XS_MIR_TERMINATOR_GOTO)
    {
        targets[0] = block->terminator.target;
        return 1;
    }
    if(block->terminator.kind == XS_MIR_TERMINATOR_BRANCH)
    {
        targets[0] = block->terminator.target;
        targets[1] = block->terminator.else_target;
        return 2;
    }
    return 0;
}

static XsMirStatus check_local_initialization_flow(const XsMirFunction *function, XsMirError *error)
{
    if(function->local_count == 0 || function->block_count == 0)
        return XS_MIR_OK;
    if(function->local_count > SIZE_MAX / function->block_count)
        return xs_mir_set_error(error, XS_MIR_ALLOCATION_FAILED, "MIR initialization state is too large");
    size_t state_count = function->block_count * function->local_count;
    uint8_t *may_incoming = calloc(state_count, sizeof(*may_incoming));
    uint8_t *definite_incoming = calloc(state_count, sizeof(*definite_incoming));
    uint8_t *reachable = calloc(function->block_count, sizeof(*reachable));
    uint8_t *has_incoming = calloc(function->block_count, sizeof(*has_incoming));
    uint8_t *may_outgoing = calloc(function->local_count, sizeof(*may_outgoing));
    uint8_t *definite_outgoing = calloc(function->local_count, sizeof(*definite_outgoing));
    if(may_incoming == nullptr || definite_incoming == nullptr || reachable == nullptr || has_incoming == nullptr ||
       may_outgoing == nullptr || definite_outgoing == nullptr)
    {
        free(may_incoming);
        free(definite_incoming);
        free(reachable);
        free(has_incoming);
        free(may_outgoing);
        free(definite_outgoing);
        return xs_mir_set_error(error, XS_MIR_ALLOCATION_FAILED,
                                "out of memory while checking MIR initialization flow");
    }
    reachable[0] = true;
    has_incoming[0] = true;
    for(size_t local = 0; local < function->local_count; ++local)
    {
        if(function->locals[local].kind == XS_MIR_LOCAL_PARAMETER)
        {
            may_incoming[local] = true;
            definite_incoming[local] = true;
        }
    }
    bool changed = true;
    XsMirStatus result = XS_MIR_OK;
    while(changed && result == XS_MIR_OK)
    {
        changed = false;
        for(size_t index = 0; index < function->block_count && result == XS_MIR_OK; ++index)
        {
            if(!reachable[index])
                continue;
            const XsMirBlock *block = function->blocks[index];
            memcpy(may_outgoing, &may_incoming[index * function->local_count], function->local_count);
            memcpy(definite_outgoing, &definite_incoming[index * function->local_count], function->local_count);
            for(size_t instruction = 0; instruction < block->instruction_count; ++instruction)
            {
                const XsMirInstruction *current = &block->instructions[instruction];
                if(current->kind == XS_MIR_INSTRUCTION_LOAD)
                {
                    const XsMirPlace *place = function->places[current->place];
                    if(!definite_outgoing[place->root_local])
                    {
                        result =
                            xs_mir_set_error(error, XS_MIR_INVALID_ARGUMENT,
                                             "MIR load reads a local that is not initialized on every reachable path");
                        break;
                    }
                    continue;
                }
                if(current->kind != XS_MIR_INSTRUCTION_STORE)
                    continue;
                const XsMirPlace *place = function->places[current->place];
                if(!function->locals[place->root_local].is_mutable)
                {
                    if(may_outgoing[place->root_local])
                    {
                        result = xs_mir_set_error(error, XS_MIR_UNSUPPORTED,
                                                  "MIR store reassigns an initialized immutable local");
                        break;
                    }
                }
                may_outgoing[place->root_local] = true;
                definite_outgoing[place->root_local] = true;
            }
            XsMirBlockId targets[2] = {0};
            size_t target_count = successor_ids(block, targets);
            for(size_t target_index = 0; target_index < target_count; ++target_index)
            {
                size_t target = targets[target_index];
                if(!reachable[target])
                {
                    reachable[target] = true;
                    changed = true;
                }
                uint8_t *target_may = &may_incoming[target * function->local_count];
                uint8_t *target_definite = &definite_incoming[target * function->local_count];
                if(!has_incoming[target])
                {
                    memcpy(target_may, may_outgoing, function->local_count);
                    memcpy(target_definite, definite_outgoing, function->local_count);
                    has_incoming[target] = true;
                    changed = true;
                    continue;
                }
                for(size_t local = 0; local < function->local_count; ++local)
                {
                    uint8_t merged_may = target_may[local] | may_outgoing[local];
                    uint8_t merged_definite = target_definite[local] & definite_outgoing[local];
                    if(merged_may != target_may[local] || merged_definite != target_definite[local])
                    {
                        target_may[local] = merged_may;
                        target_definite[local] = merged_definite;
                        changed = true;
                    }
                }
            }
        }
    }
    free(may_incoming);
    free(definite_incoming);
    free(reachable);
    free(has_incoming);
    free(may_outgoing);
    free(definite_outgoing);
    return result;
}

static XsMirStatus check_function(const XsMirFunction *function, XsMirError *error)
{
    if(!function->is_definition)
        return XS_MIR_OK;
    for(size_t index = 0; index < function->block_count; ++index)
    {
        XsMirStatus status = check_block(function, function->blocks[index], error);
        if(status != XS_MIR_OK)
            return status;
    }
    return check_local_initialization_flow(function, error);
}

XsMirStatus xs_mir_borrow_check_module(const XsMirModule *module, XsMirError *error)
{
    xs_mir_clear_error(error);
    if(module == nullptr)
        return xs_mir_set_error(error, XS_MIR_INVALID_ARGUMENT, "valid MIR module is required");
    for(size_t i = 0; i < module->function_count; ++i)
    {
        XsMirStatus status = check_function(&module->functions[i], error);
        if(status != XS_MIR_OK)
            return status;
    }
    return XS_MIR_OK;
}
