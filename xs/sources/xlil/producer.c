/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "model_internal.h"

#include <stdlib.h>
#include <string.h>

const char *xs_lil_status_name(XsLilStatus status)
{
  switch(status)
  {
  case XS_LIL_OK:
    return "ok";
  case XS_LIL_INVALID_ARGUMENT:
    return "invalid_argument";
  case XS_LIL_ALLOCATION_FAILED:
    return "allocation_failed";
  case XS_LIL_IO_ERROR:
    return "io_error";
  }
  return "unknown";
}

void xs_lil_error_reset(XsLilError *error)
{
  xs_lil_clear_error(error);
}

XsLilError *xs_lil_error_create(void)
{
  return calloc(1, sizeof(XsLilError));
}

void xs_lil_error_destroy(XsLilError *error)
{
  free(error);
}

XsLilStatus xs_lil_error_status(const XsLilError *error)
{
  return error == nullptr ? XS_LIL_OK : error->status;
}

const char *xs_lil_error_message(const XsLilError *error)
{
  return error == nullptr ? "" : error->message;
}

XsLilType xs_lil_scalar_type(XsLilTypeKind kind)
{
  if(kind > XS_LIL_TYPE_STRING)
    return (XsLilType){.kind = XS_LIL_TYPE_VOID};
  return (XsLilType){.kind = kind};
}

bool xs_lil_type_is_scalar(XsLilType type)
{
  return type.kind <= XS_LIL_TYPE_STRING && type.registry_id == 0;
}

XsLilTypeKind xs_lil_type_kind(XsLilType type)
{
  return type.kind;
}

uint32_t xs_lil_type_registry_id(XsLilType type)
{
  return type.registry_id;
}

uint32_t xs_lil_module_text_version(const XsLilModule *module)
{
  return module == nullptr ? UINT32_MAX : XS_LIL_TEXT_VERSION;
}

bool xs_lil_module_type_is_valid(const XsLilModule *module, XsLilType type)
{
  if(module == nullptr || (unsigned)type.kind > (unsigned)XS_LIL_TYPE_ARRAY)
    return false;
  if(type.kind == XS_LIL_TYPE_AGGREGATE)
    return type.registry_id < module->aggregate_type_count;
  if(type.kind == XS_LIL_TYPE_ARRAY)
    return type.registry_id < module->array_type_count;
  return type.registry_id == 0;
}

XsLilFunction *xs_lil_module_function_at_mut(XsLilModule *module, size_t index)
{
  if(module == nullptr || index >= module->function_count)
    return nullptr;
  return module->functions[index];
}

const XsLilFunction *xs_lil_module_function_named(const XsLilModule *module, const char *name)
{
  if(module == nullptr || name == nullptr)
    return nullptr;
  for(size_t index = 0; index < module->function_count; ++index)
    if(strcmp(module->functions[index]->name, name) == 0)
      return module->functions[index];
  return nullptr;
}

XsLilFunction *xs_lil_module_function_named_mut(XsLilModule *module, const char *name)
{
  if(module == nullptr || name == nullptr)
    return nullptr;
  for(size_t index = 0; index < module->function_count; ++index)
    if(strcmp(module->functions[index]->name, name) == 0)
      return module->functions[index];
  return nullptr;
}

XsLilValueId xs_lil_function_parameter_value(const XsLilFunction *function, size_t index)
{
  return function == nullptr || index >= function->parameter_count ? XS_LIL_INVALID_VALUE_ID : (XsLilValueId)index;
}

XsLilBlock *xs_lil_function_block_at_mut(XsLilFunction *function, size_t index)
{
  if(function == nullptr || index >= function->block_count)
    return nullptr;
  return function->blocks[index];
}

const XsLilBlock *xs_lil_function_block_named(const XsLilFunction *function, const char *label)
{
  if(function == nullptr || label == nullptr)
    return nullptr;
  for(size_t index = 0; index < function->block_count; ++index)
    if(strcmp(function->blocks[index]->label, label) == 0)
      return function->blocks[index];
  return nullptr;
}

XsLilBlock *xs_lil_function_block_named_mut(XsLilFunction *function, const char *label)
{
  if(function == nullptr || label == nullptr)
    return nullptr;
  for(size_t index = 0; index < function->block_count; ++index)
    if(strcmp(function->blocks[index]->label, label) == 0)
      return function->blocks[index];
  return nullptr;
}

bool xs_lil_block_instruction_has_result(const XsLilBlock *block, size_t index)
{
  if(block == nullptr || index >= block->instruction_count)
    return false;
  const XsLilInstruction *instruction = &block->instructions[index];
  if(instruction->kind == XS_LIL_INSTRUCTION_STORE)
    return false;
  return instruction->kind != XS_LIL_INSTRUCTION_CALL || instruction->result != XS_LIL_INVALID_VALUE_ID;
}

XsLilType xs_lil_block_instruction_result_type(const XsLilBlock *block, size_t index)
{
  if(!xs_lil_block_instruction_has_result(block, index))
    return xs_lil_scalar_type(XS_LIL_TYPE_VOID);
  XsLilValueId result = block->instructions[index].result;
  if(block->owner == nullptr || (size_t)result >= block->owner->value_count)
    return xs_lil_scalar_type(XS_LIL_TYPE_VOID);
  return block->owner->values[result].type;
}

uint32_t xs_lil_block_instruction_field(const XsLilBlock *block, size_t index)
{
  if(block == nullptr || index >= block->instruction_count ||
     block->instructions[index].kind != XS_LIL_INSTRUCTION_EXTRACT)
    return UINT32_MAX;
  return (uint32_t)block->instructions[index].immediate_i64;
}
