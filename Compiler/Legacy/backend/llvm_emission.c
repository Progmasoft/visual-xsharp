/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

#include "llvm_internal.h"

#include <llvm-c/Analysis.h>
#include <llvm-c/Transforms/PassBuilder.h>

#include <stdlib.h>

static XsBackendStatus verify_module(XsLlvmCodegenUnit *unit, XsBackendError *error)
{
    char *llvm_error = nullptr;
    if(LLVMVerifyModule(unit->module, LLVMReturnStatusAction, &llvm_error) == 0)
    {
        if(llvm_error != nullptr)
            LLVMDisposeMessage(llvm_error);
        return XS_BACKEND_OK;
    }
    XsBackendStatus status = xs_llvm_set_error(error, XS_BACKEND_LLVM_ERROR, llvm_error);
    LLVMDisposeMessage(llvm_error);
    return status;
}

XsBackendStatus xs_llvm_optimize_codegen_unit(XsLlvmCodegenUnit *unit, XsBackendError *error)
{
    xs_llvm_clear_error(error);
    if(unit == nullptr)
        return xs_llvm_set_error(error, XS_BACKEND_INVALID_ARGUMENT, "codegen unit is required");
    if(unit->backend->verify_modules)
    {
        XsBackendStatus status = verify_module(unit, error);
        if(status != XS_BACKEND_OK)
            return status;
    }
    static const char *const pipelines[] = {"default<O0>", "default<O1>", "default<O2>", "default<O3>"};
    LLVMPassBuilderOptionsRef options = LLVMCreatePassBuilderOptions();
    LLVMErrorRef llvm_error =
        LLVMRunPasses(unit->module, pipelines[unit->backend->optimization], unit->backend->target_machine, options);
    LLVMDisposePassBuilderOptions(options);
    if(llvm_error != nullptr)
    {
        char *message = LLVMGetErrorMessage(llvm_error);
        XsBackendStatus status = xs_llvm_set_error(error, XS_BACKEND_LLVM_ERROR, message);
        LLVMDisposeErrorMessage(message);
        return status;
    }
    return XS_BACKEND_OK;
}

XsBackendStatus xs_llvm_write_ir_file(XsLlvmCodegenUnit *unit, const char *path, XsBackendError *error)
{
    xs_llvm_clear_error(error);
    if(unit == nullptr || path == nullptr || path[0] == '\0')
        return xs_llvm_set_error(error, XS_BACKEND_INVALID_ARGUMENT, "codegen unit and LLVM IR path are required");
    if(unit->backend->verify_modules)
    {
        XsBackendStatus status = verify_module(unit, error);
        if(status != XS_BACKEND_OK)
            return status;
    }
    char *llvm_error = nullptr;
    if(LLVMPrintModuleToFile(unit->module, path, &llvm_error) != 0)
    {
        XsBackendStatus status = xs_llvm_set_error(error, XS_BACKEND_LLVM_ERROR, llvm_error);
        LLVMDisposeMessage(llvm_error);
        return status;
    }
    return XS_BACKEND_OK;
}

XsBackendStatus xs_llvm_emit_object_file(XsLlvmCodegenUnit *unit, const char *path, XsBackendError *error)
{
    xs_llvm_clear_error(error);
    if(unit == nullptr || path == nullptr || path[0] == '\0')
        return xs_llvm_set_error(error, XS_BACKEND_INVALID_ARGUMENT, "codegen unit and object-file path are required");
    if(unit->backend->verify_modules)
    {
        XsBackendStatus status = verify_module(unit, error);
        if(status != XS_BACKEND_OK)
            return status;
    }
    char *mutable_path = xs_llvm_copy_text(path);
    if(mutable_path == nullptr)
        return xs_llvm_set_error(error, XS_BACKEND_SYSTEM_ERROR, "out of memory while preparing object-file path");
    char *llvm_error = nullptr;
    int failed = LLVMTargetMachineEmitToFile(unit->backend->target_machine, unit->module, mutable_path, LLVMObjectFile,
                                             &llvm_error);
    free(mutable_path);
    if(failed != 0)
    {
        XsBackendStatus status = xs_llvm_set_error(error, XS_BACKEND_LLVM_ERROR, llvm_error);
        LLVMDisposeMessage(llvm_error);
        return status;
    }
    return XS_BACKEND_OK;
}
