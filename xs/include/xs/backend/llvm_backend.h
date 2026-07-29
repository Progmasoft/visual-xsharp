/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef XS_BACKEND_LLVM_BACKEND_H
#define XS_BACKEND_LLVM_BACKEND_H

#include <llvm-c/Core.h>
#include <llvm-c/TargetMachine.h>

#include "xs/lil.h"

#include <stddef.h>

typedef enum
{
  XS_BACKEND_OK,
  XS_BACKEND_DEFERRED,
  XS_BACKEND_INVALID_ARGUMENT,
  XS_BACKEND_LLVM_ERROR,
  XS_BACKEND_SYSTEM_ERROR,
} XsBackendStatus;

typedef struct
{
  XsBackendStatus status;
  char message[512];
} XsBackendError;

typedef enum
{
  XS_LLVM_OPT_NONE,
  XS_LLVM_OPT_LESS,
  XS_LLVM_OPT_DEFAULT,
  XS_LLVM_OPT_AGGRESSIVE,
} XsLlvmOptimizationLevel;

typedef struct
{
  const char *target_triple;
  const char *cpu;
  const char *features;
  XsLlvmOptimizationLevel optimization;
  bool verify_modules;
} XsLlvmBackendConfig;

typedef enum
{
  XS_PRIMITIVE_UNIT,
  XS_PRIMITIVE_BOOL,
  XS_PRIMITIVE_BYTE,
  XS_PRIMITIVE_SBYTE,
  XS_PRIMITIVE_CHAR,
  XS_PRIMITIVE_STR,
  XS_PRIMITIVE_SHORT,
  XS_PRIMITIVE_LONG,
  XS_PRIMITIVE_INT,
  XS_PRIMITIVE_INTEGER,
  XS_PRIMITIVE_USHORT,
  XS_PRIMITIVE_ULONG,
  XS_PRIMITIVE_UINT,
  XS_PRIMITIVE_UINTEGER,
  XS_PRIMITIVE_SFLOAT,
  XS_PRIMITIVE_LFLOAT,
  XS_PRIMITIVE_FLOAT,
  XS_PRIMITIVE_DOUBLE,
  XS_PRIMITIVE_STRING,
} XsPrimitiveType;

typedef struct
{
  const char *name;
  XsPrimitiveType return_type;
  const XsPrimitiveType *parameter_types;
  size_t parameter_count;
} XsFunctionSignature;

typedef struct XsLlvmBackend XsLlvmBackend;
typedef struct XsLlvmCodegenUnit XsLlvmCodegenUnit;

XsBackendStatus xs_llvm_backend_create(const XsLlvmBackendConfig *config, XsLlvmBackend **backend,
                                       XsBackendError *error);
void xs_llvm_backend_destroy(XsLlvmBackend *backend);
LLVMContextRef xs_llvm_backend_context(const XsLlvmBackend *backend);
const char *xs_llvm_backend_target_triple(const XsLlvmBackend *backend);
const char *xs_llvm_backend_data_layout(const XsLlvmBackend *backend);

XsBackendStatus xs_llvm_codegen_unit_create(XsLlvmBackend *backend, const char *name, XsLlvmCodegenUnit **unit,
                                            XsBackendError *error);
void xs_llvm_codegen_unit_destroy(XsLlvmCodegenUnit *unit);
LLVMModuleRef xs_llvm_codegen_unit_module(const XsLlvmCodegenUnit *unit);

XsBackendStatus xs_llvm_primitive_type(XsLlvmBackend *backend, XsPrimitiveType primitive, LLVMTypeRef *type,
                                       XsBackendError *error);
XsBackendStatus xs_llvm_lil_type(XsLlvmBackend *backend, XsLilType type, LLVMTypeRef *llvm_type, XsBackendError *error);
XsBackendStatus xs_llvm_register_lil_types(XsLlvmCodegenUnit *unit, const XsLilModule *module, XsBackendError *error);
XsBackendStatus xs_llvm_declare_function(XsLlvmCodegenUnit *unit, const XsFunctionSignature *signature,
                                         LLVMValueRef *function, XsBackendError *error);
XsBackendStatus xs_llvm_declare_lil_function(XsLlvmCodegenUnit *unit, const char *name, XsLilType return_type,
                                             const XsLilType *parameter_types, size_t parameter_count,
                                             LLVMValueRef *function, XsBackendError *error);
XsBackendStatus xs_llvm_lower_lil_function_body(XsLlvmCodegenUnit *unit, const XsLilFunction *function,
                                                XsBackendError *error);
XsBackendStatus xs_llvm_optimize_codegen_unit(XsLlvmCodegenUnit *unit, XsBackendError *error);
XsBackendStatus xs_llvm_write_ir_file(XsLlvmCodegenUnit *unit, const char *path, XsBackendError *error);
XsBackendStatus xs_llvm_emit_object_file(XsLlvmCodegenUnit *unit, const char *path, XsBackendError *error);

#endif
