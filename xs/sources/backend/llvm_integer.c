/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "llvm_integer.h"

#include <stdio.h>

static bool integer_constant_type(XsLilInstructionKind kind, XsLilTypeKind *type)
{
  switch(kind)
  {
  case XS_LIL_INSTRUCTION_CONST_U8:
    *type = XS_LIL_TYPE_U8;
    return true;
  case XS_LIL_INSTRUCTION_CONST_I8:
    *type = XS_LIL_TYPE_I8;
    return true;
  case XS_LIL_INSTRUCTION_CONST_U16:
    *type = XS_LIL_TYPE_U16;
    return true;
  case XS_LIL_INSTRUCTION_CONST_I16:
    *type = XS_LIL_TYPE_I16;
    return true;
  case XS_LIL_INSTRUCTION_CONST_U32:
    *type = XS_LIL_TYPE_U32;
    return true;
  case XS_LIL_INSTRUCTION_CONST_U64:
    *type = XS_LIL_TYPE_U64;
    return true;
  case XS_LIL_INSTRUCTION_CONST_U128:
    *type = XS_LIL_TYPE_U128;
    return true;
  case XS_LIL_INSTRUCTION_CONST_I128:
    *type = XS_LIL_TYPE_I128;
    return true;
  default:
    return false;
  }
}

bool xs_llvm_is_integer_constant(XsLilInstructionKind kind)
{
  XsLilTypeKind unused = XS_LIL_TYPE_VOID;
  return integer_constant_type(kind, &unused);
}

XsBackendStatus xs_llvm_lower_integer_constant(XsLlvmBackend *backend, const XsLilBlock *block, size_t index,
                                               LLVMValueRef *value, XsBackendError *error)
{
  XsLilTypeKind integer_kind = XS_LIL_TYPE_VOID;
  if(value == nullptr || !integer_constant_type(xs_lil_block_instruction_kind(block, index), &integer_kind))
  {
    if(error != nullptr)
    {
      error->status = XS_BACKEND_INVALID_ARGUMENT;
      snprintf(error->message, sizeof(error->message), "%s", "valid XLIL integer constant is required");
    }
    return XS_BACKEND_INVALID_ARGUMENT;
  }
  LLVMTypeRef type = nullptr;
  XsBackendStatus status = xs_llvm_lil_type(backend, (XsLilType){.kind = integer_kind}, &type, error);
  if(status != XS_BACKEND_OK)
    return status;
  XsUInt128 bits = xs_lil_block_instruction_integer_bits(block, index);
  if(integer_kind == XS_LIL_TYPE_U128 || integer_kind == XS_LIL_TYPE_I128)
  {
    uint64_t words[2] = {bits.low, bits.high};
    *value = LLVMConstIntOfArbitraryPrecision(type, 2, words);
  }
  else
  {
    *value = LLVMConstInt(type, bits.low, false);
  }
  return XS_BACKEND_OK;
}

static XsBackendStatus integer_error(XsBackendError *error, XsBackendStatus status, const char *message)
{
  if(error != nullptr)
  {
    error->status = status;
    snprintf(error->message, sizeof(error->message), "%s", message);
  }
  return status;
}

static bool is_signed_integer(XsLilTypeKind type)
{
  return type == XS_LIL_TYPE_I8 || type == XS_LIL_TYPE_I16 || type == XS_LIL_TYPE_I32 || type == XS_LIL_TYPE_I64 ||
         type == XS_LIL_TYPE_I128;
}

static LLVMValueRef lower_comparison(LLVMBuilderRef builder, XsLilIntegerBinaryOperation operation, bool is_signed,
                                     LLVMValueRef left, LLVMValueRef right)
{
  LLVMIntPredicate predicate = LLVMIntEQ;
  switch(operation)
  {
  case XS_LIL_INTEGER_EQUAL:
    predicate = LLVMIntEQ;
    break;
  case XS_LIL_INTEGER_NOT_EQUAL:
    predicate = LLVMIntNE;
    break;
  case XS_LIL_INTEGER_LESS:
    predicate = is_signed ? LLVMIntSLT : LLVMIntULT;
    break;
  case XS_LIL_INTEGER_LESS_EQUAL:
    predicate = is_signed ? LLVMIntSLE : LLVMIntULE;
    break;
  case XS_LIL_INTEGER_GREATER:
    predicate = is_signed ? LLVMIntSGT : LLVMIntUGT;
    break;
  case XS_LIL_INTEGER_GREATER_EQUAL:
    predicate = is_signed ? LLVMIntSGE : LLVMIntUGE;
    break;
  default:
    return nullptr;
  }
  return LLVMBuildICmp(builder, predicate, left, right, "icmp");
}

static LLVMValueRef lower_operation(LLVMBuilderRef builder, XsLilIntegerBinaryOperation operation, bool is_signed,
                                    LLVMValueRef left, LLVMValueRef right)
{
  switch(operation)
  {
  case XS_LIL_INTEGER_ADD:
    return LLVMBuildAdd(builder, left, right, "add");
  case XS_LIL_INTEGER_SUB:
    return LLVMBuildSub(builder, left, right, "sub");
  case XS_LIL_INTEGER_MUL:
    return LLVMBuildMul(builder, left, right, "mul");
  case XS_LIL_INTEGER_DIV:
    return is_signed ? LLVMBuildSDiv(builder, left, right, "div") : LLVMBuildUDiv(builder, left, right, "div");
  case XS_LIL_INTEGER_REM:
    return is_signed ? LLVMBuildSRem(builder, left, right, "rem") : LLVMBuildURem(builder, left, right, "rem");
  case XS_LIL_INTEGER_BIT_AND:
    return LLVMBuildAnd(builder, left, right, "and");
  case XS_LIL_INTEGER_BIT_OR:
    return LLVMBuildOr(builder, left, right, "or");
  case XS_LIL_INTEGER_BIT_XOR:
    return LLVMBuildXor(builder, left, right, "xor");
  case XS_LIL_INTEGER_SHIFT_LEFT:
    return LLVMBuildShl(builder, left, right, "shl");
  case XS_LIL_INTEGER_SHIFT_RIGHT:
    return is_signed ? LLVMBuildAShr(builder, left, right, "shr") : LLVMBuildLShr(builder, left, right, "shr");
  default:
    return lower_comparison(builder, operation, is_signed, left, right);
  }
}

XsBackendStatus xs_llvm_lower_integer_operation(LLVMBuilderRef builder, const XsLilBlock *block, size_t index,
                                                LLVMValueRef *values, size_t value_count, XsBackendError *error)
{
  XsLilValueId result = xs_lil_block_instruction_result(block, index);
  XsLilValueId left = xs_lil_block_instruction_left(block, index);
  XsLilValueId right = xs_lil_block_instruction_right(block, index);
  if((size_t)result >= value_count || (size_t)left >= value_count || (size_t)right >= value_count ||
     values[left] == nullptr || values[right] == nullptr)
    return integer_error(error, XS_BACKEND_INVALID_ARGUMENT, "XLIL integer operation references an unavailable value");
  XsLilType type = xs_lil_block_instruction_integer_type(block, index);
  values[result] = lower_operation(builder, xs_lil_block_instruction_integer_operation(block, index),
                                   is_signed_integer(type.kind), values[left], values[right]);
  if(values[result] == nullptr)
    return integer_error(error, XS_BACKEND_LLVM_ERROR, "LLVM could not lower XLIL integer operation");
  return XS_BACKEND_OK;
}
