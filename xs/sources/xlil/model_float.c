/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "model_internal.h"

static XsLilInstructionKind binary_kind(XsLilFloatBinaryOperation operation, XsLilTypeKind type)
{
  static const XsLilInstructionKind f32[] = {XS_LIL_INSTRUCTION_ADD_F32, XS_LIL_INSTRUCTION_SUB_F32,
                                             XS_LIL_INSTRUCTION_MUL_F32, XS_LIL_INSTRUCTION_DIV_F32,
                                             XS_LIL_INSTRUCTION_REM_F32};
  static const XsLilInstructionKind f64[] = {XS_LIL_INSTRUCTION_ADD_F64, XS_LIL_INSTRUCTION_SUB_F64,
                                             XS_LIL_INSTRUCTION_MUL_F64, XS_LIL_INSTRUCTION_DIV_F64,
                                             XS_LIL_INSTRUCTION_REM_F64};
  return type == XS_LIL_TYPE_F32 ? f32[operation] : f64[operation];
}

static XsLilInstructionKind comparison_kind(XsLilFloatComparisonOperation operation, XsLilTypeKind type)
{
  static const XsLilInstructionKind f32[] = {XS_LIL_INSTRUCTION_EQ_F32, XS_LIL_INSTRUCTION_NE_F32,
                                             XS_LIL_INSTRUCTION_LT_F32, XS_LIL_INSTRUCTION_LE_F32,
                                             XS_LIL_INSTRUCTION_GT_F32, XS_LIL_INSTRUCTION_GE_F32};
  static const XsLilInstructionKind f64[] = {XS_LIL_INSTRUCTION_EQ_F64, XS_LIL_INSTRUCTION_NE_F64,
                                             XS_LIL_INSTRUCTION_LT_F64, XS_LIL_INSTRUCTION_LE_F64,
                                             XS_LIL_INSTRUCTION_GT_F64, XS_LIL_INSTRUCTION_GE_F64};
  return type == XS_LIL_TYPE_F32 ? f32[operation] : f64[operation];
}

static XsLilStatus add_float(XsLilBlock *block, XsLilInstructionKind kind, XsLilType operand_type,
                             XsLilType result_type, XsLilValueId left, XsLilValueId right, XsLilValueId *result,
                             XsLilError *error)
{
  if(result != nullptr)
    *result = UINT32_MAX;
  if(block == nullptr || block->owner == nullptr ||
     (operand_type.kind != XS_LIL_TYPE_F32 && operand_type.kind != XS_LIL_TYPE_F64) ||
     (size_t)left >= block->owner->value_count || (size_t)right >= block->owner->value_count ||
     block->owner->values[left].type.kind != operand_type.kind ||
     block->owner->values[right].type.kind != operand_type.kind)
    return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT,
                            "XLIL floating instruction requires matching f32 or f64 operands");
  XsLilInstruction instruction = {.kind = kind, .left = left, .right = right};
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

XsLilStatus xs_lil_block_binary_float(XsLilBlock *block, XsLilFloatBinaryOperation operation, XsLilType type,
                                      XsLilValueId left, XsLilValueId right, XsLilValueId *result, XsLilError *error)
{
  xs_lil_clear_error(error);
  if((unsigned)operation > XS_LIL_FLOAT_REM || (type.kind != XS_LIL_TYPE_F32 && type.kind != XS_LIL_TYPE_F64))
    return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "valid XLIL floating binary operation is required");
  return add_float(block, binary_kind(operation, type.kind), type, type, left, right, result, error);
}

XsLilStatus xs_lil_block_compare_float(XsLilBlock *block, XsLilFloatComparisonOperation operation, XsLilType type,
                                       XsLilValueId left, XsLilValueId right, XsLilValueId *result, XsLilError *error)
{
  xs_lil_clear_error(error);
  if((unsigned)operation > XS_LIL_FLOAT_GE || (type.kind != XS_LIL_TYPE_F32 && type.kind != XS_LIL_TYPE_F64))
    return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "valid XLIL floating comparison is required");
  return add_float(block, comparison_kind(operation, type.kind), type, (XsLilType){.kind = XS_LIL_TYPE_BOOL}, left,
                   right, result, error);
}
