/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

#ifndef XS_LLVM_STRING_H
#define XS_LLVM_STRING_H

#include "Visual/XSharp/Legacy/backend/llvm_backend.h"

XsBackendStatus
xs_llvm_lower_lil_const_str(XsLlvmBackend *backend, XsLlvmCodegenUnit *unit, const XsLilBlock *block, size_t index, LLVMValueRef *value, XsBackendError *error);
XsBackendStatus
xs_llvm_lower_lil_compare_str(XsLlvmCodegenUnit *unit, LLVMBuilderRef builder, const XsLilBlock *block, size_t index, LLVMValueRef *values, size_t value_count, XsBackendError *error);

#endif
