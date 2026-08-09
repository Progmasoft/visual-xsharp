/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "direct_xlil.h"
#include "native_artifact.h"

#include "xs/backend/linker.h"
#include "xs/backend/llvm_backend.h"

#include <llvm-c/TargetMachine.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool validate_native_entry(const XsLilModule *module, XsBackendError *error)
{
    const XsLilFunction *entry = nullptr;
    size_t entry_count = 0;
    size_t function_count = xs_lil_module_function_count(module);
    for(size_t i = 0; i < function_count; ++i)
    {
        const XsLilFunction *function = xs_lil_module_function_at(module, i);
        if(strcmp(xs_lil_function_name(function), "main") != 0)
            continue;
        entry = function;
        ++entry_count;
    }
    if(entry_count != 1)
    {
        error->status = XS_BACKEND_INVALID_ARGUMENT;
        snprintf(error->message, sizeof(error->message), "%s",
                 "direct XLIL native build requires exactly one '.func main : () -> i32' definition");
        return false;
    }
    if(!xs_lil_function_is_definition(entry) || xs_lil_function_parameter_count(entry) != 0 ||
       xs_lil_function_return_type(entry).kind != XS_LIL_TYPE_I32)
    {
        error->status = XS_BACKEND_INVALID_ARGUMENT;
        snprintf(error->message, sizeof(error->message), "%s",
                 "direct XLIL entry 'main' must be a definition with signature '() -> i32'");
        return false;
    }
    return true;
}

static bool target_is_native_host(const XsLlvmBackend *backend)
{
    char *host_triple = LLVMGetDefaultTargetTriple();
    if(host_triple == nullptr)
        return false;
    bool matches = strcmp(xs_llvm_backend_target_triple(backend), host_triple) == 0;
    LLVMDisposeMessage(host_triple);
    return matches;
}

static bool link_native_executable(const char *object_path, const char *executable_path, XsBackendError *error)
{
#ifdef _WIN32
    size_t option_length = strlen(executable_path) + sizeof("/Fe:");
    char *output_option = malloc(option_length);
    if(output_option == nullptr)
    {
        error->status = XS_BACKEND_SYSTEM_ERROR;
        snprintf(error->message, sizeof(error->message), "%s", "out of memory while preparing linker arguments");
        return false;
    }
    (void)snprintf(output_option, option_length, "/Fe:%s", executable_path);
    const char *arguments[] = {"-fuse-ld=lld", object_path, "libcmt.lib", "oldnames.lib", "kernel32.lib",
                               output_option};
#else
    const char *arguments[] = {"-fuse-ld=lld", object_path, "-lm", "-o", executable_path};
#endif
    XsLinkerInvocation invocation = {
        .program = XS_CLANG_EXECUTABLE,
        .arguments = arguments,
        .argument_count = sizeof(arguments) / sizeof(*arguments),
    };
    int exit_code = -1;
    XsBackendStatus status = xs_linker_invoke(&invocation, &exit_code, error);
#ifdef _WIN32
    free(output_option);
#endif
    if(status != XS_BACKEND_OK)
        return false;
    if(exit_code == 0)
        return true;
    error->status = XS_BACKEND_SYSTEM_ERROR;
    snprintf(error->message, sizeof(error->message), "ClangCL and LLD exited with status %d", exit_code);
    return false;
}

static bool declare_module_functions(XsLlvmCodegenUnit *unit, const XsLilModule *module, size_t *declared,
                                     XsBackendError *error)
{
    *declared = 0;
    size_t function_count = xs_lil_module_function_count(module);
    for(size_t i = 0; i < function_count; ++i)
    {
        const XsLilFunction *function = xs_lil_module_function_at(module, i);
        const char *name = xs_lil_function_name(function);
        size_t parameter_count = xs_lil_function_parameter_count(function);
        XsLilType *parameters = nullptr;
        if(parameter_count != 0)
        {
            parameters = malloc(parameter_count * sizeof(*parameters));
            if(parameters == nullptr)
            {
                error->status = XS_BACKEND_SYSTEM_ERROR;
                snprintf(error->message, sizeof(error->message), "%s",
                         "out of memory while preparing XLIL function declaration");
                return false;
            }
        }
        for(size_t parameter = 0; parameter < parameter_count; ++parameter)
            parameters[parameter] = xs_lil_function_parameter_type(function, parameter);
        LLVMValueRef llvm_function = nullptr;
        XsBackendStatus status = xs_llvm_declare_lil_function(unit, name, xs_lil_function_return_type(function),
                                                              parameters, parameter_count, &llvm_function, error);
        free(parameters);
        if(status != XS_BACKEND_OK)
            return false;
        ++*declared;
    }
    return true;
}

static bool lower_module_function_bodies(XsLlvmCodegenUnit *unit, const XsLilModule *module, size_t *lowered,
                                         XsBackendError *error)
{
    *lowered = 0;
    size_t function_count = xs_lil_module_function_count(module);
    for(size_t i = 0; i < function_count; ++i)
    {
        const XsLilFunction *function = xs_lil_module_function_at(module, i);
        if(!xs_lil_function_is_definition(function))
            continue;
        XsBackendStatus status = xs_llvm_lower_lil_function_body(unit, function, error);
        if(status != XS_BACKEND_OK)
            return false;
        ++*lowered;
    }
    return true;
}

bool xs_driver_build_lil_module_native(const char *input_path, const XsLilModule *module)
{
    if(module == nullptr)
    {
        fprintf(stderr, "vxs: XLIL module is required for native build\n");
        return false;
    }
    char *ir_path = xs_driver_native_artifact_path(input_path, ".ll");
    char *object_path = xs_driver_native_artifact_path(input_path, ".obj");
    char *executable_path = xs_driver_native_artifact_path(input_path, ".vxse");
    if(ir_path == nullptr || object_path == nullptr || executable_path == nullptr)
    {
        free(ir_path);
        free(object_path);
        free(executable_path);
        fprintf(stderr, "vxs: out of memory while preparing direct XLIL artifact paths\n");
        return false;
    }
    XsBackendError error = {0};
    XsLlvmBackend *backend = nullptr;
    XsLlvmCodegenUnit *unit = nullptr;
    XsLlvmBackendConfig config = {
#ifdef XS_CONFIGURED_TARGET_TRIPLE
        .target_triple = XS_CONFIGURED_TARGET_TRIPLE,
#endif
        .optimization = XS_LLVM_OPT_NONE,
        .verify_modules = true,
    };
    const char *stage = "create LLVM backend";
    bool success = xs_llvm_backend_create(&config, &backend, &error) == XS_BACKEND_OK;
    if(success)
    {
        stage = "validate direct XLIL entry";
        success = validate_native_entry(module, &error);
    }
    if(success)
    {
        stage = "create LLVM codegen unit";
        success = xs_llvm_codegen_unit_create(backend, xs_lil_module_name(module), &unit, &error) == XS_BACKEND_OK;
    }
    size_t declared = 0;
    if(success)
    {
        stage = "register XLIL aggregate types";
        success = xs_llvm_register_lil_types(unit, module, &error) == XS_BACKEND_OK;
    }
    if(success)
    {
        stage = "lower XLIL declarations to LLVM";
        success = declare_module_functions(unit, module, &declared, &error);
    }
    size_t lowered = 0;
    if(success)
    {
        stage = "lower XLIL function bodies to LLVM";
        success = lower_module_function_bodies(unit, module, &lowered, &error);
    }
    if(success)
    {
        stage = "verify and optimize LLVM module";
        success = xs_llvm_optimize_codegen_unit(unit, &error) == XS_BACKEND_OK;
    }
    if(success)
    {
        stage = "write LLVM IR";
        success = xs_llvm_write_ir_file(unit, ir_path, &error) == XS_BACKEND_OK;
    }
    if(success)
    {
        stage = "emit object file";
        success = xs_llvm_emit_object_file(unit, object_path, &error) == XS_BACKEND_OK;
    }
    if(success && !target_is_native_host(backend))
    {
        stage = "link native executable";
        error.status = XS_BACKEND_DEFERRED;
        snprintf(error.message, sizeof(error.message), "%s",
                 "object file was emitted, but direct XLIL executable linking supports only the native host target");
        success = false;
    }
    if(success)
    {
        stage = "link native executable";
        success = link_native_executable(object_path, executable_path, &error);
    }
    if(success)
        fprintf(stderr,
                "vxs: wrote optimized LLVM IR '%s', object '%s', and executable '%s' from XLIL module '%s' with %zu "
                "declaration(s), %zu body/bodies\n",
                ir_path, object_path, executable_path, xs_lil_module_name(module), declared, lowered);
    else
        fprintf(stderr, "vxs: direct XLIL native build failed during %s: %s\n", stage, error.message);
    xs_llvm_codegen_unit_destroy(unit);
    xs_llvm_backend_destroy(backend);
    free(ir_path);
    free(object_path);
    free(executable_path);
    return success;
}

bool xs_driver_build_direct_xlil(const char *input_path, const char *text, size_t length)
{
    XsLilError lil_error = {0};
    XsLilModule *module = nullptr;
    if(xs_lil_module_parse_text(input_path, text, length, &module, &lil_error) != XS_LIL_OK)
    {
        fprintf(stderr, "vxs: XLIL parse failed: %s\n", lil_error.message);
        return false;
    }
    bool success = xs_driver_build_lil_module_native(input_path, module);
    xs_lil_module_destroy(module);
    return success;
}
