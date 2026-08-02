/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "xs/backend/llvm_backend.h"

#include "llvm_integer.h"
#include "llvm_internal.h"
#include "llvm_string.h"

#include <llvm-c/Target.h>

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void xs_llvm_clear_error(XsBackendError *error)
{
    if(error != nullptr)
        *error = (XsBackendError){.status = XS_BACKEND_OK};
}

XsBackendStatus xs_llvm_set_error(XsBackendError *error, XsBackendStatus status, const char *message)
{
    if(error != nullptr)
    {
        error->status = status;
        snprintf(error->message, sizeof(error->message), "%s", message == nullptr ? "unknown backend error" : message);
    }
    return status;
}

char *xs_llvm_copy_text(const char *text)
{
    size_t length = strlen(text);
    char *copy = malloc(length + 1);
    if(copy != nullptr)
        memcpy(copy, text, length + 1);
    return copy;
}

static LLVMCodeGenOptLevel codegen_level(XsLlvmOptimizationLevel level)
{
    switch(level)
    {
    case XS_LLVM_OPT_NONE:
        return LLVMCodeGenLevelNone;
    case XS_LLVM_OPT_LESS:
        return LLVMCodeGenLevelLess;
    case XS_LLVM_OPT_DEFAULT:
        return LLVMCodeGenLevelDefault;
    case XS_LLVM_OPT_AGGRESSIVE:
        return LLVMCodeGenLevelAggressive;
    }
    return LLVMCodeGenLevelDefault;
}

XsBackendStatus xs_llvm_backend_create(const XsLlvmBackendConfig *config, XsLlvmBackend **backend,
                                       XsBackendError *error)
{
    xs_llvm_clear_error(error);
    if(config == nullptr || backend == nullptr)
        return xs_llvm_set_error(error, XS_BACKEND_INVALID_ARGUMENT, "backend configuration and output are required");
    if((unsigned)config->optimization > (unsigned)XS_LLVM_OPT_AGGRESSIVE)
        return xs_llvm_set_error(error, XS_BACKEND_INVALID_ARGUMENT, "invalid LLVM optimization level");
    *backend = nullptr;

    LLVMInitializeAllTargetInfos();
    LLVMInitializeAllTargets();
    LLVMInitializeAllTargetMCs();
    LLVMInitializeAllAsmPrinters();

    char *default_triple = nullptr;
    const char *requested_triple = config->target_triple;
    if(requested_triple == nullptr || requested_triple[0] == '\0')
    {
        default_triple = LLVMGetDefaultTargetTriple();
        requested_triple = default_triple;
    }

    LLVMTargetRef target = nullptr;
    char *llvm_error = nullptr;
    if(LLVMGetTargetFromTriple(requested_triple, &target, &llvm_error) != 0)
    {
        XsBackendStatus status = xs_llvm_set_error(error, XS_BACKEND_LLVM_ERROR, llvm_error);
        LLVMDisposeMessage(llvm_error);
        if(default_triple != nullptr)
            LLVMDisposeMessage(default_triple);
        return status;
    }

    XsLlvmBackend *result = calloc(1, sizeof(*result));
    if(result == nullptr)
    {
        if(default_triple != nullptr)
            LLVMDisposeMessage(default_triple);
        return xs_llvm_set_error(error, XS_BACKEND_SYSTEM_ERROR, "out of memory while creating LLVM backend");
    }
    result->context = LLVMContextCreate();
    result->optimization = config->optimization;
    result->verify_modules = config->verify_modules;
    result->target_triple = xs_llvm_copy_text(requested_triple);
    result->target_machine =
        LLVMCreateTargetMachine(target, requested_triple, config->cpu == nullptr ? "" : config->cpu,
                                config->features == nullptr ? "" : config->features,
                                codegen_level(config->optimization), LLVMRelocPIC, LLVMCodeModelDefault);
    if(default_triple != nullptr)
        LLVMDisposeMessage(default_triple);

    if(result->context == nullptr || result->target_triple == nullptr || result->target_machine == nullptr)
    {
        xs_llvm_backend_destroy(result);
        return xs_llvm_set_error(error, XS_BACKEND_LLVM_ERROR,
                                 "LLVM could not create the target context or target machine");
    }
    result->target_data = LLVMCreateTargetDataLayout(result->target_machine);
    if(result->target_data == nullptr)
    {
        xs_llvm_backend_destroy(result);
        return xs_llvm_set_error(error, XS_BACKEND_LLVM_ERROR, "LLVM could not determine target data layout");
    }
    char *layout = LLVMCopyStringRepOfTargetData(result->target_data);
    if(layout == nullptr)
    {
        xs_llvm_backend_destroy(result);
        return xs_llvm_set_error(error, XS_BACKEND_LLVM_ERROR, "LLVM could not format target data layout");
    }
    result->data_layout = xs_llvm_copy_text(layout);
    LLVMDisposeMessage(layout);
    if(result->target_data == nullptr || result->data_layout == nullptr)
    {
        xs_llvm_backend_destroy(result);
        return xs_llvm_set_error(error, XS_BACKEND_LLVM_ERROR, "LLVM could not determine target data layout");
    }
    *backend = result;
    return XS_BACKEND_OK;
}

void xs_llvm_backend_destroy(XsLlvmBackend *backend)
{
    if(backend == nullptr)
        return;
    if(backend->target_data != nullptr)
        LLVMDisposeTargetData(backend->target_data);
    if(backend->target_machine != nullptr)
        LLVMDisposeTargetMachine(backend->target_machine);
    if(backend->context != nullptr)
        LLVMContextDispose(backend->context);
    free(backend->target_triple);
    free(backend->data_layout);
    free(backend);
}

LLVMContextRef xs_llvm_backend_context(const XsLlvmBackend *backend)
{
    return backend == nullptr ? nullptr : backend->context;
}

const char *xs_llvm_backend_target_triple(const XsLlvmBackend *backend)
{
    return backend == nullptr ? nullptr : backend->target_triple;
}

const char *xs_llvm_backend_data_layout(const XsLlvmBackend *backend)
{
    return backend == nullptr ? nullptr : backend->data_layout;
}

XsBackendStatus xs_llvm_codegen_unit_create(XsLlvmBackend *backend, const char *name, XsLlvmCodegenUnit **unit,
                                            XsBackendError *error)
{
    xs_llvm_clear_error(error);
    if(backend == nullptr || name == nullptr || unit == nullptr)
        return xs_llvm_set_error(error, XS_BACKEND_INVALID_ARGUMENT,
                                 "backend, codegen-unit name, and output are required");
    *unit = calloc(1, sizeof(**unit));
    if(*unit == nullptr)
        return xs_llvm_set_error(error, XS_BACKEND_SYSTEM_ERROR, "out of memory while creating codegen unit");
    (*unit)->backend = backend;
    (*unit)->module = LLVMModuleCreateWithNameInContext(name, backend->context);
    if((*unit)->module == nullptr)
    {
        free(*unit);
        *unit = nullptr;
        return xs_llvm_set_error(error, XS_BACKEND_LLVM_ERROR, "LLVM could not create a module for the codegen unit");
    }
    LLVMSetTarget((*unit)->module, backend->target_triple);
    LLVMSetDataLayout((*unit)->module, backend->data_layout);
    return XS_BACKEND_OK;
}

void xs_llvm_codegen_unit_destroy(XsLlvmCodegenUnit *unit)
{
    if(unit == nullptr)
        return;
    if(unit->module != nullptr)
        LLVMDisposeModule(unit->module);
    free(unit->lil_types);
    free(unit->lil_array_types);
    free(unit->lil_array_elements);
    free(unit->lil_array_dynamic);
    free(unit);
}

LLVMModuleRef xs_llvm_codegen_unit_module(const XsLlvmCodegenUnit *unit)
{
    return unit == nullptr ? nullptr : unit->module;
}

XsBackendStatus xs_llvm_primitive_type(XsLlvmBackend *backend, XsPrimitiveType primitive, LLVMTypeRef *type,
                                       XsBackendError *error)
{
    xs_llvm_clear_error(error);
    if(backend == nullptr || type == nullptr)
        return xs_llvm_set_error(error, XS_BACKEND_INVALID_ARGUMENT, "backend and LLVM type output are required");
    *type = nullptr;
    switch(primitive)
    {
    case XS_PRIMITIVE_UNIT:
        *type = LLVMVoidTypeInContext(backend->context);
        break;
    case XS_PRIMITIVE_BOOL:
        *type = LLVMInt8TypeInContext(backend->context);
        break;
    case XS_PRIMITIVE_BYTE:
    case XS_PRIMITIVE_SBYTE:
        *type = LLVMInt8TypeInContext(backend->context);
        break;
    case XS_PRIMITIVE_SHORT:
    case XS_PRIMITIVE_USHORT:
        *type = LLVMInt16TypeInContext(backend->context);
        break;
    case XS_PRIMITIVE_CHAR:
        *type = LLVMInt32TypeInContext(backend->context);
        break;
    case XS_PRIMITIVE_LONG:
    case XS_PRIMITIVE_ULONG:
        *type = LLVMInt32TypeInContext(backend->context);
        break;
    case XS_PRIMITIVE_INT:
    case XS_PRIMITIVE_UINT:
        *type = LLVMInt64TypeInContext(backend->context);
        break;
    case XS_PRIMITIVE_INTEGER:
    case XS_PRIMITIVE_UINTEGER:
        *type = LLVMInt128TypeInContext(backend->context);
        break;
    case XS_PRIMITIVE_SFLOAT:
        *type = LLVMHalfTypeInContext(backend->context);
        break;
    case XS_PRIMITIVE_LFLOAT:
        *type = LLVMFloatTypeInContext(backend->context);
        break;
    case XS_PRIMITIVE_FLOAT:
        *type = LLVMDoubleTypeInContext(backend->context);
        break;
    case XS_PRIMITIVE_DOUBLE:
        *type = LLVMFP128TypeInContext(backend->context);
        break;
    case XS_PRIMITIVE_STRING:
        *type = LLVMPointerTypeInContext(backend->context, 0);
        break;
    case XS_PRIMITIVE_STR:
    {
        LLVMTypeRef fields[] = {LLVMPointerTypeInContext(backend->context, 0),
                                LLVMIntPtrTypeInContext(backend->context, backend->target_data)};
        *type = LLVMStructTypeInContext(backend->context, fields, 2, false);
        break;
    }
    default:
        return xs_llvm_set_error(error, XS_BACKEND_INVALID_ARGUMENT, "unknown X# primitive type");
    }
    return XS_BACKEND_OK;
}

XsBackendStatus xs_llvm_lil_type(XsLlvmBackend *backend, XsLilType type, LLVMTypeRef *llvm_type, XsBackendError *error)
{
    xs_llvm_clear_error(error);
    if(backend == nullptr || llvm_type == nullptr)
        return xs_llvm_set_error(error, XS_BACKEND_INVALID_ARGUMENT, "backend and LLVM type output are required");
    *llvm_type = nullptr;
    switch(type.kind)
    {
    case XS_LIL_TYPE_VOID:
        *llvm_type = LLVMVoidTypeInContext(backend->context);
        break;
    case XS_LIL_TYPE_BOOL:
        *llvm_type = LLVMInt8TypeInContext(backend->context);
        break;
    case XS_LIL_TYPE_U8:
    case XS_LIL_TYPE_I8:
        *llvm_type = LLVMInt8TypeInContext(backend->context);
        break;
    case XS_LIL_TYPE_U16:
    case XS_LIL_TYPE_I16:
        *llvm_type = LLVMInt16TypeInContext(backend->context);
        break;
    case XS_LIL_TYPE_U32:
    case XS_LIL_TYPE_I32:
        *llvm_type = LLVMInt32TypeInContext(backend->context);
        break;
    case XS_LIL_TYPE_U64:
    case XS_LIL_TYPE_I64:
        *llvm_type = LLVMInt64TypeInContext(backend->context);
        break;
    case XS_LIL_TYPE_U128:
    case XS_LIL_TYPE_I128:
        *llvm_type = LLVMInt128TypeInContext(backend->context);
        break;
    case XS_LIL_TYPE_F32:
        *llvm_type = LLVMFloatTypeInContext(backend->context);
        break;
    case XS_LIL_TYPE_F64:
        *llvm_type = LLVMDoubleTypeInContext(backend->context);
        break;
    case XS_LIL_TYPE_F16:
    case XS_LIL_TYPE_F128:
        return xs_llvm_set_error(error, XS_BACKEND_DEFERRED, "XLIL f16 and f128 lowering is deferred");
    case XS_LIL_TYPE_STR:
    {
        LLVMTypeRef fields[] = {LLVMPointerTypeInContext(backend->context, 0),
                                LLVMIntPtrTypeInContext(backend->context, backend->target_data)};
        *llvm_type = LLVMStructTypeInContext(backend->context, fields, 2, false);
        break;
    }
    case XS_LIL_TYPE_STRING:
        *llvm_type = LLVMPointerTypeInContext(backend->context, 0);
        break;
    case XS_LIL_TYPE_AGGREGATE:
    case XS_LIL_TYPE_ARRAY:
        return xs_llvm_set_error(error, XS_BACKEND_DEFERRED,
                                 "composite XLIL types require a codegen-unit type registry");
    default:
        return xs_llvm_set_error(error, XS_BACKEND_INVALID_ARGUMENT, "unknown XLIL type");
    }
    return XS_BACKEND_OK;
}

static XsBackendStatus declare_function_with_types(XsLlvmCodegenUnit *unit, const char *name, LLVMTypeRef return_type,
                                                   const LLVMTypeRef *parameter_types, size_t parameter_count,
                                                   LLVMValueRef *function, XsBackendError *error)
{
    if(unit == nullptr || name == nullptr || name[0] == '\0' || return_type == nullptr || function == nullptr ||
       (parameter_count != 0 && parameter_types == nullptr))
        return xs_llvm_set_error(error, XS_BACKEND_INVALID_ARGUMENT,
                                 "valid codegen unit and function signature are required");
    if(parameter_count > (size_t)UINT_MAX)
        return xs_llvm_set_error(error, XS_BACKEND_INVALID_ARGUMENT, "function has too many parameters for LLVM C API");
    if(LLVMGetNamedFunction(unit->module, name) != nullptr)
        return xs_llvm_set_error(error, XS_BACKEND_INVALID_ARGUMENT,
                                 "function is already declared in this codegen unit");
    LLVMTypeRef function_type =
        LLVMFunctionType(return_type, (LLVMTypeRef *)parameter_types, (unsigned)parameter_count, false);
    *function = LLVMAddFunction(unit->module, name, function_type);
    return XS_BACKEND_OK;
}

XsBackendStatus xs_llvm_declare_function(XsLlvmCodegenUnit *unit, const XsFunctionSignature *signature,
                                         LLVMValueRef *function, XsBackendError *error)
{
    xs_llvm_clear_error(error);
    if(unit == nullptr || signature == nullptr ||
       (signature->parameter_count != 0 && signature->parameter_types == nullptr))
        return xs_llvm_set_error(error, XS_BACKEND_INVALID_ARGUMENT,
                                 "valid codegen unit and function signature are required");

    LLVMTypeRef return_type = nullptr;
    XsBackendStatus status = xs_llvm_primitive_type(unit->backend, signature->return_type, &return_type, error);
    if(status != XS_BACKEND_OK)
        return status;
    LLVMTypeRef *parameters = nullptr;
    if(signature->parameter_count != 0)
    {
        parameters = malloc(signature->parameter_count * sizeof(*parameters));
        if(parameters == nullptr)
            return xs_llvm_set_error(error, XS_BACKEND_SYSTEM_ERROR, "out of memory while lowering function signature");
    }
    for(size_t i = 0; i < signature->parameter_count; ++i)
    {
        status = xs_llvm_primitive_type(unit->backend, signature->parameter_types[i], &parameters[i], error);
        if(status != XS_BACKEND_OK)
        {
            free(parameters);
            return status;
        }
        if(LLVMGetTypeKind(parameters[i]) == LLVMVoidTypeKind)
        {
            free(parameters);
            return xs_llvm_set_error(error, XS_BACKEND_INVALID_ARGUMENT, "function parameters cannot have unit type");
        }
    }
    status = declare_function_with_types(unit, signature->name, return_type, parameters, signature->parameter_count,
                                         function, error);
    free(parameters);
    return status;
}

XsBackendStatus xs_llvm_declare_lil_function(XsLlvmCodegenUnit *unit, const char *name, XsLilType return_type,
                                             const XsLilType *parameter_types, size_t parameter_count,
                                             LLVMValueRef *function, XsBackendError *error)
{
    xs_llvm_clear_error(error);
    if(unit == nullptr || name == nullptr || function == nullptr ||
       (parameter_count != 0 && parameter_types == nullptr))
        return xs_llvm_set_error(error, XS_BACKEND_INVALID_ARGUMENT,
                                 "valid codegen unit and XLIL function signature are required");
    LLVMTypeRef lowered_return_type = nullptr;
    XsBackendStatus status = xs_llvm_codegen_lil_type(unit, return_type, &lowered_return_type, error);
    if(status != XS_BACKEND_OK)
        return status;
    LLVMTypeRef *lowered_parameters = nullptr;
    if(parameter_count != 0)
    {
        lowered_parameters = malloc(parameter_count * sizeof(*lowered_parameters));
        if(lowered_parameters == nullptr)
            return xs_llvm_set_error(error, XS_BACKEND_SYSTEM_ERROR,
                                     "out of memory while lowering XLIL function signature");
    }
    for(size_t i = 0; i < parameter_count; ++i)
    {
        status = xs_llvm_codegen_lil_type(unit, parameter_types[i], &lowered_parameters[i], error);
        if(status != XS_BACKEND_OK)
        {
            free(lowered_parameters);
            return status;
        }
        if(LLVMGetTypeKind(lowered_parameters[i]) == LLVMVoidTypeKind)
        {
            free(lowered_parameters);
            return xs_llvm_set_error(error, XS_BACKEND_INVALID_ARGUMENT,
                                     "XLIL function parameters cannot have void type");
        }
    }
    status = declare_function_with_types(unit, name, lowered_return_type, lowered_parameters, parameter_count, function,
                                         error);
    free(lowered_parameters);
    return status;
}

static bool is_lil_binary_integer(XsLilInstructionKind kind)
{
    return kind == XS_LIL_INSTRUCTION_ADD_I64 || kind == XS_LIL_INSTRUCTION_SUB_I64 ||
           kind == XS_LIL_INSTRUCTION_MUL_I64 || kind == XS_LIL_INSTRUCTION_DIV_I64 ||
           kind == XS_LIL_INSTRUCTION_REM_I64 || kind == XS_LIL_INSTRUCTION_EQ_I64 ||
           kind == XS_LIL_INSTRUCTION_AND_I64 || kind == XS_LIL_INSTRUCTION_OR_I64 ||
           kind == XS_LIL_INSTRUCTION_XOR_I64 || kind == XS_LIL_INSTRUCTION_SHL_I64 ||
           kind == XS_LIL_INSTRUCTION_SHR_I64 || kind == XS_LIL_INSTRUCTION_NE_I64 ||
           kind == XS_LIL_INSTRUCTION_LT_I64 || kind == XS_LIL_INSTRUCTION_LE_I64 ||
           kind == XS_LIL_INSTRUCTION_GT_I64 || kind == XS_LIL_INSTRUCTION_GE_I64 ||
           kind == XS_LIL_INSTRUCTION_ADD_I32 || kind == XS_LIL_INSTRUCTION_SUB_I32 ||
           kind == XS_LIL_INSTRUCTION_MUL_I32 || kind == XS_LIL_INSTRUCTION_DIV_I32 ||
           kind == XS_LIL_INSTRUCTION_REM_I32 || kind == XS_LIL_INSTRUCTION_AND_I32 ||
           kind == XS_LIL_INSTRUCTION_OR_I32 || kind == XS_LIL_INSTRUCTION_XOR_I32 ||
           kind == XS_LIL_INSTRUCTION_SHL_I32 || kind == XS_LIL_INSTRUCTION_SHR_I32 ||
           kind == XS_LIL_INSTRUCTION_EQ_I32 || kind == XS_LIL_INSTRUCTION_NE_I32 ||
           kind == XS_LIL_INSTRUCTION_LT_I32 || kind == XS_LIL_INSTRUCTION_LE_I32 ||
           kind == XS_LIL_INSTRUCTION_GT_I32 || kind == XS_LIL_INSTRUCTION_GE_I32;
}

static LLVMValueRef lower_lil_binary_integer_op(LLVMBuilderRef builder, XsLilInstructionKind kind, LLVMValueRef left,
                                                LLVMValueRef right)
{
    switch(kind)
    {
    case XS_LIL_INSTRUCTION_ADD_I64:
    case XS_LIL_INSTRUCTION_ADD_I32:
        return LLVMBuildAdd(builder, left, right, "add");
    case XS_LIL_INSTRUCTION_SUB_I64:
    case XS_LIL_INSTRUCTION_SUB_I32:
        return LLVMBuildSub(builder, left, right, "sub");
    case XS_LIL_INSTRUCTION_MUL_I64:
    case XS_LIL_INSTRUCTION_MUL_I32:
        return LLVMBuildMul(builder, left, right, "mul");
    case XS_LIL_INSTRUCTION_DIV_I64:
    case XS_LIL_INSTRUCTION_DIV_I32:
        return LLVMBuildSDiv(builder, left, right, "div");
    case XS_LIL_INSTRUCTION_REM_I64:
    case XS_LIL_INSTRUCTION_REM_I32:
        return LLVMBuildSRem(builder, left, right, "rem");
    case XS_LIL_INSTRUCTION_AND_I64:
    case XS_LIL_INSTRUCTION_AND_I32:
        return LLVMBuildAnd(builder, left, right, "and");
    case XS_LIL_INSTRUCTION_OR_I64:
    case XS_LIL_INSTRUCTION_OR_I32:
        return LLVMBuildOr(builder, left, right, "or");
    case XS_LIL_INSTRUCTION_XOR_I64:
    case XS_LIL_INSTRUCTION_XOR_I32:
        return LLVMBuildXor(builder, left, right, "xor");
    case XS_LIL_INSTRUCTION_SHL_I64:
    case XS_LIL_INSTRUCTION_SHL_I32:
        return LLVMBuildShl(builder, left, right, "shl");
    case XS_LIL_INSTRUCTION_SHR_I64:
    case XS_LIL_INSTRUCTION_SHR_I32:
        return LLVMBuildAShr(builder, left, right, "shr");
    case XS_LIL_INSTRUCTION_EQ_I64:
    case XS_LIL_INSTRUCTION_EQ_I32:
        return LLVMBuildICmp(builder, LLVMIntEQ, left, right, "eq");
    case XS_LIL_INSTRUCTION_NE_I64:
    case XS_LIL_INSTRUCTION_NE_I32:
        return LLVMBuildICmp(builder, LLVMIntNE, left, right, "ne");
    case XS_LIL_INSTRUCTION_LT_I64:
    case XS_LIL_INSTRUCTION_LT_I32:
        return LLVMBuildICmp(builder, LLVMIntSLT, left, right, "lt");
    case XS_LIL_INSTRUCTION_LE_I64:
    case XS_LIL_INSTRUCTION_LE_I32:
        return LLVMBuildICmp(builder, LLVMIntSLE, left, right, "le");
    case XS_LIL_INSTRUCTION_GT_I64:
    case XS_LIL_INSTRUCTION_GT_I32:
        return LLVMBuildICmp(builder, LLVMIntSGT, left, right, "gt");
    case XS_LIL_INSTRUCTION_GE_I64:
    case XS_LIL_INSTRUCTION_GE_I32:
        return LLVMBuildICmp(builder, LLVMIntSGE, left, right, "ge");
    default:
        return nullptr;
    }
}

static XsBackendStatus lower_lil_binary_integer(LLVMBuilderRef builder, const XsLilBlock *block, size_t index,
                                                LLVMValueRef *values, size_t value_count, XsBackendError *error)
{
    XsLilValueId result = xs_lil_block_instruction_result(block, index);
    XsLilValueId left = xs_lil_block_instruction_left(block, index);
    XsLilValueId right = xs_lil_block_instruction_right(block, index);
    if((size_t)result >= value_count || (size_t)left >= value_count || (size_t)right >= value_count ||
       values[left] == nullptr || values[right] == nullptr)
        return xs_llvm_set_error(error, XS_BACKEND_INVALID_ARGUMENT,
                                 "XLIL binary integer instruction references an unavailable value");
    LLVMValueRef lowered =
        lower_lil_binary_integer_op(builder, xs_lil_block_instruction_kind(block, index), values[left], values[right]);
    if(lowered == nullptr)
        return xs_llvm_set_error(error, XS_BACKEND_LLVM_ERROR, "LLVM could not lower XLIL binary integer instruction");
    values[result] = lowered;
    return XS_BACKEND_OK;
}

static bool is_lil_binary_float(XsLilInstructionKind kind)
{
    return kind >= XS_LIL_INSTRUCTION_ADD_F32 && kind <= XS_LIL_INSTRUCTION_GE_F64;
}

static LLVMValueRef lower_lil_binary_float_op(LLVMBuilderRef builder, XsLilInstructionKind kind, LLVMValueRef left,
                                              LLVMValueRef right)
{
    switch(kind)
    {
    case XS_LIL_INSTRUCTION_ADD_F32:
    case XS_LIL_INSTRUCTION_ADD_F64:
        return LLVMBuildFAdd(builder, left, right, "fadd");
    case XS_LIL_INSTRUCTION_SUB_F32:
    case XS_LIL_INSTRUCTION_SUB_F64:
        return LLVMBuildFSub(builder, left, right, "fsub");
    case XS_LIL_INSTRUCTION_MUL_F32:
    case XS_LIL_INSTRUCTION_MUL_F64:
        return LLVMBuildFMul(builder, left, right, "fmul");
    case XS_LIL_INSTRUCTION_DIV_F32:
    case XS_LIL_INSTRUCTION_DIV_F64:
        return LLVMBuildFDiv(builder, left, right, "fdiv");
    case XS_LIL_INSTRUCTION_REM_F32:
    case XS_LIL_INSTRUCTION_REM_F64:
        return LLVMBuildFRem(builder, left, right, "frem");
    case XS_LIL_INSTRUCTION_EQ_F32:
    case XS_LIL_INSTRUCTION_EQ_F64:
        return LLVMBuildFCmp(builder, LLVMRealOEQ, left, right, "feq");
    case XS_LIL_INSTRUCTION_NE_F32:
    case XS_LIL_INSTRUCTION_NE_F64:
        return LLVMBuildFCmp(builder, LLVMRealONE, left, right, "fne");
    case XS_LIL_INSTRUCTION_LT_F32:
    case XS_LIL_INSTRUCTION_LT_F64:
        return LLVMBuildFCmp(builder, LLVMRealOLT, left, right, "flt");
    case XS_LIL_INSTRUCTION_LE_F32:
    case XS_LIL_INSTRUCTION_LE_F64:
        return LLVMBuildFCmp(builder, LLVMRealOLE, left, right, "fle");
    case XS_LIL_INSTRUCTION_GT_F32:
    case XS_LIL_INSTRUCTION_GT_F64:
        return LLVMBuildFCmp(builder, LLVMRealOGT, left, right, "fgt");
    case XS_LIL_INSTRUCTION_GE_F32:
    case XS_LIL_INSTRUCTION_GE_F64:
        return LLVMBuildFCmp(builder, LLVMRealOGE, left, right, "fge");
    default:
        return nullptr;
    }
}

static XsBackendStatus lower_lil_binary_float(LLVMBuilderRef builder, const XsLilBlock *block, size_t index,
                                              LLVMValueRef *values, size_t value_count, XsBackendError *error)
{
    XsLilValueId result = xs_lil_block_instruction_result(block, index);
    XsLilValueId left = xs_lil_block_instruction_left(block, index);
    XsLilValueId right = xs_lil_block_instruction_right(block, index);
    if((size_t)result >= value_count || (size_t)left >= value_count || (size_t)right >= value_count ||
       values[left] == nullptr || values[right] == nullptr)
        return xs_llvm_set_error(error, XS_BACKEND_INVALID_ARGUMENT,
                                 "XLIL floating instruction references an unavailable value");
    values[result] =
        lower_lil_binary_float_op(builder, xs_lil_block_instruction_kind(block, index), values[left], values[right]);
    if(values[result] == nullptr)
        return xs_llvm_set_error(error, XS_BACKEND_LLVM_ERROR, "LLVM could not lower XLIL floating instruction");
    return XS_BACKEND_OK;
}

static XsBackendStatus lower_lil_instruction(XsLlvmCodegenUnit *unit, LLVMBuilderRef builder,
                                             const XsLilFunction *function, const XsLilBlock *block, size_t index,
                                             LLVMValueRef *values, size_t value_count, LLVMValueRef *slots,
                                             size_t slot_count, XsBackendError *error)
{
    XsLilInstructionKind kind = xs_lil_block_instruction_kind(block, index);
    XsLilValueId result = xs_lil_block_instruction_result(block, index);
    if(kind != XS_LIL_INSTRUCTION_CALL && kind != XS_LIL_INSTRUCTION_STORE && (size_t)result >= value_count)
        return xs_llvm_set_error(error, XS_BACKEND_INVALID_ARGUMENT,
                                 "XLIL instruction result references an unknown value");
    LLVMTypeRef type = nullptr;
    if(kind == XS_LIL_INSTRUCTION_CONST_I64)
    {
        XsBackendStatus status = xs_llvm_lil_type(unit->backend, (XsLilType){.kind = XS_LIL_TYPE_I64}, &type, error);
        if(status != XS_BACKEND_OK)
            return status;
        values[result] = LLVMConstInt(type, (unsigned long long)xs_lil_block_instruction_i64(block, index), true);
        return XS_BACKEND_OK;
    }
    if(kind == XS_LIL_INSTRUCTION_CONST_I32)
    {
        XsBackendStatus status = xs_llvm_lil_type(unit->backend, (XsLilType){.kind = XS_LIL_TYPE_I32}, &type, error);
        if(status != XS_BACKEND_OK)
            return status;
        values[result] = LLVMConstInt(type, (unsigned long long)xs_lil_block_instruction_i64(block, index), true);
        return XS_BACKEND_OK;
    }
    if(xs_llvm_is_integer_constant(kind))
    {
        return xs_llvm_lower_integer_constant(unit->backend, block, index, &values[result], error);
    }
    if(kind == XS_LIL_INSTRUCTION_CONST_F32 || kind == XS_LIL_INSTRUCTION_CONST_F64)
    {
        XsLilTypeKind float_kind = kind == XS_LIL_INSTRUCTION_CONST_F32 ? XS_LIL_TYPE_F32 : XS_LIL_TYPE_F64;
        XsBackendStatus status = xs_llvm_lil_type(unit->backend, (XsLilType){.kind = float_kind}, &type, error);
        if(status != XS_BACKEND_OK)
            return status;
        LLVMTypeRef bits_type = kind == XS_LIL_INSTRUCTION_CONST_F32 ? LLVMInt32TypeInContext(unit->backend->context)
                                                                     : LLVMInt64TypeInContext(unit->backend->context);
        LLVMValueRef bits = LLVMConstInt(bits_type, xs_lil_block_instruction_float_bits(block, index), false);
        values[result] = LLVMConstBitCast(bits, type);
        if(values[result] == nullptr)
            return xs_llvm_set_error(error, XS_BACKEND_LLVM_ERROR, "LLVM could not lower XLIL floating constant");
        return XS_BACKEND_OK;
    }
    if(kind == XS_LIL_INSTRUCTION_CONST_BOOL)
    {
        XsBackendStatus status = xs_llvm_lil_type(unit->backend, (XsLilType){.kind = XS_LIL_TYPE_BOOL}, &type, error);
        if(status != XS_BACKEND_OK)
            return status;
        values[result] = LLVMConstInt(type, xs_lil_block_instruction_bool(block, index) ? 1 : 0, false);
        return XS_BACKEND_OK;
    }
    if(kind == XS_LIL_INSTRUCTION_CONST_STR)
        return xs_llvm_lower_lil_const_str(unit->backend, unit, block, index, &values[result], error);
    if(kind == XS_LIL_INSTRUCTION_EQ_STR || kind == XS_LIL_INSTRUCTION_NE_STR)
    {
        XsBackendStatus status = xs_llvm_lower_lil_compare_str(unit, builder, block, index, values, value_count, error);
        if(status == XS_BACKEND_OK)
            values[result] =
                LLVMBuildZExt(builder, values[result], LLVMInt8TypeInContext(unit->backend->context), "bool");
        return status;
    }
    if(kind == XS_LIL_INSTRUCTION_NOT_BOOL)
    {
        XsLilValueId operand = xs_lil_block_instruction_left(block, index);
        if((size_t)operand >= value_count || values[operand] == nullptr)
            return xs_llvm_set_error(error, XS_BACKEND_INVALID_ARGUMENT,
                                     "XLIL not.bool references an unavailable value");
        values[result] = LLVMBuildXor(builder, values[operand],
                                      LLVMConstInt(LLVMInt8TypeInContext(unit->backend->context), 1, false), "not");
        if(values[result] == nullptr)
            return xs_llvm_set_error(error, XS_BACKEND_LLVM_ERROR, "LLVM could not lower XLIL not.bool instruction");
        return XS_BACKEND_OK;
    }
    if(is_lil_binary_integer(kind))
    {
        XsBackendStatus status = lower_lil_binary_integer(builder, block, index, values, value_count, error);
        XsLilType result_type = xs_lil_function_value_type(function, result);
        if(status == XS_BACKEND_OK && result_type.kind == XS_LIL_TYPE_BOOL)
            values[result] =
                LLVMBuildZExt(builder, values[result], LLVMInt8TypeInContext(unit->backend->context), "bool");
        return status;
    }
    if(kind == XS_LIL_INSTRUCTION_BINARY_INTEGER)
    {
        XsBackendStatus status = xs_llvm_lower_integer_operation(builder, block, index, values, value_count, error);
        XsLilType result_type = xs_lil_function_value_type(function, result);
        if(status == XS_BACKEND_OK && result_type.kind == XS_LIL_TYPE_BOOL)
            values[result] =
                LLVMBuildZExt(builder, values[result], LLVMInt8TypeInContext(unit->backend->context), "bool");
        return status;
    }
    if(is_lil_binary_float(kind))
    {
        XsBackendStatus status = lower_lil_binary_float(builder, block, index, values, value_count, error);
        XsLilType result_type = xs_lil_function_value_type(function, result);
        if(status == XS_BACKEND_OK && result_type.kind == XS_LIL_TYPE_BOOL)
            values[result] =
                LLVMBuildZExt(builder, values[result], LLVMInt8TypeInContext(unit->backend->context), "bool");
        return status;
    }
    if(kind == XS_LIL_INSTRUCTION_CALL)
    {
        const char *callee_name = xs_lil_block_instruction_callee(block, index);
        LLVMValueRef callee = LLVMGetNamedFunction(unit->module, callee_name);
        if(callee == nullptr)
            return xs_llvm_set_error(error, XS_BACKEND_INVALID_ARGUMENT,
                                     "XLIL call target must be declared before body lowering");
        size_t argument_count = xs_lil_block_instruction_argument_count(block, index);
        if(argument_count > (size_t)UINT_MAX)
            return xs_llvm_set_error(error, XS_BACKEND_INVALID_ARGUMENT,
                                     "XLIL call has too many arguments for LLVM C API");
        LLVMValueRef *arguments = calloc(argument_count == 0 ? 1 : argument_count, sizeof(*arguments));
        if(arguments == nullptr)
            return xs_llvm_set_error(error, XS_BACKEND_SYSTEM_ERROR,
                                     "out of memory while lowering XLIL call arguments");
        XsBackendStatus status = XS_BACKEND_OK;
        for(size_t argument = 0; argument < argument_count; ++argument)
        {
            XsLilValueId value = xs_lil_block_instruction_argument(block, index, argument);
            if((size_t)value >= value_count || values[value] == nullptr)
            {
                status = xs_llvm_set_error(error, XS_BACKEND_INVALID_ARGUMENT,
                                           "XLIL call references an unavailable argument value");
                goto call_cleanup;
            }
            arguments[argument] = values[value];
        }
        LLVMValueRef call = LLVMBuildCall2(builder, LLVMGlobalGetValueType(callee), callee, arguments,
                                           (unsigned)argument_count, result == UINT32_MAX ? "" : "call");
        if(call == nullptr)
        {
            status = xs_llvm_set_error(error, XS_BACKEND_LLVM_ERROR, "LLVM could not lower XLIL call instruction");
            goto call_cleanup;
        }
        if(result != UINT32_MAX)
        {
            if((size_t)result >= value_count)
            {
                status = xs_llvm_set_error(error, XS_BACKEND_INVALID_ARGUMENT,
                                           "XLIL call result references an unknown value");
                goto call_cleanup;
            }
            values[result] = call;
        }
    call_cleanup:
        free(arguments);
        return status;
    }
    if(kind == XS_LIL_INSTRUCTION_AGGREGATE || kind == XS_LIL_INSTRUCTION_EXTRACT ||
       kind == XS_LIL_INSTRUCTION_ARRAY_GET || kind == XS_LIL_INSTRUCTION_ARRAY_SET ||
       kind == XS_LIL_INSTRUCTION_ARRAY_LENGTH)
        return xs_llvm_lower_aggregate_instruction(unit, builder, function, block, index, values, value_count, error);
    if(kind == XS_LIL_INSTRUCTION_LOAD)
    {
        XsLilSlotId slot = xs_lil_block_instruction_slot(block, index);
        if((size_t)slot >= slot_count || slots[slot] == nullptr)
            return xs_llvm_set_error(error, XS_BACKEND_INVALID_ARGUMENT,
                                     "XLIL load references an unavailable stack slot");
        XsBackendStatus status =
            xs_llvm_codegen_lil_type(unit, xs_lil_function_value_type(function, result), &type, error);
        if(status != XS_BACKEND_OK)
            return status;
        values[result] = LLVMBuildLoad2(builder, type, slots[slot], "load");
        return values[result] == nullptr
                   ? xs_llvm_set_error(error, XS_BACKEND_LLVM_ERROR, "LLVM could not lower XLIL load")
                   : XS_BACKEND_OK;
    }
    if(kind == XS_LIL_INSTRUCTION_STORE)
    {
        XsLilSlotId slot = xs_lil_block_instruction_slot(block, index);
        XsLilValueId value = xs_lil_block_instruction_left(block, index);
        if((size_t)slot >= slot_count || slots[slot] == nullptr || (size_t)value >= value_count ||
           values[value] == nullptr)
            return xs_llvm_set_error(error, XS_BACKEND_INVALID_ARGUMENT,
                                     "XLIL store references an unavailable value or stack slot");
        return LLVMBuildStore(builder, values[value], slots[slot]) == nullptr
                   ? xs_llvm_set_error(error, XS_BACKEND_LLVM_ERROR, "LLVM could not lower XLIL store")
                   : XS_BACKEND_OK;
    }
    return xs_llvm_set_error(error, XS_BACKEND_DEFERRED, "XLIL instruction lowering is not supported yet");
}

static XsBackendStatus lower_lil_terminator(LLVMBuilderRef builder, const XsLilBlock *block, LLVMValueRef *values,
                                            size_t value_count, LLVMBasicBlockRef *blocks, size_t block_count,
                                            XsBackendError *error)
{
    switch(xs_lil_block_terminator_kind(block))
    {
    case XS_LIL_TERMINATOR_RETURN:
        if(xs_lil_block_terminator_has_value(block))
        {
            XsLilValueId value = xs_lil_block_terminator_value(block);
            if((size_t)value >= value_count || values[value] == nullptr)
                return xs_llvm_set_error(error, XS_BACKEND_INVALID_ARGUMENT,
                                         "XLIL return references an unavailable value");
            LLVMBuildRet(builder, values[value]);
        }
        else
        {
            LLVMBuildRetVoid(builder);
        }
        return XS_BACKEND_OK;
    case XS_LIL_TERMINATOR_BRANCH:
    {
        XsLilBlockId target = xs_lil_block_terminator_target(block);
        if((size_t)target >= block_count || blocks[target] == nullptr)
            return xs_llvm_set_error(error, XS_BACKEND_INVALID_ARGUMENT,
                                     "XLIL branch target references an unknown block");
        LLVMBuildBr(builder, blocks[target]);
        return XS_BACKEND_OK;
    }
    case XS_LIL_TERMINATOR_BRANCH_IF:
    {
        XsLilValueId condition = xs_lil_block_terminator_condition(block);
        XsLilBlockId then_block = xs_lil_block_terminator_then_block(block);
        XsLilBlockId else_block = xs_lil_block_terminator_else_block(block);
        if((size_t)condition >= value_count || values[condition] == nullptr)
            return xs_llvm_set_error(error, XS_BACKEND_INVALID_ARGUMENT,
                                     "XLIL branch_if condition references an unavailable value");
        if((size_t)then_block >= block_count || blocks[then_block] == nullptr || (size_t)else_block >= block_count ||
           blocks[else_block] == nullptr)
            return xs_llvm_set_error(error, XS_BACKEND_INVALID_ARGUMENT,
                                     "XLIL branch_if target references an unknown block");
        LLVMValueRef zero = LLVMConstInt(LLVMTypeOf(values[condition]), 0, false);
        LLVMValueRef predicate = LLVMBuildICmp(builder, LLVMIntNE, values[condition], zero, "branch.condition");
        LLVMBuildCondBr(builder, predicate, blocks[then_block], blocks[else_block]);
        return XS_BACKEND_OK;
    }
    case XS_LIL_TERMINATOR_PANIC:
    {
        LLVMModuleRef module = LLVMGetGlobalParent(LLVMGetBasicBlockParent(LLVMGetInsertBlock(builder)));
        LLVMTypeRef trap_type =
            LLVMFunctionType(LLVMVoidTypeInContext(LLVMGetModuleContext(module)), nullptr, 0, false);
        LLVMValueRef trap = LLVMGetNamedFunction(module, "llvm.trap");
        if(trap == nullptr)
            trap = LLVMAddFunction(module, "llvm.trap", trap_type);
        LLVMBuildCall2(builder, trap_type, trap, nullptr, 0, "");
        LLVMBuildUnreachable(builder);
        return XS_BACKEND_OK;
    }
    case XS_LIL_TERMINATOR_NONE:
        return xs_llvm_set_error(error, XS_BACKEND_INVALID_ARGUMENT, "XLIL block is missing a terminator");
    }
    return xs_llvm_set_error(error, XS_BACKEND_INVALID_ARGUMENT, "unknown XLIL terminator");
}

XsBackendStatus xs_llvm_lower_lil_function_body(XsLlvmCodegenUnit *unit, const XsLilFunction *function,
                                                XsBackendError *error)
{
    xs_llvm_clear_error(error);
    if(unit == nullptr || function == nullptr)
        return xs_llvm_set_error(error, XS_BACKEND_INVALID_ARGUMENT,
                                 "valid codegen unit and XLIL function are required");
    if(!xs_lil_function_is_definition(function))
        return XS_BACKEND_OK;
    const char *name = xs_lil_function_name(function);
    LLVMValueRef llvm_function = LLVMGetNamedFunction(unit->module, name);
    if(llvm_function == nullptr)
        return xs_llvm_set_error(error, XS_BACKEND_INVALID_ARGUMENT,
                                 "XLIL function must be declared before body lowering");
    if(LLVMCountBasicBlocks(llvm_function) != 0)
        return xs_llvm_set_error(error, XS_BACKEND_INVALID_ARGUMENT, "LLVM function body already exists");

    size_t block_count = xs_lil_function_block_count(function);
    size_t value_count = xs_lil_function_value_count(function);
    size_t slot_count = xs_lil_function_slot_count(function);
    if(block_count > (size_t)UINT_MAX || value_count > (size_t)UINT_MAX || slot_count > (size_t)UINT_MAX)
        return xs_llvm_set_error(error, XS_BACKEND_INVALID_ARGUMENT, "XLIL function body is too large for LLVM C API");
    LLVMBasicBlockRef *blocks = calloc(block_count == 0 ? 1 : block_count, sizeof(*blocks));
    LLVMValueRef *values = calloc(value_count == 0 ? 1 : value_count, sizeof(*values));
    LLVMValueRef *slots = calloc(slot_count == 0 ? 1 : slot_count, sizeof(*slots));
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(unit->backend->context);
    if(blocks == nullptr || values == nullptr || slots == nullptr || builder == nullptr)
    {
        free(blocks);
        free(values);
        free(slots);
        if(builder != nullptr)
            LLVMDisposeBuilder(builder);
        return xs_llvm_set_error(error, XS_BACKEND_SYSTEM_ERROR, "out of memory while lowering XLIL function body");
    }

    XsBackendStatus status = XS_BACKEND_OK;
    size_t parameter_count = xs_lil_function_parameter_count(function);
    for(size_t parameter = 0; parameter < parameter_count; ++parameter)
    {
        if(parameter >= value_count)
        {
            status =
                xs_llvm_set_error(error, XS_BACKEND_INVALID_ARGUMENT, "XLIL function parameter value is unavailable");
            goto cleanup;
        }
        values[parameter] = LLVMGetParam(llvm_function, (unsigned)parameter);
        if(values[parameter] == nullptr)
        {
            status = xs_llvm_set_error(error, XS_BACKEND_LLVM_ERROR, "LLVM could not read function parameter");
            goto cleanup;
        }
    }
    for(size_t i = 0; i < block_count; ++i)
    {
        const XsLilBlock *block = xs_lil_function_block_at(function, i);
        if(xs_lil_block_id(block) != (XsLilBlockId)i)
        {
            status = xs_llvm_set_error(error, XS_BACKEND_INVALID_ARGUMENT, "XLIL block ids must be sequential");
            goto cleanup;
        }
        blocks[i] = LLVMAppendBasicBlockInContext(unit->backend->context, llvm_function, xs_lil_block_label(block));
        if(blocks[i] == nullptr)
        {
            status = xs_llvm_set_error(error, XS_BACKEND_LLVM_ERROR, "LLVM could not create a basic block");
            goto cleanup;
        }
    }
    if(slot_count != 0)
    {
        if(block_count == 0)
        {
            status = xs_llvm_set_error(error, XS_BACKEND_INVALID_ARGUMENT, "XLIL stack slots require an entry block");
            goto cleanup;
        }
        LLVMPositionBuilderAtEnd(builder, blocks[0]);
        for(size_t slot = 0; slot < slot_count; ++slot)
        {
            LLVMTypeRef slot_type = nullptr;
            status = xs_llvm_codegen_lil_type(unit, xs_lil_function_slot_type(function, (XsLilSlotId)slot), &slot_type,
                                              error);
            if(status != XS_BACKEND_OK)
                goto cleanup;
            slots[slot] = LLVMBuildAlloca(builder, slot_type, "slot");
            if(slots[slot] == nullptr)
            {
                status = xs_llvm_set_error(error, XS_BACKEND_LLVM_ERROR, "LLVM could not allocate XLIL stack slot");
                goto cleanup;
            }
        }
    }
    for(size_t i = 0; i < block_count; ++i)
    {
        const XsLilBlock *block = xs_lil_function_block_at(function, i);
        LLVMPositionBuilderAtEnd(builder, blocks[i]);
        size_t instruction_count = xs_lil_block_instruction_count(block);
        for(size_t instruction = 0; instruction < instruction_count; ++instruction)
        {
            status = lower_lil_instruction(unit, builder, function, block, instruction, values, value_count, slots,
                                           slot_count, error);
            if(status != XS_BACKEND_OK)
                goto cleanup;
        }
        status = lower_lil_terminator(builder, block, values, value_count, blocks, block_count, error);
        if(status != XS_BACKEND_OK)
            goto cleanup;
    }

cleanup:
    LLVMDisposeBuilder(builder);
    free(blocks);
    free(values);
    free(slots);
    return status;
}
