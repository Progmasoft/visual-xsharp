/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "model_internal.h"

#include <stdio.h>

static XsLilStatus write_type(FILE *stream, XsLilError *error, XsLilType type)
{
  int written = 0;
  if(type.kind == XS_LIL_TYPE_AGGREGATE)
    written = fprintf(stream, "%%t%u", type.registry_id);
  else if(type.kind == XS_LIL_TYPE_ARRAY)
    written = fprintf(stream, "%%a%u", type.registry_id);
  else
    written = fprintf(stream, "%s", xs_lil_type_name(type));
  return written < 0 ? xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL type") : XS_LIL_OK;
}

static XsLilStatus write_signature(FILE *stream, XsLilError *error, const XsLilFunction *function)
{
  if(fprintf(stream, "%s : (", function->name) < 0)
    return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL function signature");
  for(size_t parameter = 0; parameter < function->parameter_count; ++parameter)
  {
    if(parameter != 0 && xs_lil_write_checked(stream, error, ", ") != XS_LIL_OK)
      return error == nullptr ? XS_LIL_IO_ERROR : error->status;
    if(write_type(stream, error, function->parameters[parameter]) != XS_LIL_OK)
      return error == nullptr ? XS_LIL_IO_ERROR : error->status;
  }
  if(xs_lil_write_checked(stream, error, ") -> ") != XS_LIL_OK ||
     write_type(stream, error, function->return_type) != XS_LIL_OK)
    return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL function signature");
  if(xs_lil_write_checked(stream, error, "\n") != XS_LIL_OK)
    return error == nullptr ? XS_LIL_IO_ERROR : error->status;
  return XS_LIL_OK;
}

static const char *float_instruction_name(XsLilInstructionKind kind)
{
  switch(kind)
  {
  case XS_LIL_INSTRUCTION_ADD_F32:
  case XS_LIL_INSTRUCTION_ADD_F64:
    return "add";
  case XS_LIL_INSTRUCTION_SUB_F32:
  case XS_LIL_INSTRUCTION_SUB_F64:
    return "sub";
  case XS_LIL_INSTRUCTION_MUL_F32:
  case XS_LIL_INSTRUCTION_MUL_F64:
    return "mul";
  case XS_LIL_INSTRUCTION_DIV_F32:
  case XS_LIL_INSTRUCTION_DIV_F64:
    return "div";
  case XS_LIL_INSTRUCTION_REM_F32:
  case XS_LIL_INSTRUCTION_REM_F64:
    return "rem";
  case XS_LIL_INSTRUCTION_EQ_F32:
  case XS_LIL_INSTRUCTION_EQ_F64:
    return "eq";
  case XS_LIL_INSTRUCTION_NE_F32:
  case XS_LIL_INSTRUCTION_NE_F64:
    return "ne";
  case XS_LIL_INSTRUCTION_LT_F32:
  case XS_LIL_INSTRUCTION_LT_F64:
    return "lt";
  case XS_LIL_INSTRUCTION_LE_F32:
  case XS_LIL_INSTRUCTION_LE_F64:
    return "le";
  case XS_LIL_INSTRUCTION_GT_F32:
  case XS_LIL_INSTRUCTION_GT_F64:
    return "gt";
  case XS_LIL_INSTRUCTION_GE_F32:
  case XS_LIL_INSTRUCTION_GE_F64:
    return "ge";
  default:
    return nullptr;
  }
}

static XsLilStatus write_float_instruction(FILE *stream, const XsLilInstruction *instruction, XsLilError *error)
{
  const char *operation = float_instruction_name(instruction->kind);
  if(operation == nullptr)
    return XS_LIL_OK;
  bool f32 = instruction->kind >= XS_LIL_INSTRUCTION_ADD_F32 && instruction->kind <= XS_LIL_INSTRUCTION_GE_F32;
  bool comparison =
      (instruction->kind >= XS_LIL_INSTRUCTION_EQ_F32 && instruction->kind <= XS_LIL_INSTRUCTION_GE_F32) ||
      instruction->kind >= XS_LIL_INSTRUCTION_EQ_F64;
  const char *type = f32 ? "f32" : "f64";
  if(fprintf(stream, "  %%r%u:%s = %s.%s %%r%u, %%r%u\n", instruction->result, comparison ? "bool" : type, operation,
             type, instruction->left, instruction->right) < 0)
    return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL floating instruction");
  return XS_LIL_OK;
}

static const char *integer_constant_type(XsLilInstructionKind kind, size_t *digits)
{
  switch(kind)
  {
  case XS_LIL_INSTRUCTION_CONST_U8:
    *digits = 2;
    return "u8";
  case XS_LIL_INSTRUCTION_CONST_I8:
    *digits = 2;
    return "i8";
  case XS_LIL_INSTRUCTION_CONST_I16:
    *digits = 4;
    return "i16";
  case XS_LIL_INSTRUCTION_CONST_U32:
    *digits = 8;
    return "u32";
  case XS_LIL_INSTRUCTION_CONST_U64:
    *digits = 16;
    return "u64";
  case XS_LIL_INSTRUCTION_CONST_U128:
    *digits = 32;
    return "u128";
  case XS_LIL_INSTRUCTION_CONST_I128:
    *digits = 32;
    return "i128";
  default:
    return nullptr;
  }
}

static XsLilStatus write_integer_constant(FILE *stream, const XsLilInstruction *instruction, XsLilError *error)
{
  size_t digits = 0;
  const char *type = integer_constant_type(instruction->kind, &digits);
  if(type == nullptr)
    return XS_LIL_OK;
  char hexadecimal[33];
  xs_uint128_format_hex(instruction->immediate_integer_bits, hexadecimal);
  if(fprintf(stream, "  %%r%u:%s = const.%s 0x%s\n", instruction->result, type, type, hexadecimal + 32U - digits) < 0)
    return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL integer constant");
  return XS_LIL_OK;
}

static XsLilStatus write_integer_operation(FILE *stream, const XsLilInstruction *instruction, XsLilError *error)
{
  if(instruction->kind != XS_LIL_INSTRUCTION_BINARY_INTEGER)
    return XS_LIL_OK;
  const char *operation = xs_lil_integer_operation_name(instruction->integer_operation);
  const char *operand_type = xs_lil_type_name(instruction->integer_type);
  const char *result_type =
      xs_lil_integer_operation_is_comparison(instruction->integer_operation) ? "bool" : operand_type;
  if(operation == nullptr || fprintf(stream, "  %%r%u:%s = %s.%s %%r%u, %%r%u\n", instruction->result, result_type,
                                     operation, operand_type, instruction->left, instruction->right) < 0)
    return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL integer operation");
  return XS_LIL_OK;
}

static XsLilStatus write_block(FILE *stream, XsLilError *error, const XsLilBlock *block)
{
  if(fprintf(stream, "bb%u.%s:\n", block->id, block->label) < 0)
    return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL block label");
  for(size_t i = 0; i < block->instruction_count; ++i)
  {
    const XsLilInstruction *instruction = &block->instructions[i];
    if(instruction->kind == XS_LIL_INSTRUCTION_CONST_I64 &&
       fprintf(stream, "  %%r%u:i64 = const.i64 %lld\n", instruction->result, (long long)instruction->immediate_i64) <
           0)
      return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL const.i64 instruction");
    if(instruction->kind == XS_LIL_INSTRUCTION_CONST_I32 &&
       fprintf(stream, "  %%r%u:i32 = const.i32 %d\n", instruction->result, (int)instruction->immediate_i64) < 0)
      return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL const.i32 instruction");
    if(instruction->kind == XS_LIL_INSTRUCTION_CONST_U16 &&
       fprintf(stream, "  %%r%u:u16 = const.u16 0x%04x\n", instruction->result,
               (unsigned int)instruction->immediate_integer_bits.low) < 0)
      return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL const.u16 instruction");
    if(write_integer_constant(stream, instruction, error) != XS_LIL_OK)
      return error == nullptr ? XS_LIL_IO_ERROR : error->status;
    if(instruction->kind == XS_LIL_INSTRUCTION_CONST_BOOL &&
       fprintf(stream, "  %%r%u:bool = const.bool %s\n", instruction->result,
               instruction->immediate_bool ? "true" : "false") < 0)
      return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL const.bool instruction");
    if(instruction->kind == XS_LIL_INSTRUCTION_CONST_STR)
    {
      const char *encoding = instruction->utf32_encoding == XS_LIL_UTF32_LE ? "utf32le" : "utf32be";
      if(fprintf(stream, "  %%r%u:str = const.str %s [", instruction->result, encoding) < 0)
        return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL const.str instruction");
      for(size_t unit = 0; unit < instruction->utf32_length; ++unit)
      {
        if(unit != 0 && fputs(", ", stream) == EOF)
          return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL const.str separator");
        if(fprintf(stream, "0x%08x", instruction->utf32_units[unit]) < 0)
          return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL const.str code unit");
      }
      if(fputs("]\n", stream) == EOF)
        return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not finish XLIL const.str instruction");
    }
    if((instruction->kind == XS_LIL_INSTRUCTION_EQ_STR || instruction->kind == XS_LIL_INSTRUCTION_NE_STR) &&
       fprintf(stream, "  %%r%u:bool = %s.str %%r%u, %%r%u\n", instruction->result,
               instruction->kind == XS_LIL_INSTRUCTION_EQ_STR ? "eq" : "ne", instruction->left, instruction->right) < 0)
      return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL Str comparison");
    if(instruction->kind == XS_LIL_INSTRUCTION_CONST_F32 &&
       fprintf(stream, "  %%r%u:f32 = const.f32 0x%08llx\n", instruction->result,
               (unsigned long long)instruction->immediate_float_bits) < 0)
      return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL const.f32 instruction");
    if(instruction->kind == XS_LIL_INSTRUCTION_CONST_F64 &&
       fprintf(stream, "  %%r%u:f64 = const.f64 0x%016llx\n", instruction->result,
               (unsigned long long)instruction->immediate_float_bits) < 0)
      return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL const.f64 instruction");
    if(float_instruction_name(instruction->kind) != nullptr &&
       write_float_instruction(stream, instruction, error) != XS_LIL_OK)
      return error == nullptr ? XS_LIL_IO_ERROR : error->status;
    if(write_integer_operation(stream, instruction, error) != XS_LIL_OK)
      return error == nullptr ? XS_LIL_IO_ERROR : error->status;
    if(instruction->kind == XS_LIL_INSTRUCTION_ADD_I64 &&
       fprintf(stream, "  %%r%u:i64 = add.i64 %%r%u, %%r%u\n", instruction->result, instruction->left,
               instruction->right) < 0)
      return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL add.i64 instruction");
    if(instruction->kind == XS_LIL_INSTRUCTION_SUB_I64 &&
       fprintf(stream, "  %%r%u:i64 = sub.i64 %%r%u, %%r%u\n", instruction->result, instruction->left,
               instruction->right) < 0)
      return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL sub.i64 instruction");
    if(instruction->kind == XS_LIL_INSTRUCTION_MUL_I64 &&
       fprintf(stream, "  %%r%u:i64 = mul.i64 %%r%u, %%r%u\n", instruction->result, instruction->left,
               instruction->right) < 0)
      return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL mul.i64 instruction");
    if(instruction->kind == XS_LIL_INSTRUCTION_DIV_I64 &&
       fprintf(stream, "  %%r%u:i64 = div.i64 %%r%u, %%r%u\n", instruction->result, instruction->left,
               instruction->right) < 0)
      return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL div.i64 instruction");
    if(instruction->kind == XS_LIL_INSTRUCTION_REM_I64 &&
       fprintf(stream, "  %%r%u:i64 = rem.i64 %%r%u, %%r%u\n", instruction->result, instruction->left,
               instruction->right) < 0)
      return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL rem.i64 instruction");
    if(instruction->kind == XS_LIL_INSTRUCTION_AND_I64 &&
       fprintf(stream, "  %%r%u:i64 = and.i64 %%r%u, %%r%u\n", instruction->result, instruction->left,
               instruction->right) < 0)
      return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL and.i64 instruction");
    if(instruction->kind == XS_LIL_INSTRUCTION_OR_I64 &&
       fprintf(stream, "  %%r%u:i64 = or.i64 %%r%u, %%r%u\n", instruction->result, instruction->left,
               instruction->right) < 0)
      return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL or.i64 instruction");
    if(instruction->kind == XS_LIL_INSTRUCTION_XOR_I64 &&
       fprintf(stream, "  %%r%u:i64 = xor.i64 %%r%u, %%r%u\n", instruction->result, instruction->left,
               instruction->right) < 0)
      return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL xor.i64 instruction");
    if(instruction->kind == XS_LIL_INSTRUCTION_SHL_I64 &&
       fprintf(stream, "  %%r%u:i64 = shl.i64 %%r%u, %%r%u\n", instruction->result, instruction->left,
               instruction->right) < 0)
      return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL shl.i64 instruction");
    if(instruction->kind == XS_LIL_INSTRUCTION_SHR_I64 &&
       fprintf(stream, "  %%r%u:i64 = shr.i64 %%r%u, %%r%u\n", instruction->result, instruction->left,
               instruction->right) < 0)
      return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL shr.i64 instruction");
    if(instruction->kind == XS_LIL_INSTRUCTION_EQ_I64 &&
       fprintf(stream, "  %%r%u:bool = eq.i64 %%r%u, %%r%u\n", instruction->result, instruction->left,
               instruction->right) < 0)
      return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL eq.i64 instruction");
    if(instruction->kind == XS_LIL_INSTRUCTION_NE_I64 &&
       fprintf(stream, "  %%r%u:bool = ne.i64 %%r%u, %%r%u\n", instruction->result, instruction->left,
               instruction->right) < 0)
      return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL ne.i64 instruction");
    if(instruction->kind == XS_LIL_INSTRUCTION_LT_I64 &&
       fprintf(stream, "  %%r%u:bool = lt.i64 %%r%u, %%r%u\n", instruction->result, instruction->left,
               instruction->right) < 0)
      return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL lt.i64 instruction");
    if(instruction->kind == XS_LIL_INSTRUCTION_LE_I64 &&
       fprintf(stream, "  %%r%u:bool = le.i64 %%r%u, %%r%u\n", instruction->result, instruction->left,
               instruction->right) < 0)
      return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL le.i64 instruction");
    if(instruction->kind == XS_LIL_INSTRUCTION_GT_I64 &&
       fprintf(stream, "  %%r%u:bool = gt.i64 %%r%u, %%r%u\n", instruction->result, instruction->left,
               instruction->right) < 0)
      return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL gt.i64 instruction");
    if(instruction->kind == XS_LIL_INSTRUCTION_GE_I64 &&
       fprintf(stream, "  %%r%u:bool = ge.i64 %%r%u, %%r%u\n", instruction->result, instruction->left,
               instruction->right) < 0)
      return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL ge.i64 instruction");
    if(instruction->kind == XS_LIL_INSTRUCTION_ADD_I32 &&
       fprintf(stream, "  %%r%u:i32 = add.i32 %%r%u, %%r%u\n", instruction->result, instruction->left,
               instruction->right) < 0)
      return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL add.i32 instruction");
    if(instruction->kind == XS_LIL_INSTRUCTION_SUB_I32 &&
       fprintf(stream, "  %%r%u:i32 = sub.i32 %%r%u, %%r%u\n", instruction->result, instruction->left,
               instruction->right) < 0)
      return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL sub.i32 instruction");
    if(instruction->kind == XS_LIL_INSTRUCTION_MUL_I32 &&
       fprintf(stream, "  %%r%u:i32 = mul.i32 %%r%u, %%r%u\n", instruction->result, instruction->left,
               instruction->right) < 0)
      return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL mul.i32 instruction");
    if(instruction->kind == XS_LIL_INSTRUCTION_DIV_I32 &&
       fprintf(stream, "  %%r%u:i32 = div.i32 %%r%u, %%r%u\n", instruction->result, instruction->left,
               instruction->right) < 0)
      return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL div.i32 instruction");
    if(instruction->kind == XS_LIL_INSTRUCTION_REM_I32 &&
       fprintf(stream, "  %%r%u:i32 = rem.i32 %%r%u, %%r%u\n", instruction->result, instruction->left,
               instruction->right) < 0)
      return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL rem.i32 instruction");
    if(instruction->kind == XS_LIL_INSTRUCTION_AND_I32 &&
       fprintf(stream, "  %%r%u:i32 = and.i32 %%r%u, %%r%u\n", instruction->result, instruction->left,
               instruction->right) < 0)
      return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL and.i32 instruction");
    if(instruction->kind == XS_LIL_INSTRUCTION_OR_I32 &&
       fprintf(stream, "  %%r%u:i32 = or.i32 %%r%u, %%r%u\n", instruction->result, instruction->left,
               instruction->right) < 0)
      return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL or.i32 instruction");
    if(instruction->kind == XS_LIL_INSTRUCTION_XOR_I32 &&
       fprintf(stream, "  %%r%u:i32 = xor.i32 %%r%u, %%r%u\n", instruction->result, instruction->left,
               instruction->right) < 0)
      return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL xor.i32 instruction");
    if(instruction->kind == XS_LIL_INSTRUCTION_SHL_I32 &&
       fprintf(stream, "  %%r%u:i32 = shl.i32 %%r%u, %%r%u\n", instruction->result, instruction->left,
               instruction->right) < 0)
      return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL shl.i32 instruction");
    if(instruction->kind == XS_LIL_INSTRUCTION_SHR_I32 &&
       fprintf(stream, "  %%r%u:i32 = shr.i32 %%r%u, %%r%u\n", instruction->result, instruction->left,
               instruction->right) < 0)
      return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL shr.i32 instruction");
    if(instruction->kind == XS_LIL_INSTRUCTION_EQ_I32 &&
       fprintf(stream, "  %%r%u:bool = eq.i32 %%r%u, %%r%u\n", instruction->result, instruction->left,
               instruction->right) < 0)
      return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL eq.i32 instruction");
    if(instruction->kind == XS_LIL_INSTRUCTION_NE_I32 &&
       fprintf(stream, "  %%r%u:bool = ne.i32 %%r%u, %%r%u\n", instruction->result, instruction->left,
               instruction->right) < 0)
      return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL ne.i32 instruction");
    if(instruction->kind == XS_LIL_INSTRUCTION_LT_I32 &&
       fprintf(stream, "  %%r%u:bool = lt.i32 %%r%u, %%r%u\n", instruction->result, instruction->left,
               instruction->right) < 0)
      return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL lt.i32 instruction");
    if(instruction->kind == XS_LIL_INSTRUCTION_LE_I32 &&
       fprintf(stream, "  %%r%u:bool = le.i32 %%r%u, %%r%u\n", instruction->result, instruction->left,
               instruction->right) < 0)
      return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL le.i32 instruction");
    if(instruction->kind == XS_LIL_INSTRUCTION_GT_I32 &&
       fprintf(stream, "  %%r%u:bool = gt.i32 %%r%u, %%r%u\n", instruction->result, instruction->left,
               instruction->right) < 0)
      return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL gt.i32 instruction");
    if(instruction->kind == XS_LIL_INSTRUCTION_GE_I32 &&
       fprintf(stream, "  %%r%u:bool = ge.i32 %%r%u, %%r%u\n", instruction->result, instruction->left,
               instruction->right) < 0)
      return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL ge.i32 instruction");
    if(instruction->kind == XS_LIL_INSTRUCTION_NOT_BOOL &&
       fprintf(stream, "  %%r%u:bool = not.bool %%r%u\n", instruction->result, instruction->left) < 0)
      return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL not.bool instruction");
    if(instruction->kind == XS_LIL_INSTRUCTION_CALL)
    {
      if(instruction->result != UINT32_MAX)
      {
        if(fprintf(stream, "  %%r%u:", instruction->result) < 0 ||
           write_type(stream, error, block->owner->values[instruction->result].type) != XS_LIL_OK ||
           xs_lil_write_checked(stream, error, " = ") != XS_LIL_OK)
          return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL call result");
      }
      if(fprintf(stream, "%scall %s(", instruction->result == UINT32_MAX ? "  " : "", instruction->callee) < 0)
        return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL call instruction");
      for(size_t argument = 0; argument < instruction->argument_count; ++argument)
      {
        if((argument != 0 && xs_lil_write_checked(stream, error, ", ") != XS_LIL_OK) ||
           fprintf(stream, "%%r%u", instruction->arguments[argument]) < 0)
          return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL call argument");
      }
      if(xs_lil_write_checked(stream, error, ")\n") != XS_LIL_OK)
        return error == nullptr ? XS_LIL_IO_ERROR : error->status;
    }
    if(instruction->kind == XS_LIL_INSTRUCTION_AGGREGATE)
    {
      bool array = instruction->integer_type.kind == XS_LIL_TYPE_ARRAY;
      if(fprintf(stream, "  %%r%u:", instruction->result) < 0 ||
         write_type(stream, error, instruction->integer_type) != XS_LIL_OK ||
         xs_lil_write_checked(stream, error, array ? " = array " : " = aggregate ") != XS_LIL_OK)
        return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL composite instruction");
      for(size_t field = 0; field < instruction->argument_count; ++field)
      {
        if((field != 0 && xs_lil_write_checked(stream, error, ", ") != XS_LIL_OK) ||
           fprintf(stream, "%%r%u", instruction->arguments[field]) < 0)
          return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL aggregate field");
      }
      if(xs_lil_write_checked(stream, error, "\n") != XS_LIL_OK)
        return error == nullptr ? XS_LIL_IO_ERROR : error->status;
    }
    if(instruction->kind == XS_LIL_INSTRUCTION_EXTRACT)
    {
      bool array = block->owner->values[instruction->left].type.kind == XS_LIL_TYPE_ARRAY;
      if(fprintf(stream, "  %%r%u:", instruction->result) < 0 ||
         write_type(stream, error, block->owner->values[instruction->result].type) != XS_LIL_OK ||
         fprintf(stream, array ? " = extract.array %%r%u, %lld\n" : " = extract %%r%u, %lld\n", instruction->left,
                 (long long)instruction->immediate_i64) < 0)
        return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL extract instruction");
    }
    if(instruction->kind == XS_LIL_INSTRUCTION_ARRAY_GET || instruction->kind == XS_LIL_INSTRUCTION_ARRAY_SET)
    {
      if(fprintf(stream, "  %%r%u:", instruction->result) < 0 ||
         write_type(stream, error, block->owner->values[instruction->result].type) != XS_LIL_OK ||
         (instruction->kind == XS_LIL_INSTRUCTION_ARRAY_GET
              ? fprintf(stream, " = array.get %%r%u, %%r%u\n", instruction->left, instruction->right)
              : fprintf(stream, " = array.set %%r%u, %%r%u, %%r%u\n", instruction->left, instruction->right,
                        instruction->arguments[0])) < 0)
        return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL dynamic array instruction");
    }
    if(instruction->kind == XS_LIL_INSTRUCTION_ARRAY_LENGTH &&
       fprintf(stream, "  %%r%u:i64 = len.array %%r%u\n", instruction->result, instruction->left) < 0)
      return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL array length instruction");
    if(instruction->kind == XS_LIL_INSTRUCTION_LOAD)
    {
      if(fprintf(stream, "  %%r%u:", instruction->result) < 0 ||
         write_type(stream, error, block->owner->values[instruction->result].type) != XS_LIL_OK ||
         fprintf(stream, " = load %%s%u\n", instruction->slot) < 0)
        return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL load instruction");
    }
    if(instruction->kind == XS_LIL_INSTRUCTION_STORE &&
       fprintf(stream, "  store %%r%u, %%s%u\n", instruction->left, instruction->slot) < 0)
      return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL store instruction");
  }
  if(block->terminator.kind == XS_LIL_TERMINATOR_RETURN)
  {
    if(block->terminator.has_value)
    {
      if(fprintf(stream, "  ret %%r%u\n", block->terminator.value) < 0)
        return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL return terminator");
    }
    else if(xs_lil_write_checked(stream, error, "  ret\n") != XS_LIL_OK)
    {
      return error == nullptr ? XS_LIL_IO_ERROR : error->status;
    }
  }
  else if(block->terminator.kind == XS_LIL_TERMINATOR_BRANCH)
  {
    if(fprintf(stream, "  br bb%u\n", block->terminator.target) < 0)
      return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL branch terminator");
  }
  else if(block->terminator.kind == XS_LIL_TERMINATOR_BRANCH_IF)
  {
    if(fprintf(stream, "  br_if %%r%u, bb%u, bb%u\n", block->terminator.condition, block->terminator.target,
               block->terminator.else_target) < 0)
      return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL branch_if terminator");
  }
  else if(block->terminator.kind == XS_LIL_TERMINATOR_PANIC)
  {
    if(xs_lil_write_checked(stream, error, "  panic\n") != XS_LIL_OK)
      return error == nullptr ? XS_LIL_IO_ERROR : error->status;
  }
  else if(block->terminator.kind == XS_LIL_TERMINATOR_NONE)
  {
    if(xs_lil_write_checked(stream, error, "  .missing_terminator\n") != XS_LIL_OK)
      return error == nullptr ? XS_LIL_IO_ERROR : error->status;
  }
  return XS_LIL_OK;
}

XsLilStatus xs_lil_module_write_text(const XsLilModule *module, FILE *stream, XsLilError *error)
{
  xs_lil_clear_error(error);
  if(module == nullptr || stream == nullptr)
    return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL module and stream are required");
  if(xs_lil_write_checked(stream, error, ".xlil version 0\n") != XS_LIL_OK)
    return error == nullptr ? XS_LIL_IO_ERROR : error->status;
  if(fprintf(stream, ".xlil module %s\n", module->name) < 0)
    return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL module header");
  for(size_t type = 0; type < module->aggregate_type_count; ++type)
  {
    const XsLilAggregateType *aggregate = &module->aggregate_types[type];
    if(fprintf(stream, ".type %%t%zu %s : (", type, aggregate->name) < 0)
      return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL aggregate type");
    for(size_t field = 0; field < aggregate->field_count; ++field)
    {
      if((field != 0 && xs_lil_write_checked(stream, error, ", ") != XS_LIL_OK) ||
         write_type(stream, error, aggregate->fields[field]) != XS_LIL_OK)
        return error == nullptr ? XS_LIL_IO_ERROR : error->status;
    }
    if(xs_lil_write_checked(stream, error, ")\n") != XS_LIL_OK)
      return error == nullptr ? XS_LIL_IO_ERROR : error->status;
  }
  for(size_t type = 0; type < module->array_type_count; ++type)
  {
    const XsLilArrayType *array = &module->array_types[type];
    if(fprintf(stream, ".array %%a%zu : ", type) < 0 || write_type(stream, error, array->element_type) != XS_LIL_OK ||
       (array->dynamic
            ? xs_lil_write_checked(stream, error, "\n")
            : (fprintf(stream, " x %llu\n", (unsigned long long)array->length) < 0 ? XS_LIL_IO_ERROR : XS_LIL_OK)) !=
           XS_LIL_OK)
      return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL array type");
  }
  for(size_t i = 0; i < module->function_count; ++i)
  {
    const XsLilFunction *function = module->functions[i];
    if(xs_lil_write_checked(stream, error, function->is_definition ? ".func " : ".extern ") != XS_LIL_OK)
      return error == nullptr ? XS_LIL_IO_ERROR : error->status;
    if(write_signature(stream, error, function) != XS_LIL_OK)
      return error == nullptr ? XS_LIL_IO_ERROR : error->status;
    if(!function->is_definition)
      continue;
    for(size_t parameter = 0; parameter < function->parameter_count; ++parameter)
    {
      if(fprintf(stream, ".param %%r%zu:", parameter) < 0 ||
         write_type(stream, error, function->parameters[parameter]) != XS_LIL_OK ||
         xs_lil_write_checked(stream, error, "\n") != XS_LIL_OK)
        return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL function parameter");
    }
    for(size_t slot = 0; slot < function->slot_count; ++slot)
    {
      if(fprintf(stream, ".slot %%s%zu:", slot) < 0 ||
         write_type(stream, error, function->slots[slot].type) != XS_LIL_OK ||
         xs_lil_write_checked(stream, error, "\n") != XS_LIL_OK)
        return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL stack slot");
    }
    for(size_t block = 0; block < function->block_count; ++block)
    {
      if(write_block(stream, error, function->blocks[block]) != XS_LIL_OK)
        return error == nullptr ? XS_LIL_IO_ERROR : error->status;
    }
    if(xs_lil_write_checked(stream, error, ".end\n") != XS_LIL_OK)
      return error == nullptr ? XS_LIL_IO_ERROR : error->status;
  }
  return XS_LIL_OK;
}
