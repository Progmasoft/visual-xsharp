/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

#include "llvm_string.h"

#include <llvm-c/Core.h>

#include <stdio.h>
#include <stdlib.h>

static XsBackendStatus fail(XsBackendError *error, XsBackendStatus status, const char *message)
{
    if(error != nullptr)
    {
        error->status = status;
        snprintf(error->message, sizeof(error->message), "%s", message);
    }
    return status;
}

XsBackendStatus xs_llvm_lower_lil_const_str(XsLlvmBackend *backend, XsLlvmCodegenUnit *unit, const XsLilBlock *block,
                                            size_t index, LLVMValueRef *value, XsBackendError *error)
{
    size_t unit_count = xs_lil_block_instruction_utf32_length(block, index);
    if(unit_count > SIZE_MAX / 4U)
        return fail(error, XS_BACKEND_INVALID_ARGUMENT, "XLIL const.str is too large for LLVM lowering");
    size_t byte_count = unit_count * 4U;
    LLVMContextRef context = LLVMGetModuleContext(xs_llvm_codegen_unit_module(unit));
    LLVMTypeRef byte_type = LLVMInt8TypeInContext(context);
    LLVMValueRef *bytes = calloc(byte_count == 0 ? 1 : byte_count, sizeof(*bytes));
    if(bytes == nullptr)
        return fail(error, XS_BACKEND_SYSTEM_ERROR, "out of memory while lowering XLIL const.str");
    XsLilUtf32Encoding encoding = xs_lil_block_instruction_utf32_encoding(block, index);
    for(size_t current = 0; current < unit_count; ++current)
    {
        uint32_t code_point = xs_lil_block_instruction_utf32_unit(block, index, current);
        for(size_t byte = 0; byte < 4U; ++byte)
        {
            size_t shift = encoding == XS_LIL_UTF32_LE ? byte * 8U : (3U - byte) * 8U;
            bytes[current * 4U + byte] = LLVMConstInt(byte_type, (code_point >> shift) & 0xFFU, false);
        }
    }
    LLVMTypeRef array_type = LLVMArrayType2(byte_type, (uint64_t)byte_count);
    LLVMValueRef initializer = LLVMConstArray2(byte_type, bytes, (uint64_t)byte_count);
    free(bytes);
    LLVMValueRef global = LLVMAddGlobal(xs_llvm_codegen_unit_module(unit), array_type, ".vxs.str");
    LLVMSetInitializer(global, initializer);
    LLVMSetGlobalConstant(global, true);
    LLVMSetLinkage(global, LLVMPrivateLinkage);
    LLVMSetUnnamedAddress(global, LLVMGlobalUnnamedAddr);
    LLVMSetAlignment(global, 4);

    LLVMTypeRef str_type = nullptr;
    XsBackendStatus status = xs_llvm_lil_type(backend, (XsLilType){.kind = XS_LIL_TYPE_STR}, &str_type, error);
    if(status != XS_BACKEND_OK)
        return status;
    LLVMTypeRef length_type = LLVMStructGetTypeAtIndex(str_type, 1);
    LLVMValueRef fields[] = {global, LLVMConstInt(length_type, (unsigned long long)unit_count, false)};
    *value = LLVMConstNamedStruct(str_type, fields, 2);
    return *value == nullptr ? fail(error, XS_BACKEND_LLVM_ERROR, "LLVM could not create XLIL str view")
                             : XS_BACKEND_OK;
}

XsBackendStatus xs_llvm_lower_lil_compare_str(XsLlvmCodegenUnit *unit, LLVMBuilderRef builder, const XsLilBlock *block,
                                              size_t index, LLVMValueRef *values, size_t value_count,
                                              XsBackendError *error)
{
    XsLilValueId result = xs_lil_block_instruction_result(block, index);
    XsLilValueId left = xs_lil_block_instruction_left(block, index);
    XsLilValueId right = xs_lil_block_instruction_right(block, index);
    if((size_t)result >= value_count || (size_t)left >= value_count || (size_t)right >= value_count ||
       values[left] == nullptr || values[right] == nullptr)
        return fail(error, XS_BACKEND_INVALID_ARGUMENT, "XLIL Str comparison references an unavailable value");

    LLVMValueRef left_pointer = LLVMBuildExtractValue(builder, values[left], 0, "str.left.ptr");
    LLVMValueRef left_length = LLVMBuildExtractValue(builder, values[left], 1, "str.left.len");
    LLVMValueRef right_pointer = LLVMBuildExtractValue(builder, values[right], 0, "str.right.ptr");
    LLVMValueRef right_length = LLVMBuildExtractValue(builder, values[right], 1, "str.right.len");
    LLVMValueRef lengths_equal = LLVMBuildICmp(builder, LLVMIntEQ, left_length, right_length, "str.len.eq");
    LLVMValueRef left_is_shorter = LLVMBuildICmp(builder, LLVMIntULE, left_length, right_length, "str.len.le");
    LLVMValueRef compared_units = LLVMBuildSelect(builder, left_is_shorter, left_length, right_length, "str.min.len");
    LLVMValueRef byte_count =
        LLVMBuildShl(builder, compared_units, LLVMConstInt(LLVMTypeOf(compared_units), 2, false), "str.byte.len");

    LLVMContextRef context = LLVMGetModuleContext(xs_llvm_codegen_unit_module(unit));
    LLVMTypeRef parameters[] = {LLVMPointerTypeInContext(context, 0), LLVMPointerTypeInContext(context, 0),
                                LLVMTypeOf(byte_count)};
    LLVMTypeRef memcmp_type = LLVMFunctionType(LLVMInt32TypeInContext(context), parameters, 3, false);
    LLVMValueRef memcmp_function = LLVMGetNamedFunction(xs_llvm_codegen_unit_module(unit), "memcmp");
    if(memcmp_function == nullptr)
        memcmp_function = LLVMAddFunction(xs_llvm_codegen_unit_module(unit), "memcmp", memcmp_type);
    LLVMValueRef arguments[] = {left_pointer, right_pointer, byte_count};
    LLVMValueRef comparison = LLVMBuildCall2(builder, memcmp_type, memcmp_function, arguments, 3, "str.memcmp");
    LLVMValueRef contents_equal = LLVMBuildICmp(builder, LLVMIntEQ, comparison,
                                                LLVMConstInt(LLVMInt32TypeInContext(context), 0, false), "str.data.eq");
    LLVMValueRef equal = LLVMBuildAnd(builder, lengths_equal, contents_equal, "str.eq");
    if(xs_lil_block_instruction_kind(block, index) == XS_LIL_INSTRUCTION_NE_STR)
        equal = LLVMBuildNot(builder, equal, "str.ne");
    values[result] = equal;
    return equal == nullptr ? fail(error, XS_BACKEND_LLVM_ERROR, "LLVM could not lower XLIL Str comparison")
                            : XS_BACKEND_OK;
}
