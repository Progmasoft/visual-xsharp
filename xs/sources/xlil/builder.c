/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "model_internal.h"

#include <stdlib.h>

struct XsLilBuilder
{
  XsLilModule *module;
  XsLilBlock *block;
};

static XsLilStatus require_block(XsLilBuilder *builder, XsLilBlock **block, XsLilError *error)
{
  xs_lil_clear_error(error);
  if(builder == nullptr || builder->module == nullptr || builder->block == nullptr)
    return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL builder has no insertion block");
  *block = builder->block;
  return XS_LIL_OK;
}

static bool same_function(const XsLilBlock *left, const XsLilBlock *right)
{
  return left != nullptr && right != nullptr && left->owner == right->owner;
}

uint32_t xs_lil_c_api_version(void)
{
  return XS_LIL_C_API_VERSION;
}

XsLilStatus xs_lil_builder_create(XsLilModule *module, XsLilBuilder **builder, XsLilError *error)
{
  xs_lil_clear_error(error);
  if(builder != nullptr)
    *builder = nullptr;
  if(module == nullptr || builder == nullptr)
    return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL module and builder output are required");
  *builder = calloc(1, sizeof(**builder));
  if(*builder == nullptr)
    return xs_lil_set_error(error, XS_LIL_ALLOCATION_FAILED, "out of memory while creating XLIL builder");
  (*builder)->module = module;
  return XS_LIL_OK;
}

void xs_lil_builder_destroy(XsLilBuilder *builder)
{
  free(builder);
}

XsLilModule *xs_lil_builder_module(XsLilBuilder *builder)
{
  return builder == nullptr ? nullptr : builder->module;
}

XsLilBlock *xs_lil_builder_block(XsLilBuilder *builder)
{
  return builder == nullptr ? nullptr : builder->block;
}

XsLilStatus xs_lil_builder_position_at_end(XsLilBuilder *builder, XsLilBlock *block, XsLilError *error)
{
  xs_lil_clear_error(error);
  if(builder == nullptr || block == nullptr || block->owner == nullptr || block->owner->owner != builder->module)
    return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL block does not belong to the builder module");
  builder->block = block;
  return XS_LIL_OK;
}

XsLilStatus xs_lil_builder_append_block(XsLilBuilder *builder, XsLilFunction *function, const char *label,
                                        XsLilBlock **block, XsLilError *error)
{
  xs_lil_clear_error(error);
  if(builder == nullptr || function == nullptr || function->owner != builder->module)
    return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL function does not belong to the builder module");
  XsLilBlock *created = nullptr;
  XsLilStatus status = xs_lil_function_append_block(function, label, &created, error);
  if(status != XS_LIL_OK)
    return status;
  builder->block = created;
  if(block != nullptr)
    *block = created;
  return XS_LIL_OK;
}

XsLilStatus xs_lil_builder_const_i32(XsLilBuilder *builder, int32_t value, XsLilValueId *result, XsLilError *error)
{
  XsLilBlock *block = nullptr;
  XsLilStatus status = require_block(builder, &block, error);
  return status == XS_LIL_OK ? xs_lil_block_add_const_i32(block, value, result, error) : status;
}

XsLilStatus xs_lil_builder_const_i64(XsLilBuilder *builder, int64_t value, XsLilValueId *result, XsLilError *error)
{
  XsLilBlock *block = nullptr;
  XsLilStatus status = require_block(builder, &block, error);
  return status == XS_LIL_OK ? xs_lil_block_add_const_i64(block, value, result, error) : status;
}

XsLilStatus xs_lil_builder_const_bool(XsLilBuilder *builder, bool value, XsLilValueId *result, XsLilError *error)
{
  XsLilBlock *block = nullptr;
  XsLilStatus status = require_block(builder, &block, error);
  return status == XS_LIL_OK ? xs_lil_block_add_const_bool(block, value, result, error) : status;
}

XsLilStatus xs_lil_builder_const_str(XsLilBuilder *builder, XsLilUtf32Encoding encoding, const uint32_t *units,
                                     size_t unit_count, XsLilValueId *result, XsLilError *error)
{
  XsLilBlock *block = nullptr;
  XsLilStatus status = require_block(builder, &block, error);
  return status == XS_LIL_OK ? xs_lil_block_add_const_str(block, encoding, units, unit_count, result, error) : status;
}

XsLilStatus xs_lil_builder_binary_integer(XsLilBuilder *builder, XsLilIntegerBinaryOperation operation,
                                          XsLilValueId left, XsLilValueId right, XsLilValueId *result,
                                          XsLilError *error)
{
  XsLilBlock *block = nullptr;
  XsLilStatus status = require_block(builder, &block, error);
  if(status != XS_LIL_OK)
    return status;
  if(block == nullptr)
    return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL builder insertion block is unavailable");
  XsLilFunction *function = block->owner;
  if((size_t)left >= function->value_count || (size_t)right >= function->value_count ||
     !xs_lil_type_equal(function->values[left].type, function->values[right].type))
    return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL integer operands must have one known type");
  return xs_lil_block_binary_integer(block, operation, function->values[left].type, left, right, result, error);
}

static XsLilStatus builder_float(XsLilBuilder *builder, XsLilFloatBinaryOperation operation,
                                 XsLilFloatComparisonOperation comparison, bool compare, XsLilValueId left,
                                 XsLilValueId right, XsLilValueId *result, XsLilError *error)
{
  XsLilBlock *block = nullptr;
  XsLilStatus status = require_block(builder, &block, error);
  if(status != XS_LIL_OK)
    return status;
  if(block == nullptr)
    return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL builder insertion block is unavailable");
  XsLilFunction *function = block->owner;
  if((size_t)left >= function->value_count || (size_t)right >= function->value_count ||
     !xs_lil_type_equal(function->values[left].type, function->values[right].type))
    return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL float operands must have one known type");
  XsLilType type = function->values[left].type;
  return compare ? xs_lil_block_compare_float(block, comparison, type, left, right, result, error)
                 : xs_lil_block_binary_float(block, operation, type, left, right, result, error);
}

XsLilStatus xs_lil_builder_binary_float(XsLilBuilder *builder, XsLilFloatBinaryOperation operation, XsLilValueId left,
                                        XsLilValueId right, XsLilValueId *result, XsLilError *error)
{
  return builder_float(builder, operation, XS_LIL_FLOAT_EQ, false, left, right, result, error);
}

XsLilStatus xs_lil_builder_compare_float(XsLilBuilder *builder, XsLilFloatComparisonOperation operation,
                                         XsLilValueId left, XsLilValueId right, XsLilValueId *result, XsLilError *error)
{
  return builder_float(builder, XS_LIL_FLOAT_ADD, operation, true, left, right, result, error);
}

XsLilStatus xs_lil_builder_call(XsLilBuilder *builder, const char *callee, const XsLilValueId *arguments,
                                size_t argument_count, XsLilValueId *result, XsLilError *error)
{
  XsLilBlock *block = nullptr;
  XsLilStatus status = require_block(builder, &block, error);
  if(status != XS_LIL_OK)
    return status;
  if(block == nullptr)
    return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL builder insertion block is unavailable");
  const XsLilFunction *function = xs_lil_module_function_named(builder->module, callee);
  if(function == nullptr)
    return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL call target is not registered in the module");
  if(argument_count != function->parameter_count || (argument_count != 0 && arguments == nullptr))
    return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL call argument count does not match its signature");
  for(size_t index = 0; index < argument_count; ++index)
  {
    if((size_t)arguments[index] >= block->owner->value_count ||
       !xs_lil_type_equal(block->owner->values[arguments[index]].type, function->parameters[index]))
      return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL call argument type does not match its signature");
  }
  if(function->return_type.kind == XS_LIL_TYPE_VOID)
  {
    if(result != nullptr)
      *result = XS_LIL_INVALID_VALUE_ID;
    return xs_lil_block_add_void_call(block, callee, arguments, argument_count, error);
  }
  if(result == nullptr)
    return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "non-void XLIL call requires a result output");
  return xs_lil_block_add_call(block, callee, function->return_type, arguments, argument_count, result, error);
}

XsLilStatus xs_lil_builder_extract(XsLilBuilder *builder, XsLilValueId aggregate, uint32_t field, XsLilValueId *result,
                                   XsLilError *error)
{
  XsLilBlock *block = nullptr;
  XsLilStatus status = require_block(builder, &block, error);
  if(status != XS_LIL_OK)
    return status;
  if(block == nullptr)
    return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL builder insertion block is unavailable");
  XsLilFunction *function = block->owner;
  if((size_t)aggregate >= function->value_count)
    return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL extract references an unknown value");
  XsLilType aggregate_type = function->values[aggregate].type;
  XsLilType field_type;
  if(aggregate_type.kind == XS_LIL_TYPE_AGGREGATE &&
     field < xs_lil_module_aggregate_field_count(builder->module, aggregate_type.registry_id))
    field_type = xs_lil_module_aggregate_field_type(builder->module, aggregate_type.registry_id, field);
  else if(aggregate_type.kind == XS_LIL_TYPE_ARRAY &&
          !xs_lil_module_array_is_dynamic(builder->module, aggregate_type.registry_id) &&
          field < xs_lil_module_array_length(builder->module, aggregate_type.registry_id))
    field_type = xs_lil_module_array_element_type(builder->module, aggregate_type.registry_id);
  else
    return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL extract field is outside the composite type");
  return xs_lil_block_add_extract(block, aggregate, field, field_type, result, error);
}

XsLilStatus xs_lil_builder_array_get(XsLilBuilder *builder, XsLilValueId array, XsLilValueId index,
                                     XsLilValueId *result, XsLilError *error)
{
  XsLilBlock *block = nullptr;
  XsLilStatus status = require_block(builder, &block, error);
  if(status != XS_LIL_OK)
    return status;
  if(block == nullptr)
    return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL builder insertion block is unavailable");
  XsLilFunction *function = block->owner;
  if((size_t)array >= function->value_count || function->values[array].type.kind != XS_LIL_TYPE_ARRAY)
    return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL array.get references an unknown array");
  XsLilType array_type = function->values[array].type;
  XsLilType element_type = xs_lil_module_array_element_type(builder->module, array_type.registry_id);
  return xs_lil_block_add_array_get(block, array, index, element_type, result, error);
}

XsLilStatus xs_lil_builder_return(XsLilBuilder *builder, XsLilError *error)
{
  XsLilBlock *block = nullptr;
  XsLilStatus status = require_block(builder, &block, error);
  return status == XS_LIL_OK ? xs_lil_block_set_return(block, error) : status;
}

XsLilStatus xs_lil_builder_return_value(XsLilBuilder *builder, XsLilValueId value, XsLilError *error)
{
  XsLilBlock *block = nullptr;
  XsLilStatus status = require_block(builder, &block, error);
  return status == XS_LIL_OK ? xs_lil_block_set_return_value(block, value, error) : status;
}

XsLilStatus xs_lil_builder_branch(XsLilBuilder *builder, const XsLilBlock *target, XsLilError *error)
{
  XsLilBlock *block = nullptr;
  XsLilStatus status = require_block(builder, &block, error);
  if(status != XS_LIL_OK)
    return status;
  if(!same_function(block, target))
    return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL branch blocks must belong to one function");
  return xs_lil_block_set_branch(block, target->id, error);
}

XsLilStatus xs_lil_builder_branch_if(XsLilBuilder *builder, XsLilValueId condition, const XsLilBlock *then_block,
                                     const XsLilBlock *else_block, XsLilError *error)
{
  XsLilBlock *block = nullptr;
  XsLilStatus status = require_block(builder, &block, error);
  if(status != XS_LIL_OK)
    return status;
  if(!same_function(block, then_block) || !same_function(block, else_block))
    return xs_lil_set_error(error, XS_LIL_INVALID_ARGUMENT, "XLIL branch blocks must belong to one function");
  return xs_lil_block_set_branch_if(block, condition, then_block->id, else_block->id, error);
}

XsLilStatus xs_lil_builder_const_f32_bits(XsLilBuilder *builder, uint32_t bits, XsLilValueId *result, XsLilError *error)
{
  XsLilBlock *block = nullptr;
  XsLilStatus status = require_block(builder, &block, error);
  return status == XS_LIL_OK ? xs_lil_block_add_const_f32_bits(block, bits, result, error) : status;
}

XsLilStatus xs_lil_builder_const_f64_bits(XsLilBuilder *builder, uint64_t bits, XsLilValueId *result, XsLilError *error)
{
  XsLilBlock *block = nullptr;
  XsLilStatus status = require_block(builder, &block, error);
  return status == XS_LIL_OK ? xs_lil_block_add_const_f64_bits(block, bits, result, error) : status;
}

XsLilStatus xs_lil_builder_compare_str(XsLilBuilder *builder, XsLilStrComparisonOperation operation, XsLilValueId left,
                                       XsLilValueId right, XsLilValueId *result, XsLilError *error)
{
  XsLilBlock *block = nullptr;
  XsLilStatus status = require_block(builder, &block, error);
  return status == XS_LIL_OK ? xs_lil_block_compare_str(block, operation, left, right, result, error) : status;
}

XsLilStatus xs_lil_builder_not_bool(XsLilBuilder *builder, XsLilValueId operand, XsLilValueId *result,
                                    XsLilError *error)
{
  XsLilBlock *block = nullptr;
  XsLilStatus status = require_block(builder, &block, error);
  return status == XS_LIL_OK ? xs_lil_block_not_bool(block, operand, result, error) : status;
}

XsLilStatus xs_lil_builder_aggregate(XsLilBuilder *builder, XsLilType type, const XsLilValueId *fields,
                                     size_t field_count, XsLilValueId *result, XsLilError *error)
{
  XsLilBlock *block = nullptr;
  XsLilStatus status = require_block(builder, &block, error);
  return status == XS_LIL_OK ? xs_lil_block_add_aggregate(block, type, fields, field_count, result, error) : status;
}

XsLilStatus xs_lil_builder_array(XsLilBuilder *builder, XsLilType type, const XsLilValueId *elements,
                                 size_t element_count, XsLilValueId *result, XsLilError *error)
{
  XsLilBlock *block = nullptr;
  XsLilStatus status = require_block(builder, &block, error);
  return status == XS_LIL_OK ? xs_lil_block_add_array(block, type, elements, element_count, result, error) : status;
}

XsLilStatus xs_lil_builder_array_set(XsLilBuilder *builder, XsLilValueId array, XsLilValueId index,
                                     XsLilValueId replacement, XsLilValueId *result, XsLilError *error)
{
  XsLilBlock *block = nullptr;
  XsLilStatus status = require_block(builder, &block, error);
  return status == XS_LIL_OK ? xs_lil_block_add_array_set(block, array, index, replacement, result, error) : status;
}

XsLilStatus xs_lil_builder_array_length(XsLilBuilder *builder, XsLilValueId array, XsLilValueId *result,
                                        XsLilError *error)
{
  XsLilBlock *block = nullptr;
  XsLilStatus status = require_block(builder, &block, error);
  return status == XS_LIL_OK ? xs_lil_block_add_array_length(block, array, result, error) : status;
}

XsLilStatus xs_lil_builder_load(XsLilBuilder *builder, XsLilSlotId slot, XsLilValueId *result, XsLilError *error)
{
  XsLilBlock *block = nullptr;
  XsLilStatus status = require_block(builder, &block, error);
  return status == XS_LIL_OK ? xs_lil_block_add_load(block, slot, result, error) : status;
}

XsLilStatus xs_lil_builder_store(XsLilBuilder *builder, XsLilSlotId slot, XsLilValueId value, XsLilError *error)
{
  XsLilBlock *block = nullptr;
  XsLilStatus status = require_block(builder, &block, error);
  return status == XS_LIL_OK ? xs_lil_block_add_store(block, slot, value, error) : status;
}

XsLilStatus xs_lil_builder_panic(XsLilBuilder *builder, XsLilError *error)
{
  XsLilBlock *block = nullptr;
  XsLilStatus status = require_block(builder, &block, error);
  return status == XS_LIL_OK ? xs_lil_block_set_panic(block, error) : status;
}
