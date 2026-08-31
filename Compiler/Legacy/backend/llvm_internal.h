/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

#ifndef XS_BACKEND_LLVM_INTERNAL_H
#define XS_BACKEND_LLVM_INTERNAL_H

#include "Visual/XSharp/Legacy/backend/llvm_backend.h"

struct XsLlvmBackend
{
    LLVMContextRef context;
    LLVMTargetMachineRef target_machine;
    LLVMTargetDataRef target_data;
    char *target_triple;
    char *data_layout;
    XsLlvmOptimizationLevel optimization;
    bool verify_modules;
};

struct XsLlvmCodegenUnit
{
    XsLlvmBackend *backend;
    LLVMModuleRef module;
    LLVMTypeRef *lil_types;
    size_t lil_type_count;
    LLVMTypeRef *lil_array_types;
    LLVMTypeRef *lil_array_elements;
    bool *lil_array_dynamic;
    size_t lil_array_type_count;
};

void
xs_llvm_clear_error(XsBackendError *error);
XsBackendStatus
xs_llvm_set_error(XsBackendError *error, XsBackendStatus status, const char *message);
char *
xs_llvm_copy_text(const char *text);
XsBackendStatus
xs_llvm_codegen_lil_type(XsLlvmCodegenUnit *unit, XsLilType type, LLVMTypeRef *llvm_type, XsBackendError *error);
XsBackendStatus
xs_llvm_lower_aggregate_instruction(XsLlvmCodegenUnit *unit, LLVMBuilderRef builder, const XsLilFunction *function, const XsLilBlock *block, size_t index, LLVMValueRef *values, size_t value_count, XsBackendError *error);

#endif
