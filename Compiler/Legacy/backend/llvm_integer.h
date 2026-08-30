/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

#ifndef XS_BACKEND_LLVM_INTEGER_H
#define XS_BACKEND_LLVM_INTEGER_H

#include "Visual/XSharp/backend/llvm_backend.h"

bool
xs_llvm_is_integer_constant(XsLilInstructionKind kind);
XsBackendStatus
xs_llvm_lower_integer_constant(XsLlvmBackend *backend, const XsLilBlock *block, size_t index, LLVMValueRef *value, XsBackendError *error);
XsBackendStatus
xs_llvm_lower_integer_operation(LLVMBuilderRef builder, const XsLilBlock *block, size_t index, LLVMValueRef *values, size_t value_count, XsBackendError *error);

#endif
