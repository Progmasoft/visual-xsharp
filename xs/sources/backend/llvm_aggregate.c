/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "llvm_internal.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

static XsBackendStatus aggregate_error(XsBackendError *error, XsBackendStatus status, const char *message)
{
  if(error != nullptr)
  {
    error->status = status;
    snprintf(error->message, sizeof(error->message), "%s", message);
  }
  return status;
}

XsBackendStatus xs_llvm_codegen_lil_type(XsLlvmCodegenUnit *unit, XsLilType type, LLVMTypeRef *llvm_type,
                                         XsBackendError *error)
{
  if(type.kind != XS_LIL_TYPE_AGGREGATE && type.kind != XS_LIL_TYPE_ARRAY)
    return xs_llvm_lil_type(unit->backend, type, llvm_type, error);
  LLVMTypeRef *registry = type.kind == XS_LIL_TYPE_ARRAY ? unit->lil_array_types : unit->lil_types;
  size_t count = type.kind == XS_LIL_TYPE_ARRAY ? unit->lil_array_type_count : unit->lil_type_count;
  if(type.registry_id >= count || registry[type.registry_id] == nullptr)
    return aggregate_error(error, XS_BACKEND_INVALID_ARGUMENT,
                           "XLIL composite type is not registered in the codegen unit");
  *llvm_type = registry[type.registry_id];
  return XS_BACKEND_OK;
}

XsBackendStatus xs_llvm_register_lil_types(XsLlvmCodegenUnit *unit, const XsLilModule *module, XsBackendError *error)
{
  if(error != nullptr)
    *error = (XsBackendError){.status = XS_BACKEND_OK};
  if(unit == nullptr || module == nullptr || unit->lil_types != nullptr || unit->lil_array_types != nullptr)
    return aggregate_error(error, XS_BACKEND_INVALID_ARGUMENT, "fresh codegen unit and XLIL module are required");
  size_t count = xs_lil_module_aggregate_type_count(module);
  if(count != 0)
    unit->lil_types = calloc(count, sizeof(*unit->lil_types));
  if(count != 0 && unit->lil_types == nullptr)
    return aggregate_error(error, XS_BACKEND_SYSTEM_ERROR, "out of memory while registering XLIL aggregate types");
  unit->lil_type_count = count;
  for(size_t index = 0; index < count; ++index)
  {
    unit->lil_types[index] =
        LLVMStructCreateNamed(unit->backend->context, xs_lil_module_aggregate_type_name(module, (uint32_t)index));
    if(unit->lil_types[index] == nullptr)
      return aggregate_error(error, XS_BACKEND_LLVM_ERROR, "LLVM could not create an XLIL aggregate type");
  }
  for(size_t index = 0; index < count; ++index)
  {
    size_t field_count = xs_lil_module_aggregate_field_count(module, (uint32_t)index);
    LLVMTypeRef *fields = field_count == 0 ? nullptr : malloc(field_count * sizeof(*fields));
    if(field_count != 0 && fields == nullptr)
      return aggregate_error(error, XS_BACKEND_SYSTEM_ERROR, "out of memory while lowering XLIL aggregate fields");
    XsBackendStatus status = XS_BACKEND_OK;
    for(size_t field = 0; field < field_count && status == XS_BACKEND_OK; ++field)
      status = xs_llvm_codegen_lil_type(unit, xs_lil_module_aggregate_field_type(module, (uint32_t)index, field),
                                        &fields[field], error);
    if(status == XS_BACKEND_OK)
      LLVMStructSetBody(unit->lil_types[index], fields, (unsigned)field_count, false);
    free(fields);
    if(status != XS_BACKEND_OK)
      return status;
  }
  size_t array_count = xs_lil_module_array_type_count(module);
  if(array_count != 0)
  {
    unit->lil_array_types = calloc(array_count, sizeof(*unit->lil_array_types));
    unit->lil_array_elements = calloc(array_count, sizeof(*unit->lil_array_elements));
    unit->lil_array_dynamic = calloc(array_count, sizeof(*unit->lil_array_dynamic));
  }
  if(array_count != 0 &&
     (unit->lil_array_types == nullptr || unit->lil_array_elements == nullptr || unit->lil_array_dynamic == nullptr))
    return aggregate_error(error, XS_BACKEND_SYSTEM_ERROR, "out of memory while registering XLIL array types");
  unit->lil_array_type_count = array_count;
  for(size_t index = 0; index < array_count; ++index)
  {
    LLVMTypeRef element = nullptr;
    XsBackendStatus status =
        xs_llvm_codegen_lil_type(unit, xs_lil_module_array_element_type(module, (uint32_t)index), &element, error);
    if(status != XS_BACKEND_OK)
      return status;
    unit->lil_array_elements[index] = element;
    unit->lil_array_dynamic[index] = xs_lil_module_array_is_dynamic(module, (uint32_t)index);
    if(unit->lil_array_dynamic[index])
    {
      LLVMTypeRef fields[] = {LLVMPointerTypeInContext(unit->backend->context, 0),
                              LLVMInt64TypeInContext(unit->backend->context)};
      unit->lil_array_types[index] = LLVMStructTypeInContext(unit->backend->context, fields, 2, false);
    }
    else
      unit->lil_array_types[index] = LLVMArrayType2(element, xs_lil_module_array_length(module, (uint32_t)index));
    if(unit->lil_array_types[index] == nullptr)
      return aggregate_error(error, XS_BACKEND_LLVM_ERROR, "LLVM could not create an XLIL array type");
  }
  return XS_BACKEND_OK;
}

XsBackendStatus xs_llvm_lower_aggregate_instruction(XsLlvmCodegenUnit *unit, LLVMBuilderRef builder,
                                                    const XsLilFunction *function, const XsLilBlock *block,
                                                    size_t index, LLVMValueRef *values, size_t value_count,
                                                    XsBackendError *error)
{
  XsLilInstructionKind kind = xs_lil_block_instruction_kind(block, index);
  XsLilValueId result = xs_lil_block_instruction_result(block, index);
  if(kind == XS_LIL_INSTRUCTION_ARRAY_LENGTH)
  {
    XsLilValueId array = xs_lil_block_instruction_left(block, index);
    if(result >= value_count || array >= value_count || values[array] == nullptr)
      return aggregate_error(error, XS_BACKEND_INVALID_ARGUMENT, "XLIL array length operand is unavailable");
    XsLilType source_type = xs_lil_function_value_type(function, array);
    LLVMTypeRef array_type = nullptr;
    XsBackendStatus status = xs_llvm_codegen_lil_type(unit, source_type, &array_type, error);
    if(status != XS_BACKEND_OK)
      return status;
    values[result] =
        unit->lil_array_dynamic[source_type.registry_id]
            ? LLVMBuildExtractValue(builder, values[array], 1, "array.length")
            : LLVMConstInt(LLVMInt64TypeInContext(unit->backend->context), LLVMGetArrayLength2(array_type), false);
    return values[result] == nullptr
               ? aggregate_error(error, XS_BACKEND_LLVM_ERROR, "LLVM could not lower XLIL array length")
               : XS_BACKEND_OK;
  }
  if(kind == XS_LIL_INSTRUCTION_ARRAY_GET || kind == XS_LIL_INSTRUCTION_ARRAY_SET)
  {
    XsLilValueId array = xs_lil_block_instruction_left(block, index);
    XsLilValueId dynamic_index = xs_lil_block_instruction_right(block, index);
    if(result >= value_count || array >= value_count || dynamic_index >= value_count || values[array] == nullptr ||
       values[dynamic_index] == nullptr)
      return aggregate_error(error, XS_BACKEND_INVALID_ARGUMENT, "XLIL dynamic array operand is unavailable");
    LLVMTypeRef array_type = nullptr;
    XsLilType source_type = xs_lil_function_value_type(function, array);
    XsBackendStatus status = xs_llvm_codegen_lil_type(unit, source_type, &array_type, error);
    if(status != XS_BACKEND_OK)
      return status;
    LLVMTypeRef i64 = LLVMInt64TypeInContext(unit->backend->context);
    LLVMValueRef zero = LLVMConstInt(i64, 0, false);
    bool dynamic = unit->lil_array_dynamic[source_type.registry_id];
    LLVMValueRef length = dynamic ? LLVMBuildExtractValue(builder, values[array], 1, "array.length")
                                  : LLVMConstInt(i64, LLVMGetArrayLength2(array_type), false);
    LLVMValueRef nonnegative = LLVMBuildICmp(builder, LLVMIntSGE, values[dynamic_index], zero, "index.nonnegative");
    LLVMValueRef in_range = LLVMBuildICmp(builder, LLVMIntULT, values[dynamic_index], length, "index.in.range");
    LLVMValueRef valid = LLVMBuildAnd(builder, nonnegative, in_range, "index.valid");
    LLVMValueRef llvm_function = LLVMGetBasicBlockParent(LLVMGetInsertBlock(builder));
    LLVMBasicBlockRef access_block =
        LLVMAppendBasicBlockInContext(unit->backend->context, llvm_function, "array.access");
    LLVMBasicBlockRef trap_block = LLVMAppendBasicBlockInContext(unit->backend->context, llvm_function, "array.bounds");
    LLVMBuildCondBr(builder, valid, access_block, trap_block);
    LLVMPositionBuilderAtEnd(builder, trap_block);
    LLVMTypeRef trap_type = LLVMFunctionType(LLVMVoidTypeInContext(unit->backend->context), nullptr, 0, false);
    LLVMValueRef trap = LLVMGetNamedFunction(unit->module, "llvm.trap");
    if(trap == nullptr)
      trap = LLVMAddFunction(unit->module, "llvm.trap", trap_type);
    LLVMBuildCall2(builder, trap_type, trap, nullptr, 0, "");
    LLVMBuildUnreachable(builder);
    LLVMPositionBuilderAtEnd(builder, access_block);
    LLVMValueRef element = nullptr;
    LLVMValueRef fixed_storage = nullptr;
    if(dynamic)
    {
      LLVMValueRef pointer = LLVMBuildExtractValue(builder, values[array], 0, "array.data");
      element = LLVMBuildGEP2(builder, unit->lil_array_elements[source_type.registry_id], pointer,
                              &values[dynamic_index], 1, "array.element");
    }
    else
    {
      fixed_storage = LLVMBuildAlloca(builder, array_type, "array.fixed.storage");
      LLVMBuildStore(builder, values[array], fixed_storage);
      LLVMValueRef indices[] = {zero, values[dynamic_index]};
      element = LLVMBuildGEP2(builder, array_type, fixed_storage, indices, 2, "array.element");
    }
    if(kind == XS_LIL_INSTRUCTION_ARRAY_GET)
    {
      LLVMTypeRef element_type = nullptr;
      status = xs_llvm_codegen_lil_type(unit, xs_lil_function_value_type(function, result), &element_type, error);
      if(status != XS_BACKEND_OK)
        return status;
      values[result] = LLVMBuildLoad2(builder, element_type, element, "array.get");
    }
    else
    {
      XsLilValueId replacement = xs_lil_block_instruction_argument(block, index, 0);
      if(replacement >= value_count || values[replacement] == nullptr)
        return aggregate_error(error, XS_BACKEND_INVALID_ARGUMENT, "XLIL array.set value is unavailable");
      LLVMBuildStore(builder, values[replacement], element);
      values[result] = dynamic ? values[array] : LLVMBuildLoad2(builder, array_type, fixed_storage, "array.set");
    }
    return values[result] == nullptr
               ? aggregate_error(error, XS_BACKEND_LLVM_ERROR, "LLVM could not lower dynamic XLIL array access")
               : XS_BACKEND_OK;
  }
  if(kind == XS_LIL_INSTRUCTION_EXTRACT)
  {
    XsLilValueId aggregate = xs_lil_block_instruction_left(block, index);
    int64_t field = xs_lil_block_instruction_i64(block, index);
    if(aggregate >= value_count || values[aggregate] == nullptr || field < 0 || (uint64_t)field > UINT_MAX)
      return aggregate_error(error, XS_BACKEND_INVALID_ARGUMENT, "XLIL extract source or field is unavailable");
    values[result] = LLVMBuildExtractValue(builder, values[aggregate], (unsigned)field, "extract");
    return values[result] == nullptr
               ? aggregate_error(error, XS_BACKEND_LLVM_ERROR, "LLVM could not extract an XLIL aggregate field")
               : XS_BACKEND_OK;
  }
  LLVMTypeRef type = nullptr;
  XsLilType result_type = xs_lil_function_value_type(function, result);
  XsBackendStatus status = xs_llvm_codegen_lil_type(unit, result_type, &type, error);
  if(status != XS_BACKEND_OK)
    return status;
  size_t field_count = xs_lil_block_instruction_argument_count(block, index);
  if(result_type.kind == XS_LIL_TYPE_ARRAY && unit->lil_array_dynamic[result_type.registry_id])
  {
    LLVMTypeRef i64 = LLVMInt64TypeInContext(unit->backend->context);
    uint64_t element_size =
        LLVMABISizeOfType(unit->backend->target_data, unit->lil_array_elements[result_type.registry_id]);
    LLVMValueRef byte_count = LLVMConstInt(i64, element_size * field_count, false);
    LLVMTypeRef malloc_type = LLVMFunctionType(LLVMPointerTypeInContext(unit->backend->context, 0), &i64, 1, false);
    LLVMValueRef malloc_function = LLVMGetNamedFunction(unit->module, "malloc");
    if(malloc_function == nullptr)
      malloc_function = LLVMAddFunction(unit->module, "malloc", malloc_type);
    LLVMValueRef pointer = LLVMBuildCall2(builder, malloc_type, malloc_function, &byte_count, 1, "array.data");
    if(pointer == nullptr)
      return aggregate_error(error, XS_BACKEND_LLVM_ERROR, "LLVM could not allocate runtime-sized XLIL array storage");
    for(size_t field = 0; field < field_count; ++field)
    {
      XsLilValueId value = xs_lil_block_instruction_argument(block, index, field);
      if(value >= value_count || values[value] == nullptr)
        return aggregate_error(error, XS_BACKEND_INVALID_ARGUMENT, "XLIL array references an unavailable element");
      LLVMValueRef offset = LLVMConstInt(i64, field, false);
      LLVMValueRef element = LLVMBuildGEP2(builder, unit->lil_array_elements[result_type.registry_id], pointer, &offset,
                                           1, "array.element");
      LLVMBuildStore(builder, values[value], element);
    }
    LLVMValueRef view = LLVMGetUndef(type);
    view = LLVMBuildInsertValue(builder, view, pointer, 0, "array.with.data");
    values[result] = LLVMBuildInsertValue(builder, view, LLVMConstInt(i64, field_count, false), 1, "array.with.length");
    return values[result] == nullptr
               ? aggregate_error(error, XS_BACKEND_LLVM_ERROR, "LLVM could not construct runtime-sized XLIL array")
               : XS_BACKEND_OK;
  }
  LLVMValueRef aggregate = LLVMGetUndef(type);
  for(size_t field = 0; field < field_count; ++field)
  {
    XsLilValueId value = xs_lil_block_instruction_argument(block, index, field);
    if(value >= value_count || values[value] == nullptr)
      return aggregate_error(error, XS_BACKEND_INVALID_ARGUMENT, "XLIL aggregate references an unavailable field");
    aggregate = LLVMBuildInsertValue(builder, aggregate, values[value], (unsigned)field, "aggregate");
  }
  values[result] = aggregate;
  return XS_BACKEND_OK;
}
