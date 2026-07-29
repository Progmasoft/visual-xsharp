/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "xs/mir/optimizer.h"
#include "model_internal.h"

#include <stdint.h>
#include <stdlib.h>

static bool find_const_i64(const XsMirFunction *function, XsMirValueId value, int64_t *result)
{
  for(size_t block_index = 0; block_index < function->block_count; ++block_index)
  {
    const XsMirBlock *block = function->blocks[block_index];
    for(size_t instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index)
    {
      const XsMirInstruction *instruction = &block->instructions[instruction_index];
      if(instruction->kind == XS_MIR_INSTRUCTION_CONST_I64 && instruction->result == value)
      {
        *result = instruction->immediate_i64;
        return true;
      }
    }
  }
  return false;
}

static bool find_const_i32(const XsMirFunction *function, XsMirValueId value, int32_t *result)
{
  for(size_t block_index = 0; block_index < function->block_count; ++block_index)
  {
    const XsMirBlock *block = function->blocks[block_index];
    for(size_t instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index)
    {
      const XsMirInstruction *instruction = &block->instructions[instruction_index];
      if(instruction->kind == XS_MIR_INSTRUCTION_CONST_I32 && instruction->result == value)
      {
        *result = (int32_t)instruction->immediate_i64;
        return true;
      }
    }
  }
  return false;
}

static bool find_const_bool(const XsMirFunction *function, XsMirValueId value, bool *result)
{
  for(size_t block_index = 0; block_index < function->block_count; ++block_index)
  {
    const XsMirBlock *block = function->blocks[block_index];
    for(size_t instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index)
    {
      const XsMirInstruction *instruction = &block->instructions[instruction_index];
      if(instruction->kind == XS_MIR_INSTRUCTION_CONST_BOOL && instruction->result == value)
      {
        *result = instruction->immediate_i64 != 0;
        return true;
      }
    }
  }
  return false;
}

static bool is_i32_fold_candidate(XsMirInstructionKind kind)
{
  return kind == XS_MIR_INSTRUCTION_ADD_I32 || kind == XS_MIR_INSTRUCTION_SUB_I32 ||
         kind == XS_MIR_INSTRUCTION_MUL_I32 || kind == XS_MIR_INSTRUCTION_DIV_I32 ||
         kind == XS_MIR_INSTRUCTION_REM_I32 || kind == XS_MIR_INSTRUCTION_AND_I32 ||
         kind == XS_MIR_INSTRUCTION_OR_I32 || kind == XS_MIR_INSTRUCTION_XOR_I32 ||
         kind == XS_MIR_INSTRUCTION_SHL_I32 || kind == XS_MIR_INSTRUCTION_SHR_I32 ||
         kind == XS_MIR_INSTRUCTION_EQ_I32 || kind == XS_MIR_INSTRUCTION_NE_I32 || kind == XS_MIR_INSTRUCTION_LT_I32 ||
         kind == XS_MIR_INSTRUCTION_LE_I32 || kind == XS_MIR_INSTRUCTION_GT_I32 || kind == XS_MIR_INSTRUCTION_GE_I32;
}

static bool is_i32_bool_result(XsMirInstructionKind kind)
{
  return kind == XS_MIR_INSTRUCTION_EQ_I32 || kind == XS_MIR_INSTRUCTION_NE_I32 || kind == XS_MIR_INSTRUCTION_LT_I32 ||
         kind == XS_MIR_INSTRUCTION_LE_I32 || kind == XS_MIR_INSTRUCTION_GT_I32 || kind == XS_MIR_INSTRUCTION_GE_I32;
}

static bool fold_i32_result(XsMirInstructionKind kind, int32_t left, int32_t right, int64_t *result)
{
  if((kind == XS_MIR_INSTRUCTION_DIV_I32 || kind == XS_MIR_INSTRUCTION_REM_I32) &&
     (right == 0 || (left == INT32_MIN && right == -1)))
    return false;
  if((kind == XS_MIR_INSTRUCTION_SHL_I32 || kind == XS_MIR_INSTRUCTION_SHR_I32) && (right < 0 || right >= 32))
    return false;
  if(kind == XS_MIR_INSTRUCTION_SHL_I32 && (left < 0 || left > (INT32_MAX >> (unsigned)right)))
    return false;
  if(kind == XS_MIR_INSTRUCTION_SHR_I32 && left < 0)
    return false;
  int64_t value = kind == XS_MIR_INSTRUCTION_ADD_I32   ? (int64_t)left + right
                  : kind == XS_MIR_INSTRUCTION_SUB_I32 ? (int64_t)left - right
                  : kind == XS_MIR_INSTRUCTION_MUL_I32 ? (int64_t)left * right
                  : kind == XS_MIR_INSTRUCTION_DIV_I32 ? left / right
                  : kind == XS_MIR_INSTRUCTION_REM_I32 ? left % right
                  : kind == XS_MIR_INSTRUCTION_AND_I32 ? left & right
                  : kind == XS_MIR_INSTRUCTION_OR_I32  ? left | right
                  : kind == XS_MIR_INSTRUCTION_XOR_I32 ? left ^ right
                  : kind == XS_MIR_INSTRUCTION_SHL_I32 ? left << (unsigned)right
                  : kind == XS_MIR_INSTRUCTION_SHR_I32 ? left >> (unsigned)right
                  : kind == XS_MIR_INSTRUCTION_EQ_I32  ? left == right
                  : kind == XS_MIR_INSTRUCTION_NE_I32  ? left != right
                  : kind == XS_MIR_INSTRUCTION_LT_I32  ? left < right
                  : kind == XS_MIR_INSTRUCTION_LE_I32  ? left <= right
                  : kind == XS_MIR_INSTRUCTION_GT_I32  ? left > right
                  : kind == XS_MIR_INSTRUCTION_GE_I32  ? left >= right
                                                       : 0;
  if(!is_i32_bool_result(kind) && (value < INT32_MIN || value > INT32_MAX))
    return false;
  *result = value;
  return true;
}

static void fold_function_constants(XsMirFunction *function)
{
  if(!function->is_definition)
    return;
  for(size_t block_index = 0; block_index < function->block_count; ++block_index)
  {
    XsMirBlock *block = function->blocks[block_index];
    for(size_t instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index)
    {
      XsMirInstruction *instruction = &block->instructions[instruction_index];
      if(is_i32_fold_candidate(instruction->kind))
      {
        int32_t left = 0;
        int32_t right = 0;
        int64_t result = 0;
        XsMirInstructionKind kind = instruction->kind;
        if(!find_const_i32(function, instruction->operand_left, &left) ||
           !find_const_i32(function, instruction->operand_right, &right) ||
           !fold_i32_result(kind, left, right, &result))
          continue;
        *instruction = (XsMirInstruction){
            .kind = is_i32_bool_result(kind) ? XS_MIR_INSTRUCTION_CONST_BOOL : XS_MIR_INSTRUCTION_CONST_I32,
            .result = instruction->result,
            .immediate_i64 = result,
        };
        continue;
      }
      if(instruction->kind != XS_MIR_INSTRUCTION_ADD_I64 && instruction->kind != XS_MIR_INSTRUCTION_SUB_I64 &&
         instruction->kind != XS_MIR_INSTRUCTION_MUL_I64 && instruction->kind != XS_MIR_INSTRUCTION_DIV_I64 &&
         instruction->kind != XS_MIR_INSTRUCTION_REM_I64 && instruction->kind != XS_MIR_INSTRUCTION_AND_I64 &&
         instruction->kind != XS_MIR_INSTRUCTION_OR_I64 && instruction->kind != XS_MIR_INSTRUCTION_XOR_I64 &&
         instruction->kind != XS_MIR_INSTRUCTION_SHL_I64 && instruction->kind != XS_MIR_INSTRUCTION_SHR_I64 &&
         instruction->kind != XS_MIR_INSTRUCTION_EQ_I64 && instruction->kind != XS_MIR_INSTRUCTION_NE_I64 &&
         instruction->kind != XS_MIR_INSTRUCTION_LT_I64 && instruction->kind != XS_MIR_INSTRUCTION_LE_I64 &&
         instruction->kind != XS_MIR_INSTRUCTION_GT_I64 && instruction->kind != XS_MIR_INSTRUCTION_GE_I64)
        continue;
      int64_t left = 0;
      int64_t right = 0;
      if(!find_const_i64(function, instruction->operand_left, &left) ||
         !find_const_i64(function, instruction->operand_right, &right))
        continue;
      XsMirInstructionKind kind = instruction->kind;
      if((kind == XS_MIR_INSTRUCTION_DIV_I64 || kind == XS_MIR_INSTRUCTION_REM_I64) &&
         (right == 0 || (left == INT64_MIN && right == -1)))
        continue;
      if((kind == XS_MIR_INSTRUCTION_SHL_I64 || kind == XS_MIR_INSTRUCTION_SHR_I64) && (right < 0 || right >= 64))
        continue;
      if(kind == XS_MIR_INSTRUCTION_SHL_I64 && (left < 0 || left > (INT64_MAX >> (unsigned)right)))
        continue;
      if(kind == XS_MIR_INSTRUCTION_SHR_I64 && left < 0)
        continue;
      bool bool_result = kind == XS_MIR_INSTRUCTION_EQ_I64 || kind == XS_MIR_INSTRUCTION_NE_I64 ||
                         kind == XS_MIR_INSTRUCTION_LT_I64 || kind == XS_MIR_INSTRUCTION_LE_I64 ||
                         kind == XS_MIR_INSTRUCTION_GT_I64 || kind == XS_MIR_INSTRUCTION_GE_I64;
      *instruction = (XsMirInstruction){
          .kind = bool_result ? XS_MIR_INSTRUCTION_CONST_BOOL : XS_MIR_INSTRUCTION_CONST_I64,
          .result = instruction->result,
          .immediate_i64 = kind == XS_MIR_INSTRUCTION_ADD_I64   ? left + right
                           : kind == XS_MIR_INSTRUCTION_SUB_I64 ? left - right
                           : kind == XS_MIR_INSTRUCTION_MUL_I64 ? left * right
                           : kind == XS_MIR_INSTRUCTION_DIV_I64 ? left / right
                           : kind == XS_MIR_INSTRUCTION_REM_I64 ? left % right
                           : kind == XS_MIR_INSTRUCTION_AND_I64 ? left & right
                           : kind == XS_MIR_INSTRUCTION_OR_I64  ? left | right
                           : kind == XS_MIR_INSTRUCTION_XOR_I64 ? left ^ right
                           : kind == XS_MIR_INSTRUCTION_SHL_I64 ? left << (unsigned)right
                           : kind == XS_MIR_INSTRUCTION_SHR_I64 ? left >> (unsigned)right
                           : kind == XS_MIR_INSTRUCTION_EQ_I64  ? left == right
                           : kind == XS_MIR_INSTRUCTION_NE_I64  ? left != right
                           : kind == XS_MIR_INSTRUCTION_LT_I64  ? left < right
                           : kind == XS_MIR_INSTRUCTION_LE_I64  ? left <= right
                           : kind == XS_MIR_INSTRUCTION_GT_I64  ? left > right
                           : kind == XS_MIR_INSTRUCTION_GE_I64  ? left >= right
                                                                : 0,
      };
    }
    for(size_t instruction = 0; instruction < block->instruction_count; ++instruction)
    {
      XsMirInstruction *current = &block->instructions[instruction];
      if(current->kind != XS_MIR_INSTRUCTION_NOT_BOOL)
        continue;
      bool value = false;
      if(find_const_bool(function, current->operand_left, &value))
      {
        current->kind = XS_MIR_INSTRUCTION_CONST_BOOL;
        current->immediate_i64 = !value;
      }
    }
    if(block->terminator.kind == XS_MIR_TERMINATOR_BRANCH)
    {
      bool condition = false;
      if(find_const_bool(function, block->terminator.value, &condition))
      {
        block->terminator = (XsMirTerminator){
            .kind = XS_MIR_TERMINATOR_GOTO,
            .target = condition ? block->terminator.target : block->terminator.else_target,
        };
      }
    }
  }
}

static void mark_reachable(const XsMirFunction *function, size_t index, bool *reachable)
{
  if(index >= function->block_count || reachable[index])
    return;
  reachable[index] = true;
  const XsMirBlock *block = function->blocks[index];
  if(block->terminator.kind == XS_MIR_TERMINATOR_GOTO)
  {
    mark_reachable(function, block->terminator.target, reachable);
  }
  else if(block->terminator.kind == XS_MIR_TERMINATOR_BRANCH)
  {
    mark_reachable(function, block->terminator.target, reachable);
    mark_reachable(function, block->terminator.else_target, reachable);
  }
}

static void remap_goto_targets(XsMirFunction *function, const XsMirBlockId *remap)
{
  for(size_t i = 0; i < function->block_count; ++i)
  {
    XsMirBlock *block = function->blocks[i];
    block->id = (XsMirBlockId)i;
    if(block->terminator.kind == XS_MIR_TERMINATOR_GOTO)
    {
      block->terminator.target = remap[block->terminator.target];
    }
    else if(block->terminator.kind == XS_MIR_TERMINATOR_BRANCH)
    {
      block->terminator.target = remap[block->terminator.target];
      block->terminator.else_target = remap[block->terminator.else_target];
    }
  }
}

static XsMirStatus optimize_function_cfg(XsMirFunction *function, XsMirError *error)
{
  if(!function->is_definition || function->block_count == 0)
    return XS_MIR_OK;
  bool *reachable = calloc(function->block_count, sizeof(*reachable));
  XsMirBlockId *remap = calloc(function->block_count, sizeof(*remap));
  if(reachable == nullptr || remap == nullptr)
  {
    free(reachable);
    free(remap);
    return xs_mir_set_error(error, XS_MIR_ALLOCATION_FAILED, "out of memory while optimizing MIR CFG");
  }
  mark_reachable(function, 0, reachable);
  size_t write = 0;
  for(size_t read = 0; read < function->block_count; ++read)
  {
    if(reachable[read])
    {
      remap[read] = (XsMirBlockId)write;
      function->blocks[write++] = function->blocks[read];
      continue;
    }
    xs_mir_block_free(function->blocks[read]);
    free(function->blocks[read]);
  }
  function->block_count = write;
  remap_goto_targets(function, remap);
  free(reachable);
  free(remap);
  return XS_MIR_OK;
}

XsMirStatus xs_mir_optimize_module_cfg(XsMirModule *module, XsMirError *error)
{
  xs_mir_clear_error(error);
  if(module == nullptr)
    return xs_mir_set_error(error, XS_MIR_INVALID_ARGUMENT, "valid MIR module is required");
  for(size_t i = 0; i < module->function_count; ++i)
  {
    XsMirStatus status = optimize_function_cfg(&module->functions[i], error);
    if(status != XS_MIR_OK)
      return status;
  }
  return XS_MIR_OK;
}

XsMirStatus xs_mir_optimize_module_constants(XsMirModule *module, XsMirError *error)
{
  xs_mir_clear_error(error);
  if(module == nullptr)
    return xs_mir_set_error(error, XS_MIR_INVALID_ARGUMENT, "valid MIR module is required");
  for(size_t i = 0; i < module->function_count; ++i)
    fold_function_constants(&module->functions[i]);
  return XS_MIR_OK;
}
