/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <xs-lang.chess031@slmails.com>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "model_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void xs_lil_clear_error(XsLilError *error)
{
  if(error != nullptr)
    *error = (XsLilError){.status = XS_LIL_OK};
}

XsLilStatus xs_lil_set_error(XsLilError *error, XsLilStatus status, const char *message)
{
  if(error != nullptr)
  {
    error->status = status;
    snprintf(error->message, sizeof(error->message), "%s", message == nullptr ? "XLIL error" : message);
  }
  return status;
}

char *xs_lil_copy_text(const char *text)
{
  size_t length = strlen(text);
  char *copy = malloc(length + 1);
  if(copy != nullptr)
    memcpy(copy, text, length + 1);
  return copy;
}

char *xs_lil_copy_span(const char *text, size_t length)
{
  char *copy = malloc(length + 1);
  if(copy != nullptr)
  {
    memcpy(copy, text, length);
    copy[length] = '\0';
  }
  return copy;
}

XsLilStatus xs_lil_write_checked(FILE *stream, XsLilError *error, const char *text)
{
  if(fputs(text, stream) == EOF)
    return xs_lil_set_error(error, XS_LIL_IO_ERROR, "could not write XLIL text");
  return XS_LIL_OK;
}

static void function_free(XsLilFunction *function)
{
  for(size_t i = 0; i < function->block_count; ++i)
  {
    XsLilBlock *block = function->blocks[i];
    free(block->label);
    for(size_t instruction = 0; instruction < block->instruction_count; ++instruction)
    {
      free(block->instructions[instruction].callee);
      free(block->instructions[instruction].arguments);
      free(block->instructions[instruction].utf16_units);
    }
    free(block->instructions);
    free(block);
  }
  free(function->blocks);
  free(function->slots);
  free(function->values);
  free(function->name);
  free(function->parameters);
  *function = (XsLilFunction){0};
}

XsLilStatus xs_lil_module_create(const char *name, XsLilModule **module, XsLilError *error)
{
  xs_lil_clear_error(error);
  if(name == nullptr || name[0] == '\0' || module == nullptr)
    return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL module name and output are required");
  *module = calloc(1, sizeof(**module));
  if(*module == nullptr)
    return xs_lil_set_error(error, XS_LIL_ALLOCATION_FAILED, "out of memory while creating XLIL module");
  (*module)->name = xs_lil_copy_text(name);
  if((*module)->name == nullptr)
  {
    xs_lil_module_destroy(*module);
    *module = nullptr;
    return xs_lil_set_error(error, XS_LIL_ALLOCATION_FAILED, "out of memory while naming XLIL module");
  }
  return XS_LIL_OK;
}

void xs_lil_module_destroy(XsLilModule *module)
{
  if(module == nullptr)
    return;
  for(size_t i = 0; i < module->function_count; ++i)
  {
    function_free(module->functions[i]);
    free(module->functions[i]);
  }
  for(size_t i = 0; i < module->aggregate_type_count; ++i)
  {
    free(module->aggregate_types[i].fields);
    free(module->aggregate_types[i].name);
  }
  free(module->aggregate_types);
  free(module->array_types);
  free(module->functions);
  free(module->name);
  free(module);
}

const char *xs_lil_module_name(const XsLilModule *module)
{
  return module == nullptr ? nullptr : module->name;
}

static XsLilStatus append_function(XsLilModule *module, XsLilFunction *function, XsLilError *error)
{
  if(module->function_count == module->function_capacity)
  {
    size_t capacity = module->function_capacity == 0 ? 8 : module->function_capacity * 2;
    XsLilFunction **functions = realloc(module->functions, capacity * sizeof(*functions));
    if(functions == nullptr)
      return xs_lil_set_error(error, XS_LIL_ALLOCATION_FAILED, "out of memory while adding XLIL function");
    module->functions = functions;
    module->function_capacity = capacity;
  }
  module->functions[module->function_count++] = function;
  return XS_LIL_OK;
}

XsLilStatus xs_lil_add_value(XsLilFunction *function, XsLilType type, XsLilValueId *value, XsLilError *error)
{
  if(function->value_count == function->value_capacity)
  {
    size_t capacity = function->value_capacity == 0 ? 8 : function->value_capacity * 2;
    XsLilValue *values = realloc(function->values, capacity * sizeof(*values));
    if(values == nullptr)
      return xs_lil_set_error(error, XS_LIL_ALLOCATION_FAILED, "out of memory while adding XLIL value");
    function->values = values;
    function->value_capacity = capacity;
  }
  *value = (XsLilValueId)function->value_count;
  function->values[function->value_count++] = (XsLilValue){.type = type};
  return XS_LIL_OK;
}

static XsLilStatus add_function(XsLilModule *module, const char *name, XsLilType return_type,
                                const XsLilType *parameters, size_t parameter_count, bool is_definition,
                                XsLilFunction **out_function, XsLilError *error)
{
  xs_lil_clear_error(error);
  if(out_function != nullptr)
    *out_function = nullptr;
  if(module == nullptr || name == nullptr || name[0] == '\0' || (parameter_count != 0 && parameters == nullptr))
    return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "valid XLIL module and function signature are required");
  XsLilFunction *function = calloc(1, sizeof(*function));
  if(function == nullptr)
    return xs_lil_set_error(error, XS_LIL_ALLOCATION_FAILED, "out of memory while creating XLIL function");
  *function = (XsLilFunction){
      .owner = module,
      .name = xs_lil_copy_text(name),
      .return_type = return_type,
      .parameter_count = parameter_count,
      .is_definition = is_definition,
  };
  if(function->name == nullptr)
  {
    free(function);
    return xs_lil_set_error(error, XS_LIL_ALLOCATION_FAILED, "out of memory while naming XLIL function");
  }
  if(parameter_count != 0)
  {
    function->parameters = malloc(parameter_count * sizeof(*function->parameters));
    if(function->parameters == nullptr)
    {
      function_free(function);
      free(function);
      return xs_lil_set_error(error, XS_LIL_ALLOCATION_FAILED, "out of memory while copying XLIL parameters");
    }
    memcpy(function->parameters, parameters, parameter_count * sizeof(*function->parameters));
    for(size_t parameter = 0; parameter < parameter_count; ++parameter)
    {
      XsLilValueId value = 0;
      XsLilStatus status = xs_lil_add_value(function, parameters[parameter], &value, error);
      if(status != XS_LIL_OK)
      {
        function_free(function);
        free(function);
        return status;
      }
    }
  }
  XsLilStatus status = append_function(module, function, error);
  if(status != XS_LIL_OK)
  {
    function_free(function);
    free(function);
  }
  else if(out_function != nullptr)
    *out_function = module->functions[module->function_count - 1];
  return status;
}
XsLilStatus xs_lil_module_add_function(XsLilModule *module, const char *name, XsLilType return_type,
                                       const XsLilType *parameters, size_t parameter_count, XsLilError *error)
{
  return add_function(module, name, return_type, parameters, parameter_count, false, nullptr, error);
}

XsLilStatus xs_lil_module_add_function_definition(XsLilModule *module, const char *name, XsLilType return_type,
                                                  const XsLilType *parameters, size_t parameter_count,
                                                  XsLilFunction **function, XsLilError *error)
{
  return add_function(module, name, return_type, parameters, parameter_count, true, function, error);
}

size_t xs_lil_module_function_count(const XsLilModule *module)
{
  return module == nullptr ? 0 : module->function_count;
}

const XsLilFunction *xs_lil_module_function_at(const XsLilModule *module, size_t index)
{
  if(module == nullptr || index >= module->function_count)
    return nullptr;
  return module->functions[index];
}

const char *xs_lil_function_name(const XsLilFunction *function)
{
  return function == nullptr ? nullptr : function->name;
}

XsLilType xs_lil_function_return_type(const XsLilFunction *function)
{
  return function == nullptr ? (XsLilType){.kind = XS_LIL_TYPE_VOID} : function->return_type;
}

size_t xs_lil_function_parameter_count(const XsLilFunction *function)
{
  return function == nullptr ? 0 : function->parameter_count;
}

XsLilType xs_lil_function_parameter_type(const XsLilFunction *function, size_t index)
{
  if(function == nullptr || index >= function->parameter_count)
    return (XsLilType){.kind = XS_LIL_TYPE_VOID};
  return function->parameters[index];
}

bool xs_lil_function_is_definition(const XsLilFunction *function)
{
  return function != nullptr && function->is_definition;
}

size_t xs_lil_function_value_count(const XsLilFunction *function)
{
  return function == nullptr ? 0 : function->value_count;
}

XsLilType xs_lil_function_value_type(const XsLilFunction *function, XsLilValueId value)
{
  if(function == nullptr || (size_t)value >= function->value_count)
    return (XsLilType){.kind = XS_LIL_TYPE_VOID};
  return function->values[value].type;
}

size_t xs_lil_function_block_count(const XsLilFunction *function)
{
  return function == nullptr ? 0 : function->block_count;
}

const XsLilBlock *xs_lil_function_block_at(const XsLilFunction *function, size_t index)
{
  if(function == nullptr || index >= function->block_count)
    return nullptr;
  return function->blocks[index];
}

XsLilStatus xs_lil_function_append_block(XsLilFunction *function, const char *label, XsLilBlock **block,
                                         XsLilError *error)
{
  xs_lil_clear_error(error);
  if(block != nullptr)
    *block = nullptr;
  if(function == nullptr || !function->is_definition || label == nullptr || label[0] == '\0')
    return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT,
                            "valid XLIL function definition and block label are required");
  if(function->block_count == function->block_capacity)
  {
    size_t capacity = function->block_capacity == 0 ? 4 : function->block_capacity * 2;
    XsLilBlock **blocks = realloc(function->blocks, capacity * sizeof(*blocks));
    if(blocks == nullptr)
      return xs_lil_set_error(error, XS_LIL_ALLOCATION_FAILED, "out of memory while adding XLIL block");
    function->blocks = blocks;
    function->block_capacity = capacity;
  }
  XsLilBlock *created = calloc(1, sizeof(*created));
  if(created == nullptr)
    return xs_lil_set_error(error, XS_LIL_ALLOCATION_FAILED, "out of memory while adding XLIL block");
  *created = (XsLilBlock){.label = xs_lil_copy_text(label), .owner = function, .id = (uint32_t)function->block_count};
  if(created->label == nullptr)
  {
    free(created);
    return xs_lil_set_error(error, XS_LIL_ALLOCATION_FAILED, "out of memory while naming XLIL block");
  }
  function->blocks[function->block_count++] = created;
  if(block != nullptr)
    *block = created;
  return XS_LIL_OK;
}

XsLilStatus xs_lil_append_instruction(XsLilBlock *block, XsLilInstruction instruction, XsLilError *error)
{
  if(block == nullptr)
    return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "valid XLIL block is required");
  if(block->terminator.kind != XS_LIL_TERMINATOR_NONE)
    return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "cannot add XLIL instruction after terminator");
  if(block->instruction_count == block->instruction_capacity)
  {
    size_t capacity = block->instruction_capacity == 0 ? 8 : block->instruction_capacity * 2;
    XsLilInstruction *instructions = realloc(block->instructions, capacity * sizeof(*instructions));
    if(instructions == nullptr)
      return xs_lil_set_error(error, XS_LIL_ALLOCATION_FAILED, "out of memory while adding XLIL instruction");
    block->instructions = instructions;
    block->instruction_capacity = capacity;
  }
  block->instructions[block->instruction_count++] = instruction;
  return XS_LIL_OK;
}

XsLilStatus xs_lil_block_add_const_i64(XsLilBlock *block, int64_t value, XsLilValueId *result, XsLilError *error)
{
  xs_lil_clear_error(error);
  if(result != nullptr)
    *result = 0;
  if(block == nullptr || block->owner == nullptr)
    return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "valid XLIL block is required");
  XsLilValueId value_id = 0;
  XsLilStatus status = xs_lil_add_value(block->owner, (XsLilType){.kind = XS_LIL_TYPE_I64}, &value_id, error);
  if(status != XS_LIL_OK)
    return status;
  status = xs_lil_append_instruction(block,
                                     (XsLilInstruction){
                                         .kind = XS_LIL_INSTRUCTION_CONST_I64,
                                         .result = value_id,
                                         .immediate_i64 = value,
                                     },
                                     error);
  if(status != XS_LIL_OK)
    return status;
  if(result != nullptr)
    *result = value_id;
  return XS_LIL_OK;
}

XsLilStatus xs_lil_block_add_const_i32(XsLilBlock *block, int32_t value, XsLilValueId *result, XsLilError *error)
{
  xs_lil_clear_error(error);
  if(result != nullptr)
    *result = 0;
  if(block == nullptr || block->owner == nullptr)
    return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "valid XLIL block is required");
  XsLilValueId value_id = 0;
  XsLilStatus status = xs_lil_add_value(block->owner, (XsLilType){.kind = XS_LIL_TYPE_I32}, &value_id, error);
  if(status != XS_LIL_OK)
    return status;
  status = xs_lil_append_instruction(block,
                                     (XsLilInstruction){
                                         .kind = XS_LIL_INSTRUCTION_CONST_I32,
                                         .result = value_id,
                                         .immediate_i64 = value,
                                     },
                                     error);
  if(status != XS_LIL_OK)
    return status;
  if(result != nullptr)
    *result = value_id;
  return XS_LIL_OK;
}

XsLilStatus xs_lil_block_add_const_bool(XsLilBlock *block, bool value, XsLilValueId *result, XsLilError *error)
{
  xs_lil_clear_error(error);
  if(result != nullptr)
    *result = 0;
  if(block == nullptr || block->owner == nullptr)
    return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "valid XLIL block is required");
  XsLilValueId value_id = 0;
  XsLilStatus status = xs_lil_add_value(block->owner, (XsLilType){.kind = XS_LIL_TYPE_BOOL}, &value_id, error);
  if(status != XS_LIL_OK)
    return status;
  status = xs_lil_append_instruction(block,
                                     (XsLilInstruction){
                                         .kind = XS_LIL_INSTRUCTION_CONST_BOOL,
                                         .result = value_id,
                                         .immediate_bool = value,
                                     },
                                     error);
  if(status != XS_LIL_OK)
    return status;
  if(result != nullptr)
    *result = value_id;
  return XS_LIL_OK;
}

static XsLilStatus add_const_float_bits(XsLilBlock *block, XsLilInstructionKind kind, XsLilTypeKind type, uint64_t bits,
                                        XsLilValueId *result, XsLilError *error)
{
  xs_lil_clear_error(error);
  if(result != nullptr)
    *result = 0;
  if(block == nullptr || block->owner == nullptr)
    return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "valid XLIL block is required");
  XsLilValueId value_id = 0;
  XsLilStatus status = xs_lil_add_value(block->owner, (XsLilType){.kind = type}, &value_id, error);
  if(status == XS_LIL_OK)
    status = xs_lil_append_instruction(
        block, (XsLilInstruction){.kind = kind, .result = value_id, .immediate_float_bits = bits}, error);
  if(status == XS_LIL_OK && result != nullptr)
    *result = value_id;
  return status;
}

XsLilStatus xs_lil_block_add_const_f32_bits(XsLilBlock *block, uint32_t bits, XsLilValueId *result, XsLilError *error)
{
  return add_const_float_bits(block, XS_LIL_INSTRUCTION_CONST_F32, XS_LIL_TYPE_F32, bits, result, error);
}

XsLilStatus xs_lil_block_add_const_f64_bits(XsLilBlock *block, uint64_t bits, XsLilValueId *result, XsLilError *error)
{
  return add_const_float_bits(block, XS_LIL_INSTRUCTION_CONST_F64, XS_LIL_TYPE_F64, bits, result, error);
}

static XsLilStatus add_binary_integer(XsLilBlock *block, XsLilInstructionKind kind, XsLilValueId left,
                                      XsLilValueId right, XsLilTypeKind operand_type, XsLilType result_type,
                                      XsLilValueId *result, XsLilError *error)
{
  xs_lil_clear_error(error);
  if(result != nullptr)
    *result = UINT32_MAX;
  if(block == nullptr || block->owner == nullptr || (size_t)left >= block->owner->value_count ||
     (size_t)right >= block->owner->value_count || block->owner->values[left].type.kind != operand_type ||
     block->owner->values[right].type.kind != operand_type)
    return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL binary integer instruction requires matching values");
  XsLilInstruction instruction = {
      .kind = kind,
      .left = left,
      .right = right,
  };
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

XsLilStatus xs_lil_block_add_i64(XsLilBlock *block, XsLilValueId left, XsLilValueId right, XsLilValueId *result,
                                 XsLilError *error)
{
  return add_binary_integer(block, XS_LIL_INSTRUCTION_ADD_I64, left, right, XS_LIL_TYPE_I64,
                            (XsLilType){.kind = XS_LIL_TYPE_I64}, result, error);
}

XsLilStatus xs_lil_block_sub_i64(XsLilBlock *block, XsLilValueId left, XsLilValueId right, XsLilValueId *result,
                                 XsLilError *error)
{
  return add_binary_integer(block, XS_LIL_INSTRUCTION_SUB_I64, left, right, XS_LIL_TYPE_I64,
                            (XsLilType){.kind = XS_LIL_TYPE_I64}, result, error);
}

XsLilStatus xs_lil_block_mul_i64(XsLilBlock *block, XsLilValueId left, XsLilValueId right, XsLilValueId *result,
                                 XsLilError *error)
{
  return add_binary_integer(block, XS_LIL_INSTRUCTION_MUL_I64, left, right, XS_LIL_TYPE_I64,
                            (XsLilType){.kind = XS_LIL_TYPE_I64}, result, error);
}

XsLilStatus xs_lil_block_div_i64(XsLilBlock *block, XsLilValueId left, XsLilValueId right, XsLilValueId *result,
                                 XsLilError *error)
{
  return add_binary_integer(block, XS_LIL_INSTRUCTION_DIV_I64, left, right, XS_LIL_TYPE_I64,
                            (XsLilType){.kind = XS_LIL_TYPE_I64}, result, error);
}

XsLilStatus xs_lil_block_rem_i64(XsLilBlock *block, XsLilValueId left, XsLilValueId right, XsLilValueId *result,
                                 XsLilError *error)
{
  return add_binary_integer(block, XS_LIL_INSTRUCTION_REM_I64, left, right, XS_LIL_TYPE_I64,
                            (XsLilType){.kind = XS_LIL_TYPE_I64}, result, error);
}

XsLilStatus xs_lil_block_and_i64(XsLilBlock *block, XsLilValueId left, XsLilValueId right, XsLilValueId *result,
                                 XsLilError *error)
{
  return add_binary_integer(block, XS_LIL_INSTRUCTION_AND_I64, left, right, XS_LIL_TYPE_I64,
                            (XsLilType){.kind = XS_LIL_TYPE_I64}, result, error);
}

XsLilStatus xs_lil_block_or_i64(XsLilBlock *block, XsLilValueId left, XsLilValueId right, XsLilValueId *result,
                                XsLilError *error)
{
  return add_binary_integer(block, XS_LIL_INSTRUCTION_OR_I64, left, right, XS_LIL_TYPE_I64,
                            (XsLilType){.kind = XS_LIL_TYPE_I64}, result, error);
}

XsLilStatus xs_lil_block_xor_i64(XsLilBlock *block, XsLilValueId left, XsLilValueId right, XsLilValueId *result,
                                 XsLilError *error)
{
  return add_binary_integer(block, XS_LIL_INSTRUCTION_XOR_I64, left, right, XS_LIL_TYPE_I64,
                            (XsLilType){.kind = XS_LIL_TYPE_I64}, result, error);
}

XsLilStatus xs_lil_block_shl_i64(XsLilBlock *block, XsLilValueId left, XsLilValueId right, XsLilValueId *result,
                                 XsLilError *error)
{
  return add_binary_integer(block, XS_LIL_INSTRUCTION_SHL_I64, left, right, XS_LIL_TYPE_I64,
                            (XsLilType){.kind = XS_LIL_TYPE_I64}, result, error);
}

XsLilStatus xs_lil_block_shr_i64(XsLilBlock *block, XsLilValueId left, XsLilValueId right, XsLilValueId *result,
                                 XsLilError *error)
{
  return add_binary_integer(block, XS_LIL_INSTRUCTION_SHR_I64, left, right, XS_LIL_TYPE_I64,
                            (XsLilType){.kind = XS_LIL_TYPE_I64}, result, error);
}

XsLilStatus xs_lil_block_eq_i64(XsLilBlock *block, XsLilValueId left, XsLilValueId right, XsLilValueId *result,
                                XsLilError *error)
{
  return add_binary_integer(block, XS_LIL_INSTRUCTION_EQ_I64, left, right, XS_LIL_TYPE_I64,
                            (XsLilType){.kind = XS_LIL_TYPE_BOOL}, result, error);
}

XsLilStatus xs_lil_block_ne_i64(XsLilBlock *block, XsLilValueId left, XsLilValueId right, XsLilValueId *result,
                                XsLilError *error)
{
  return add_binary_integer(block, XS_LIL_INSTRUCTION_NE_I64, left, right, XS_LIL_TYPE_I64,
                            (XsLilType){.kind = XS_LIL_TYPE_BOOL}, result, error);
}

XsLilStatus xs_lil_block_lt_i64(XsLilBlock *block, XsLilValueId left, XsLilValueId right, XsLilValueId *result,
                                XsLilError *error)
{
  return add_binary_integer(block, XS_LIL_INSTRUCTION_LT_I64, left, right, XS_LIL_TYPE_I64,
                            (XsLilType){.kind = XS_LIL_TYPE_BOOL}, result, error);
}

XsLilStatus xs_lil_block_le_i64(XsLilBlock *block, XsLilValueId left, XsLilValueId right, XsLilValueId *result,
                                XsLilError *error)
{
  return add_binary_integer(block, XS_LIL_INSTRUCTION_LE_I64, left, right, XS_LIL_TYPE_I64,
                            (XsLilType){.kind = XS_LIL_TYPE_BOOL}, result, error);
}

XsLilStatus xs_lil_block_gt_i64(XsLilBlock *block, XsLilValueId left, XsLilValueId right, XsLilValueId *result,
                                XsLilError *error)
{
  return add_binary_integer(block, XS_LIL_INSTRUCTION_GT_I64, left, right, XS_LIL_TYPE_I64,
                            (XsLilType){.kind = XS_LIL_TYPE_BOOL}, result, error);
}

XsLilStatus xs_lil_block_ge_i64(XsLilBlock *block, XsLilValueId left, XsLilValueId right, XsLilValueId *result,
                                XsLilError *error)
{
  return add_binary_integer(block, XS_LIL_INSTRUCTION_GE_I64, left, right, XS_LIL_TYPE_I64,
                            (XsLilType){.kind = XS_LIL_TYPE_BOOL}, result, error);
}

XsLilStatus xs_lil_block_add_i32(XsLilBlock *block, XsLilValueId left, XsLilValueId right, XsLilValueId *result,
                                 XsLilError *error)
{
  return add_binary_integer(block, XS_LIL_INSTRUCTION_ADD_I32, left, right, XS_LIL_TYPE_I32,
                            (XsLilType){.kind = XS_LIL_TYPE_I32}, result, error);
}

XsLilStatus xs_lil_block_sub_i32(XsLilBlock *block, XsLilValueId left, XsLilValueId right, XsLilValueId *result,
                                 XsLilError *error)
{
  return add_binary_integer(block, XS_LIL_INSTRUCTION_SUB_I32, left, right, XS_LIL_TYPE_I32,
                            (XsLilType){.kind = XS_LIL_TYPE_I32}, result, error);
}

XsLilStatus xs_lil_block_mul_i32(XsLilBlock *block, XsLilValueId left, XsLilValueId right, XsLilValueId *result,
                                 XsLilError *error)
{
  return add_binary_integer(block, XS_LIL_INSTRUCTION_MUL_I32, left, right, XS_LIL_TYPE_I32,
                            (XsLilType){.kind = XS_LIL_TYPE_I32}, result, error);
}

XsLilStatus xs_lil_block_div_i32(XsLilBlock *block, XsLilValueId left, XsLilValueId right, XsLilValueId *result,
                                 XsLilError *error)
{
  return add_binary_integer(block, XS_LIL_INSTRUCTION_DIV_I32, left, right, XS_LIL_TYPE_I32,
                            (XsLilType){.kind = XS_LIL_TYPE_I32}, result, error);
}

XsLilStatus xs_lil_block_rem_i32(XsLilBlock *block, XsLilValueId left, XsLilValueId right, XsLilValueId *result,
                                 XsLilError *error)
{
  return add_binary_integer(block, XS_LIL_INSTRUCTION_REM_I32, left, right, XS_LIL_TYPE_I32,
                            (XsLilType){.kind = XS_LIL_TYPE_I32}, result, error);
}

XsLilStatus xs_lil_block_and_i32(XsLilBlock *block, XsLilValueId left, XsLilValueId right, XsLilValueId *result,
                                 XsLilError *error)
{
  return add_binary_integer(block, XS_LIL_INSTRUCTION_AND_I32, left, right, XS_LIL_TYPE_I32,
                            (XsLilType){.kind = XS_LIL_TYPE_I32}, result, error);
}

XsLilStatus xs_lil_block_or_i32(XsLilBlock *block, XsLilValueId left, XsLilValueId right, XsLilValueId *result,
                                XsLilError *error)
{
  return add_binary_integer(block, XS_LIL_INSTRUCTION_OR_I32, left, right, XS_LIL_TYPE_I32,
                            (XsLilType){.kind = XS_LIL_TYPE_I32}, result, error);
}

XsLilStatus xs_lil_block_xor_i32(XsLilBlock *block, XsLilValueId left, XsLilValueId right, XsLilValueId *result,
                                 XsLilError *error)
{
  return add_binary_integer(block, XS_LIL_INSTRUCTION_XOR_I32, left, right, XS_LIL_TYPE_I32,
                            (XsLilType){.kind = XS_LIL_TYPE_I32}, result, error);
}

XsLilStatus xs_lil_block_shl_i32(XsLilBlock *block, XsLilValueId left, XsLilValueId right, XsLilValueId *result,
                                 XsLilError *error)
{
  return add_binary_integer(block, XS_LIL_INSTRUCTION_SHL_I32, left, right, XS_LIL_TYPE_I32,
                            (XsLilType){.kind = XS_LIL_TYPE_I32}, result, error);
}

XsLilStatus xs_lil_block_shr_i32(XsLilBlock *block, XsLilValueId left, XsLilValueId right, XsLilValueId *result,
                                 XsLilError *error)
{
  return add_binary_integer(block, XS_LIL_INSTRUCTION_SHR_I32, left, right, XS_LIL_TYPE_I32,
                            (XsLilType){.kind = XS_LIL_TYPE_I32}, result, error);
}

XsLilStatus xs_lil_block_eq_i32(XsLilBlock *block, XsLilValueId left, XsLilValueId right, XsLilValueId *result,
                                XsLilError *error)
{
  return add_binary_integer(block, XS_LIL_INSTRUCTION_EQ_I32, left, right, XS_LIL_TYPE_I32,
                            (XsLilType){.kind = XS_LIL_TYPE_BOOL}, result, error);
}

XsLilStatus xs_lil_block_ne_i32(XsLilBlock *block, XsLilValueId left, XsLilValueId right, XsLilValueId *result,
                                XsLilError *error)
{
  return add_binary_integer(block, XS_LIL_INSTRUCTION_NE_I32, left, right, XS_LIL_TYPE_I32,
                            (XsLilType){.kind = XS_LIL_TYPE_BOOL}, result, error);
}

XsLilStatus xs_lil_block_lt_i32(XsLilBlock *block, XsLilValueId left, XsLilValueId right, XsLilValueId *result,
                                XsLilError *error)
{
  return add_binary_integer(block, XS_LIL_INSTRUCTION_LT_I32, left, right, XS_LIL_TYPE_I32,
                            (XsLilType){.kind = XS_LIL_TYPE_BOOL}, result, error);
}

XsLilStatus xs_lil_block_le_i32(XsLilBlock *block, XsLilValueId left, XsLilValueId right, XsLilValueId *result,
                                XsLilError *error)
{
  return add_binary_integer(block, XS_LIL_INSTRUCTION_LE_I32, left, right, XS_LIL_TYPE_I32,
                            (XsLilType){.kind = XS_LIL_TYPE_BOOL}, result, error);
}

XsLilStatus xs_lil_block_gt_i32(XsLilBlock *block, XsLilValueId left, XsLilValueId right, XsLilValueId *result,
                                XsLilError *error)
{
  return add_binary_integer(block, XS_LIL_INSTRUCTION_GT_I32, left, right, XS_LIL_TYPE_I32,
                            (XsLilType){.kind = XS_LIL_TYPE_BOOL}, result, error);
}

XsLilStatus xs_lil_block_ge_i32(XsLilBlock *block, XsLilValueId left, XsLilValueId right, XsLilValueId *result,
                                XsLilError *error)
{
  return add_binary_integer(block, XS_LIL_INSTRUCTION_GE_I32, left, right, XS_LIL_TYPE_I32,
                            (XsLilType){.kind = XS_LIL_TYPE_BOOL}, result, error);
}

XsLilStatus xs_lil_block_not_bool(XsLilBlock *block, XsLilValueId operand, XsLilValueId *result, XsLilError *error)
{
  xs_lil_clear_error(error);
  if(result != nullptr)
    *result = UINT32_MAX;
  if(block == nullptr || block->owner == nullptr || (size_t)operand >= block->owner->value_count ||
     block->owner->values[operand].type.kind != XS_LIL_TYPE_BOOL)
    return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL not.bool operand must be bool");
  XsLilValueId value_id = 0;
  XsLilStatus status = xs_lil_add_value(block->owner, (XsLilType){.kind = XS_LIL_TYPE_BOOL}, &value_id, error);
  if(status != XS_LIL_OK)
    return status;
  status = xs_lil_append_instruction(block,
                                     (XsLilInstruction){
                                         .kind = XS_LIL_INSTRUCTION_NOT_BOOL,
                                         .result = value_id,
                                         .left = operand,
                                     },
                                     error);
  if(status != XS_LIL_OK)
    return status;
  if(result != nullptr)
    *result = value_id;
  return XS_LIL_OK;
}

static XsLilStatus add_call(XsLilBlock *block, const char *callee, XsLilType return_type, const XsLilValueId *arguments,
                            size_t argument_count, bool has_result, XsLilValueId *result, XsLilError *error)
{
  xs_lil_clear_error(error);
  if(result != nullptr)
    *result = UINT32_MAX;
  if(block == nullptr || block->owner == nullptr || callee == nullptr || callee[0] == '\0' ||
     (argument_count != 0 && arguments == nullptr))
    return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "valid XLIL call instruction is required");
  if((has_result && return_type.kind == XS_LIL_TYPE_VOID) || (!has_result && return_type.kind != XS_LIL_TYPE_VOID))
    return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL call result does not match return type");
  for(size_t argument = 0; argument < argument_count; ++argument)
  {
    if((size_t)arguments[argument] >= block->owner->value_count)
      return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL call references an unknown argument value");
  }
  XsLilInstruction instruction = {
      .kind = XS_LIL_INSTRUCTION_CALL,
      .result = UINT32_MAX,
      .callee = xs_lil_copy_text(callee),
      .argument_count = argument_count,
  };
  if(instruction.callee == nullptr)
    return xs_lil_set_error(error, XS_LIL_ALLOCATION_FAILED, "out of memory while naming XLIL call target");
  if(argument_count != 0)
  {
    instruction.arguments = malloc(argument_count * sizeof(*instruction.arguments));
    if(instruction.arguments == nullptr)
    {
      free(instruction.callee);
      return xs_lil_set_error(error, XS_LIL_ALLOCATION_FAILED, "out of memory while copying XLIL call arguments");
    }
    memcpy(instruction.arguments, arguments, argument_count * sizeof(*instruction.arguments));
  }
  if(has_result)
  {
    XsLilStatus status = xs_lil_add_value(block->owner, return_type, &instruction.result, error);
    if(status != XS_LIL_OK)
    {
      free(instruction.arguments);
      free(instruction.callee);
      return status;
    }
  }
  XsLilStatus status = xs_lil_append_instruction(block, instruction, error);
  if(status != XS_LIL_OK)
  {
    if(has_result)
      --block->owner->value_count;
    free(instruction.arguments);
    free(instruction.callee);
    return status;
  }
  if(result != nullptr)
    *result = instruction.result;
  return XS_LIL_OK;
}

XsLilStatus xs_lil_block_add_call(XsLilBlock *block, const char *callee, XsLilType return_type,
                                  const XsLilValueId *arguments, size_t argument_count, XsLilValueId *result,
                                  XsLilError *error)
{
  return add_call(block, callee, return_type, arguments, argument_count, true, result, error);
}

XsLilStatus xs_lil_block_add_void_call(XsLilBlock *block, const char *callee, const XsLilValueId *arguments,
                                       size_t argument_count, XsLilError *error)
{
  return add_call(block, callee, (XsLilType){.kind = XS_LIL_TYPE_VOID}, arguments, argument_count, false, nullptr,
                  error);
}

static XsLilStatus set_terminator(XsLilBlock *block, XsLilTerminator terminator, XsLilError *error)
{
  xs_lil_clear_error(error);
  if(block == nullptr)
    return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "valid XLIL block is required");
  if(block->terminator.kind != XS_LIL_TERMINATOR_NONE)
    return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL block already has a terminator");
  if(terminator.kind == XS_LIL_TERMINATOR_RETURN && terminator.has_value)
  {
    if((size_t)terminator.value >= block->owner->value_count)
      return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL return references an unknown value");
    if(!xs_lil_type_equal(block->owner->values[terminator.value].type, block->owner->return_type))
      return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT,
                              "XLIL return value type does not match function return type");
  }
  else if(terminator.kind == XS_LIL_TERMINATOR_RETURN && block->owner->return_type.kind != XS_LIL_TYPE_VOID)
  {
    return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL return is missing a value");
  }
  else if(terminator.kind == XS_LIL_TERMINATOR_BRANCH)
  {
    if((size_t)terminator.target >= block->owner->block_count ||
       block->owner->blocks[terminator.target]->id != terminator.target)
      return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL branch target is unknown");
  }
  else if(terminator.kind == XS_LIL_TERMINATOR_BRANCH_IF)
  {
    if((size_t)terminator.condition >= block->owner->value_count)
      return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL branch_if condition is unknown");
    if(block->owner->values[terminator.condition].type.kind != XS_LIL_TYPE_BOOL)
      return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL branch_if condition must be bool");
    if((size_t)terminator.target >= block->owner->block_count ||
       block->owner->blocks[terminator.target]->id != terminator.target ||
       (size_t)terminator.else_target >= block->owner->block_count ||
       block->owner->blocks[terminator.else_target]->id != terminator.else_target)
      return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL branch_if target is unknown");
  }
  block->terminator = terminator;
  return XS_LIL_OK;
}

XsLilStatus xs_lil_block_set_return(XsLilBlock *block, XsLilError *error)
{
  return set_terminator(block, (XsLilTerminator){.kind = XS_LIL_TERMINATOR_RETURN}, error);
}

XsLilStatus xs_lil_block_set_return_value(XsLilBlock *block, XsLilValueId value, XsLilError *error)
{
  return set_terminator(block, (XsLilTerminator){.kind = XS_LIL_TERMINATOR_RETURN, .has_value = true, .value = value},
                        error);
}

XsLilStatus xs_lil_block_set_branch(XsLilBlock *block, XsLilBlockId target, XsLilError *error)
{
  return set_terminator(block, (XsLilTerminator){.kind = XS_LIL_TERMINATOR_BRANCH, .target = target}, error);
}

XsLilStatus xs_lil_block_set_branch_if(XsLilBlock *block, XsLilValueId condition, XsLilBlockId then_block,
                                       XsLilBlockId else_block, XsLilError *error)
{
  return set_terminator(
      block,
      (XsLilTerminator){
          .kind = XS_LIL_TERMINATOR_BRANCH_IF, .condition = condition, .target = then_block, .else_target = else_block},
      error);
}

XsLilStatus xs_lil_block_set_panic(XsLilBlock *block, XsLilError *error)
{
  return set_terminator(block, (XsLilTerminator){.kind = XS_LIL_TERMINATOR_PANIC}, error);
}

XsLilBlockId xs_lil_block_id(const XsLilBlock *block)
{
  return block == nullptr ? UINT32_MAX : block->id;
}

const char *xs_lil_block_label(const XsLilBlock *block)
{
  return block == nullptr ? nullptr : block->label;
}

size_t xs_lil_block_instruction_count(const XsLilBlock *block)
{
  return block == nullptr ? 0 : block->instruction_count;
}

XsLilInstructionKind xs_lil_block_instruction_kind(const XsLilBlock *block, size_t index)
{
  if(block == nullptr || index >= block->instruction_count)
    return XS_LIL_INSTRUCTION_CONST_I64;
  return block->instructions[index].kind;
}

XsLilValueId xs_lil_block_instruction_result(const XsLilBlock *block, size_t index)
{
  if(block == nullptr || index >= block->instruction_count)
    return UINT32_MAX;
  return block->instructions[index].result;
}

int64_t xs_lil_block_instruction_i64(const XsLilBlock *block, size_t index)
{
  if(block == nullptr || index >= block->instruction_count)
    return 0;
  return block->instructions[index].immediate_i64;
}

uint64_t xs_lil_block_instruction_float_bits(const XsLilBlock *block, size_t index)
{
  if(block == nullptr || index >= block->instruction_count)
    return 0;
  return block->instructions[index].immediate_float_bits;
}

bool xs_lil_block_instruction_bool(const XsLilBlock *block, size_t index)
{
  if(block == nullptr || index >= block->instruction_count)
    return false;
  return block->instructions[index].immediate_bool;
}

const char *xs_lil_block_instruction_callee(const XsLilBlock *block, size_t index)
{
  if(block == nullptr || index >= block->instruction_count ||
     block->instructions[index].kind != XS_LIL_INSTRUCTION_CALL)
    return nullptr;
  return block->instructions[index].callee;
}

size_t xs_lil_block_instruction_argument_count(const XsLilBlock *block, size_t index)
{
  if(block == nullptr || index >= block->instruction_count ||
     (block->instructions[index].kind != XS_LIL_INSTRUCTION_CALL &&
      block->instructions[index].kind != XS_LIL_INSTRUCTION_AGGREGATE &&
      block->instructions[index].kind != XS_LIL_INSTRUCTION_ARRAY_SET))
    return 0;
  return block->instructions[index].argument_count;
}

XsLilValueId xs_lil_block_instruction_argument(const XsLilBlock *block, size_t index, size_t argument)
{
  if(block == nullptr || index >= block->instruction_count || argument >= block->instructions[index].argument_count ||
     (block->instructions[index].kind != XS_LIL_INSTRUCTION_CALL &&
      block->instructions[index].kind != XS_LIL_INSTRUCTION_AGGREGATE &&
      block->instructions[index].kind != XS_LIL_INSTRUCTION_ARRAY_SET))
    return UINT32_MAX;
  return block->instructions[index].arguments[argument];
}

XsLilValueId xs_lil_block_instruction_left(const XsLilBlock *block, size_t index)
{
  if(block == nullptr || index >= block->instruction_count)
    return UINT32_MAX;
  return block->instructions[index].left;
}

XsLilValueId xs_lil_block_instruction_right(const XsLilBlock *block, size_t index)
{
  if(block == nullptr || index >= block->instruction_count)
    return UINT32_MAX;
  return block->instructions[index].right;
}

XsLilTerminatorKind xs_lil_block_terminator_kind(const XsLilBlock *block)
{
  return block == nullptr ? XS_LIL_TERMINATOR_NONE : block->terminator.kind;
}

bool xs_lil_block_terminator_has_value(const XsLilBlock *block)
{
  return block != nullptr && block->terminator.has_value;
}

XsLilValueId xs_lil_block_terminator_value(const XsLilBlock *block)
{
  return block == nullptr ? UINT32_MAX : block->terminator.value;
}

XsLilBlockId xs_lil_block_terminator_target(const XsLilBlock *block)
{
  return block == nullptr ? UINT32_MAX : block->terminator.target;
}

XsLilValueId xs_lil_block_terminator_condition(const XsLilBlock *block)
{
  return block == nullptr ? UINT32_MAX : block->terminator.condition;
}

XsLilBlockId xs_lil_block_terminator_then_block(const XsLilBlock *block)
{
  return block == nullptr ? UINT32_MAX : block->terminator.target;
}

XsLilBlockId xs_lil_block_terminator_else_block(const XsLilBlock *block)
{
  return block == nullptr ? UINT32_MAX : block->terminator.else_target;
}

const char *xs_lil_type_name(XsLilType type)
{
  static const char *const names[] = {
      "void", "bool", "u8",  "i8",  "u16", "i16",  "u32", "i32",       "u64",   "i64",
      "u128", "i128", "f16", "f32", "f64", "f128", "str", "aggregate", "array",
  };
  if((size_t)type.kind >= sizeof(names) / sizeof(names[0]))
    return "unknown";
  return names[type.kind];
}
