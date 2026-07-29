/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "model_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

XsMirStatus xs_mir_function_append_block(XsMirFunction *function, const char *label, XsMirBlock **block,
                                         XsMirError *error)
{
  xs_mir_clear_error(error);
  if(block != nullptr)
    *block = nullptr;
  if(function == nullptr || !function->is_definition || label == nullptr || label[0] == '\0')
    return xs_mir_set_error(error, XS_MIR_INVALID_ARGUMENT,
                            "valid MIR function definition and block label are required");
  if(function->block_count == function->block_capacity)
  {
    size_t capacity = function->block_capacity == 0 ? 4 : function->block_capacity * 2;
    XsMirBlock **blocks = realloc(function->blocks, capacity * sizeof(*blocks));
    if(blocks == nullptr)
      return xs_mir_set_error(error, XS_MIR_ALLOCATION_FAILED, "out of memory while adding MIR block");
    function->blocks = blocks;
    function->block_capacity = capacity;
  }
  XsMirBlock *created = calloc(1, sizeof(*created));
  if(created == nullptr)
    return xs_mir_set_error(error, XS_MIR_ALLOCATION_FAILED, "out of memory while adding MIR block");
  *created = (XsMirBlock){
      .label = xs_mir_copy_text(label),
      .owner = function,
      .id = (XsMirBlockId)function->block_count,
  };
  if(created->label == nullptr)
  {
    free(created);
    return xs_mir_set_error(error, XS_MIR_ALLOCATION_FAILED, "out of memory while naming MIR block");
  }
  function->blocks[function->block_count] = created;
  if(block != nullptr)
    *block = created;
  ++function->block_count;
  return XS_MIR_OK;
}

size_t xs_mir_function_block_count(const XsMirFunction *function)
{
  return function == nullptr ? 0 : function->block_count;
}

const XsMirBlock *xs_mir_function_block_at(const XsMirFunction *function, size_t index)
{
  if(function == nullptr || index >= function->block_count)
    return nullptr;
  return function->blocks[index];
}

XsMirBlockId xs_mir_block_id(const XsMirBlock *block)
{
  return block == nullptr ? 0 : block->id;
}

const char *xs_mir_block_label(const XsMirBlock *block)
{
  return block == nullptr ? nullptr : block->label;
}

XsMirTerminatorKind xs_mir_block_terminator_kind(const XsMirBlock *block)
{
  return block == nullptr ? XS_MIR_TERMINATOR_NONE : block->terminator.kind;
}

static XsMirStatus set_terminator(XsMirBlock *block, XsMirTerminator terminator, XsMirError *error)
{
  xs_mir_clear_error(error);
  if(block == nullptr)
    return xs_mir_set_error(error, XS_MIR_INVALID_ARGUMENT, "valid MIR block is required");
  if(block->terminator.kind != XS_MIR_TERMINATOR_NONE)
    return xs_mir_set_error(error, XS_MIR_INVALID_ARGUMENT, "MIR block already has a terminator");
  block->terminator = terminator;
  return XS_MIR_OK;
}

XsMirStatus xs_mir_block_set_return(XsMirBlock *block, XsMirError *error)
{
  return set_terminator(block, (XsMirTerminator){.kind = XS_MIR_TERMINATOR_RETURN}, error);
}

XsMirStatus xs_mir_block_set_return_value(XsMirBlock *block, XsMirValueId value, XsMirError *error)
{
  return set_terminator(block, (XsMirTerminator){.kind = XS_MIR_TERMINATOR_RETURN, .has_value = true, .value = value},
                        error);
}

XsMirStatus xs_mir_block_set_goto(XsMirBlock *block, const XsMirBlock *target, XsMirError *error)
{
  if(target == nullptr)
  {
    xs_mir_clear_error(error);
    return xs_mir_set_error(error, XS_MIR_INVALID_ARGUMENT, "valid MIR goto target is required");
  }
  return set_terminator(block, (XsMirTerminator){.kind = XS_MIR_TERMINATOR_GOTO, .target = target->id}, error);
}

XsMirStatus xs_mir_block_set_branch(XsMirBlock *block, XsMirValueId condition, const XsMirBlock *then_target,
                                    const XsMirBlock *else_target, XsMirError *error)
{
  if(then_target == nullptr || else_target == nullptr)
  {
    xs_mir_clear_error(error);
    return xs_mir_set_error(error, XS_MIR_INVALID_ARGUMENT, "valid MIR branch targets are required");
  }
  return set_terminator(block,
                        (XsMirTerminator){
                            .kind = XS_MIR_TERMINATOR_BRANCH,
                            .has_value = true,
                            .value = condition,
                            .target = then_target->id,
                            .else_target = else_target->id,
                        },
                        error);
}

XsMirStatus xs_mir_block_set_unreachable(XsMirBlock *block, XsMirError *error)
{
  return set_terminator(block, (XsMirTerminator){.kind = XS_MIR_TERMINATOR_UNREACHABLE}, error);
}

XsMirStatus xs_mir_block_set_panic(XsMirBlock *block, XsMirError *error)
{
  return set_terminator(block, (XsMirTerminator){.kind = XS_MIR_TERMINATOR_PANIC}, error);
}

static XsMirStatus append_instruction(XsMirBlock *block, XsMirInstruction instruction, XsMirError *error)
{
  xs_mir_clear_error(error);
  if(block == nullptr)
    return xs_mir_set_error(error, XS_MIR_INVALID_ARGUMENT, "valid MIR block is required");
  if(block->terminator.kind != XS_MIR_TERMINATOR_NONE)
    return xs_mir_set_error(error, XS_MIR_INVALID_ARGUMENT, "cannot add MIR instruction after terminator");
  if(block->instruction_count == block->instruction_capacity)
  {
    size_t capacity = block->instruction_capacity == 0 ? 8 : block->instruction_capacity * 2;
    XsMirInstruction *instructions = realloc(block->instructions, capacity * sizeof(*instructions));
    if(instructions == nullptr)
      return xs_mir_set_error(error, XS_MIR_ALLOCATION_FAILED, "out of memory while adding MIR instruction");
    block->instructions = instructions;
    block->instruction_capacity = capacity;
  }
  block->instructions[block->instruction_count++] = instruction;
  return XS_MIR_OK;
}

XsMirStatus xs_mir_block_add_const_i64(XsMirBlock *block, int64_t value, XsMirValueId *result, XsMirError *error)
{
  xs_mir_clear_error(error);
  if(result != nullptr)
    *result = 0;
  if(block == nullptr || block->owner == nullptr)
    return xs_mir_set_error(error, XS_MIR_INVALID_ARGUMENT, "valid MIR block is required");
  XsMirValueId value_id = 0;
  XsMirStatus status = xs_mir_function_add_value(block->owner, (XsMirType){.kind = XS_LIL_TYPE_I64}, &value_id, error);
  if(status != XS_MIR_OK)
    return status;
  status = append_instruction(block,
                              (XsMirInstruction){
                                  .kind = XS_MIR_INSTRUCTION_CONST_I64,
                                  .result = value_id,
                                  .immediate_i64 = value,
                              },
                              error);
  if(status != XS_MIR_OK)
    return status;
  if(result != nullptr)
    *result = value_id;
  return XS_MIR_OK;
}

XsMirStatus xs_mir_block_add_const_i32(XsMirBlock *block, int32_t value, XsMirValueId *result, XsMirError *error)
{
  xs_mir_clear_error(error);
  if(result != nullptr)
    *result = 0;
  if(block == nullptr || block->owner == nullptr)
    return xs_mir_set_error(error, XS_MIR_INVALID_ARGUMENT, "valid MIR block is required");
  XsMirValueId value_id = 0;
  XsMirStatus status = xs_mir_function_add_value(block->owner, (XsMirType){.kind = XS_LIL_TYPE_I32}, &value_id, error);
  if(status != XS_MIR_OK)
    return status;
  status = append_instruction(block,
                              (XsMirInstruction){
                                  .kind = XS_MIR_INSTRUCTION_CONST_I32,
                                  .result = value_id,
                                  .immediate_i64 = value,
                              },
                              error);
  if(status != XS_MIR_OK)
    return status;
  if(result != nullptr)
    *result = value_id;
  return XS_MIR_OK;
}

XsMirStatus xs_mir_block_add_const_bool(XsMirBlock *block, bool value, XsMirValueId *result, XsMirError *error)
{
  xs_mir_clear_error(error);
  if(result != nullptr)
    *result = 0;
  if(block == nullptr || block->owner == nullptr)
    return xs_mir_set_error(error, XS_MIR_INVALID_ARGUMENT, "valid MIR block is required");
  XsMirValueId value_id = 0;
  XsMirStatus status = xs_mir_function_add_value(block->owner, (XsMirType){.kind = XS_LIL_TYPE_BOOL}, &value_id, error);
  if(status != XS_MIR_OK)
    return status;
  status = append_instruction(block,
                              (XsMirInstruction){
                                  .kind = XS_MIR_INSTRUCTION_CONST_BOOL,
                                  .result = value_id,
                                  .immediate_i64 = value ? 1 : 0,
                              },
                              error);
  if(status != XS_MIR_OK)
    return status;
  if(result != nullptr)
    *result = value_id;
  return XS_MIR_OK;
}

static XsMirStatus add_i64_binary(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirInstructionKind kind,
                                  XsMirType result_type, XsMirValueId *result, XsMirError *error)
{
  xs_mir_clear_error(error);
  if(result != nullptr)
    *result = 0;
  if(block == nullptr || block->owner == nullptr)
    return xs_mir_set_error(error, XS_MIR_INVALID_ARGUMENT, "valid MIR block is required");
  XsMirValueId value_id = 0;
  XsMirStatus status = xs_mir_function_add_value(block->owner, result_type, &value_id, error);
  if(status != XS_MIR_OK)
    return status;
  status = append_instruction(block,
                              (XsMirInstruction){
                                  .kind = kind,
                                  .result = value_id,
                                  .operand_left = left,
                                  .operand_right = right,
                              },
                              error);
  if(status != XS_MIR_OK)
    return status;
  if(result != nullptr)
    *result = value_id;
  return XS_MIR_OK;
}

XsMirStatus xs_mir_block_add_i64(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirValueId *result,
                                 XsMirError *error)
{
  return add_i64_binary(block, left, right, XS_MIR_INSTRUCTION_ADD_I64, (XsMirType){.kind = XS_LIL_TYPE_I64}, result,
                        error);
}

XsMirStatus xs_mir_block_sub_i64(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirValueId *result,
                                 XsMirError *error)
{
  return add_i64_binary(block, left, right, XS_MIR_INSTRUCTION_SUB_I64, (XsMirType){.kind = XS_LIL_TYPE_I64}, result,
                        error);
}

XsMirStatus xs_mir_block_mul_i64(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirValueId *result,
                                 XsMirError *error)
{
  return add_i64_binary(block, left, right, XS_MIR_INSTRUCTION_MUL_I64, (XsMirType){.kind = XS_LIL_TYPE_I64}, result,
                        error);
}

XsMirStatus xs_mir_block_div_i64(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirValueId *result,
                                 XsMirError *error)
{
  return add_i64_binary(block, left, right, XS_MIR_INSTRUCTION_DIV_I64, (XsMirType){.kind = XS_LIL_TYPE_I64}, result,
                        error);
}

XsMirStatus xs_mir_block_rem_i64(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirValueId *result,
                                 XsMirError *error)
{
  return add_i64_binary(block, left, right, XS_MIR_INSTRUCTION_REM_I64, (XsMirType){.kind = XS_LIL_TYPE_I64}, result,
                        error);
}

XsMirStatus xs_mir_block_and_i64(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirValueId *result,
                                 XsMirError *error)
{
  return add_i64_binary(block, left, right, XS_MIR_INSTRUCTION_AND_I64, (XsMirType){.kind = XS_LIL_TYPE_I64}, result,
                        error);
}

XsMirStatus xs_mir_block_or_i64(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirValueId *result,
                                XsMirError *error)
{
  return add_i64_binary(block, left, right, XS_MIR_INSTRUCTION_OR_I64, (XsMirType){.kind = XS_LIL_TYPE_I64}, result,
                        error);
}

XsMirStatus xs_mir_block_xor_i64(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirValueId *result,
                                 XsMirError *error)
{
  return add_i64_binary(block, left, right, XS_MIR_INSTRUCTION_XOR_I64, (XsMirType){.kind = XS_LIL_TYPE_I64}, result,
                        error);
}

XsMirStatus xs_mir_block_shl_i64(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirValueId *result,
                                 XsMirError *error)
{
  return add_i64_binary(block, left, right, XS_MIR_INSTRUCTION_SHL_I64, (XsMirType){.kind = XS_LIL_TYPE_I64}, result,
                        error);
}

XsMirStatus xs_mir_block_shr_i64(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirValueId *result,
                                 XsMirError *error)
{
  return add_i64_binary(block, left, right, XS_MIR_INSTRUCTION_SHR_I64, (XsMirType){.kind = XS_LIL_TYPE_I64}, result,
                        error);
}

XsMirStatus xs_mir_block_eq_i64(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirValueId *result,
                                XsMirError *error)
{
  return add_i64_binary(block, left, right, XS_MIR_INSTRUCTION_EQ_I64, (XsMirType){.kind = XS_LIL_TYPE_BOOL}, result,
                        error);
}

XsMirStatus xs_mir_block_ne_i64(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirValueId *result,
                                XsMirError *error)
{
  return add_i64_binary(block, left, right, XS_MIR_INSTRUCTION_NE_I64, (XsMirType){.kind = XS_LIL_TYPE_BOOL}, result,
                        error);
}

XsMirStatus xs_mir_block_lt_i64(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirValueId *result,
                                XsMirError *error)
{
  return add_i64_binary(block, left, right, XS_MIR_INSTRUCTION_LT_I64, (XsMirType){.kind = XS_LIL_TYPE_BOOL}, result,
                        error);
}

XsMirStatus xs_mir_block_le_i64(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirValueId *result,
                                XsMirError *error)
{
  return add_i64_binary(block, left, right, XS_MIR_INSTRUCTION_LE_I64, (XsMirType){.kind = XS_LIL_TYPE_BOOL}, result,
                        error);
}

XsMirStatus xs_mir_block_gt_i64(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirValueId *result,
                                XsMirError *error)
{
  return add_i64_binary(block, left, right, XS_MIR_INSTRUCTION_GT_I64, (XsMirType){.kind = XS_LIL_TYPE_BOOL}, result,
                        error);
}

XsMirStatus xs_mir_block_ge_i64(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirValueId *result,
                                XsMirError *error)
{
  return add_i64_binary(block, left, right, XS_MIR_INSTRUCTION_GE_I64, (XsMirType){.kind = XS_LIL_TYPE_BOOL}, result,
                        error);
}

static XsMirStatus add_i32_binary(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirInstructionKind kind,
                                  XsMirType result_type, XsMirValueId *result, XsMirError *error)
{
  xs_mir_clear_error(error);
  if(result != nullptr)
    *result = 0;
  if(block == nullptr || block->owner == nullptr)
    return xs_mir_set_error(error, XS_MIR_INVALID_ARGUMENT, "valid MIR block is required");
  XsMirValueId value_id = 0;
  XsMirStatus status = xs_mir_function_add_value(block->owner, result_type, &value_id, error);
  if(status != XS_MIR_OK)
    return status;
  status = append_instruction(block,
                              (XsMirInstruction){
                                  .kind = kind,
                                  .result = value_id,
                                  .operand_left = left,
                                  .operand_right = right,
                              },
                              error);
  if(status != XS_MIR_OK)
    return status;
  if(result != nullptr)
    *result = value_id;
  return XS_MIR_OK;
}

XsMirStatus xs_mir_block_add_i32(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirValueId *result,
                                 XsMirError *error)
{
  return add_i32_binary(block, left, right, XS_MIR_INSTRUCTION_ADD_I32, (XsMirType){.kind = XS_LIL_TYPE_I32}, result,
                        error);
}

XsMirStatus xs_mir_block_sub_i32(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirValueId *result,
                                 XsMirError *error)
{
  return add_i32_binary(block, left, right, XS_MIR_INSTRUCTION_SUB_I32, (XsMirType){.kind = XS_LIL_TYPE_I32}, result,
                        error);
}

XsMirStatus xs_mir_block_mul_i32(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirValueId *result,
                                 XsMirError *error)
{
  return add_i32_binary(block, left, right, XS_MIR_INSTRUCTION_MUL_I32, (XsMirType){.kind = XS_LIL_TYPE_I32}, result,
                        error);
}

XsMirStatus xs_mir_block_div_i32(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirValueId *result,
                                 XsMirError *error)
{
  return add_i32_binary(block, left, right, XS_MIR_INSTRUCTION_DIV_I32, (XsMirType){.kind = XS_LIL_TYPE_I32}, result,
                        error);
}

XsMirStatus xs_mir_block_rem_i32(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirValueId *result,
                                 XsMirError *error)
{
  return add_i32_binary(block, left, right, XS_MIR_INSTRUCTION_REM_I32, (XsMirType){.kind = XS_LIL_TYPE_I32}, result,
                        error);
}

XsMirStatus xs_mir_block_and_i32(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirValueId *result,
                                 XsMirError *error)
{
  return add_i32_binary(block, left, right, XS_MIR_INSTRUCTION_AND_I32, (XsMirType){.kind = XS_LIL_TYPE_I32}, result,
                        error);
}

XsMirStatus xs_mir_block_or_i32(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirValueId *result,
                                XsMirError *error)
{
  return add_i32_binary(block, left, right, XS_MIR_INSTRUCTION_OR_I32, (XsMirType){.kind = XS_LIL_TYPE_I32}, result,
                        error);
}

XsMirStatus xs_mir_block_xor_i32(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirValueId *result,
                                 XsMirError *error)
{
  return add_i32_binary(block, left, right, XS_MIR_INSTRUCTION_XOR_I32, (XsMirType){.kind = XS_LIL_TYPE_I32}, result,
                        error);
}

XsMirStatus xs_mir_block_shl_i32(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirValueId *result,
                                 XsMirError *error)
{
  return add_i32_binary(block, left, right, XS_MIR_INSTRUCTION_SHL_I32, (XsMirType){.kind = XS_LIL_TYPE_I32}, result,
                        error);
}

XsMirStatus xs_mir_block_shr_i32(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirValueId *result,
                                 XsMirError *error)
{
  return add_i32_binary(block, left, right, XS_MIR_INSTRUCTION_SHR_I32, (XsMirType){.kind = XS_LIL_TYPE_I32}, result,
                        error);
}

XsMirStatus xs_mir_block_eq_i32(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirValueId *result,
                                XsMirError *error)
{
  return add_i32_binary(block, left, right, XS_MIR_INSTRUCTION_EQ_I32, (XsMirType){.kind = XS_LIL_TYPE_BOOL}, result,
                        error);
}

XsMirStatus xs_mir_block_ne_i32(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirValueId *result,
                                XsMirError *error)
{
  return add_i32_binary(block, left, right, XS_MIR_INSTRUCTION_NE_I32, (XsMirType){.kind = XS_LIL_TYPE_BOOL}, result,
                        error);
}

XsMirStatus xs_mir_block_lt_i32(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirValueId *result,
                                XsMirError *error)
{
  return add_i32_binary(block, left, right, XS_MIR_INSTRUCTION_LT_I32, (XsMirType){.kind = XS_LIL_TYPE_BOOL}, result,
                        error);
}

XsMirStatus xs_mir_block_le_i32(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirValueId *result,
                                XsMirError *error)
{
  return add_i32_binary(block, left, right, XS_MIR_INSTRUCTION_LE_I32, (XsMirType){.kind = XS_LIL_TYPE_BOOL}, result,
                        error);
}

XsMirStatus xs_mir_block_gt_i32(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirValueId *result,
                                XsMirError *error)
{
  return add_i32_binary(block, left, right, XS_MIR_INSTRUCTION_GT_I32, (XsMirType){.kind = XS_LIL_TYPE_BOOL}, result,
                        error);
}

XsMirStatus xs_mir_block_ge_i32(XsMirBlock *block, XsMirValueId left, XsMirValueId right, XsMirValueId *result,
                                XsMirError *error)
{
  return add_i32_binary(block, left, right, XS_MIR_INSTRUCTION_GE_I32, (XsMirType){.kind = XS_LIL_TYPE_BOOL}, result,
                        error);
}

XsMirStatus xs_mir_block_not_bool(XsMirBlock *block, XsMirValueId operand, XsMirValueId *result, XsMirError *error)
{
  if(result != nullptr)
    *result = 0;
  if(block == nullptr || block->owner == nullptr || (size_t)operand >= block->owner->value_count ||
     block->owner->values[operand].type.kind != XS_LIL_TYPE_BOOL)
    return xs_mir_set_error(error, XS_MIR_INVALID_ARGUMENT, "MIR not.bool operand must be bool");
  XsMirValueId value = 0;
  XsMirStatus status = xs_mir_function_add_value(block->owner, (XsMirType){.kind = XS_LIL_TYPE_BOOL}, &value, error);
  if(status != XS_MIR_OK)
    return status;
  status = append_instruction(block,
                              (XsMirInstruction){
                                  .kind = XS_MIR_INSTRUCTION_NOT_BOOL,
                                  .result = value,
                                  .operand_left = operand,
                              },
                              error);
  if(status != XS_MIR_OK)
    return status;
  if(result != nullptr)
    *result = value;
  return XS_MIR_OK;
}

XsMirStatus xs_mir_block_add_call(XsMirBlock *block, const char *callee, XsMirType return_type,
                                  const XsMirValueId *arguments, size_t argument_count, XsMirValueId *result,
                                  XsMirError *error)
{
  xs_mir_clear_error(error);
  if(result != nullptr)
    *result = 0;
  if(block == nullptr || block->owner == nullptr || callee == nullptr || callee[0] == '\0' ||
     (argument_count != 0 && arguments == nullptr))
    return xs_mir_set_error(error, XS_MIR_INVALID_ARGUMENT, "valid MIR call target and arguments are required");
  XsMirInstruction instruction = {
      .kind = XS_MIR_INSTRUCTION_CALL,
      .result = UINT32_MAX,
      .callee = xs_mir_copy_text(callee),
      .argument_count = argument_count,
  };
  if(instruction.callee == nullptr)
    return xs_mir_set_error(error, XS_MIR_ALLOCATION_FAILED, "out of memory while naming MIR callee");
  if(argument_count != 0)
  {
    instruction.arguments = malloc(argument_count * sizeof(*instruction.arguments));
    if(instruction.arguments == nullptr)
    {
      free(instruction.callee);
      return xs_mir_set_error(error, XS_MIR_ALLOCATION_FAILED, "out of memory while copying MIR call arguments");
    }
    memcpy(instruction.arguments, arguments, argument_count * sizeof(*instruction.arguments));
  }
  if(return_type.kind != XS_LIL_TYPE_VOID)
  {
    XsMirStatus status = xs_mir_function_add_value(block->owner, return_type, &instruction.result, error);
    if(status != XS_MIR_OK)
    {
      free(instruction.arguments);
      free(instruction.callee);
      return status;
    }
  }
  XsMirStatus status = append_instruction(block, instruction, error);
  if(status != XS_MIR_OK)
  {
    free(instruction.arguments);
    free(instruction.callee);
    return status;
  }
  if(result != nullptr && return_type.kind != XS_LIL_TYPE_VOID)
    *result = instruction.result;
  return XS_MIR_OK;
}

XsMirStatus xs_mir_block_add_load(XsMirBlock *block, const XsMirPlace *place, XsMirType result_type,
                                  XsMirValueId *result, XsMirError *error)
{
  xs_mir_clear_error(error);
  if(result != nullptr)
    *result = 0;
  if(block == nullptr || block->owner == nullptr || place == nullptr)
    return xs_mir_set_error(error, XS_MIR_INVALID_ARGUMENT, "valid MIR block and place are required");
  XsMirValueId value_id = 0;
  XsMirStatus status = xs_mir_function_add_value(block->owner, result_type, &value_id, error);
  if(status != XS_MIR_OK)
    return status;
  status = append_instruction(block,
                              (XsMirInstruction){
                                  .kind = XS_MIR_INSTRUCTION_LOAD,
                                  .result = value_id,
                                  .place = place->id,
                              },
                              error);
  if(status != XS_MIR_OK)
    return status;
  if(result != nullptr)
    *result = value_id;
  return XS_MIR_OK;
}

XsMirStatus xs_mir_block_add_store(XsMirBlock *block, const XsMirPlace *place, XsMirValueId value, XsMirError *error)
{
  if(block == nullptr || place == nullptr)
    return xs_mir_set_error(error, XS_MIR_INVALID_ARGUMENT, "valid MIR block and place are required");
  return append_instruction(block,
                            (XsMirInstruction){
                                .kind = XS_MIR_INSTRUCTION_STORE,
                                .operand_left = value,
                                .place = place->id,
                            },
                            error);
}

size_t xs_mir_block_instruction_count(const XsMirBlock *block)
{
  return block == nullptr ? 0 : block->instruction_count;
}

XsMirInstructionKind xs_mir_block_instruction_kind(const XsMirBlock *block, size_t index)
{
  if(block == nullptr || index >= block->instruction_count)
    return XS_MIR_INSTRUCTION_CONST_I64;
  return block->instructions[index].kind;
}

const char *xs_mir_block_instruction_callee(const XsMirBlock *block, size_t index)
{
  if(block == nullptr || index >= block->instruction_count ||
     block->instructions[index].kind != XS_MIR_INSTRUCTION_CALL)
    return nullptr;
  return block->instructions[index].callee;
}

size_t xs_mir_block_instruction_argument_count(const XsMirBlock *block, size_t index)
{
  if(block == nullptr || index >= block->instruction_count ||
     block->instructions[index].kind != XS_MIR_INSTRUCTION_CALL)
    return 0;
  return block->instructions[index].argument_count;
}

XsMirValueId xs_mir_block_instruction_argument(const XsMirBlock *block, size_t index, size_t argument_index)
{
  if(block == nullptr || index >= block->instruction_count ||
     block->instructions[index].kind != XS_MIR_INSTRUCTION_CALL ||
     argument_index >= block->instructions[index].argument_count)
    return 0;
  return block->instructions[index].arguments[argument_index];
}
