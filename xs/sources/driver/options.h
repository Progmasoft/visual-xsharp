/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef XS_DRIVER_OPTIONS_H
#define XS_DRIVER_OPTIONS_H

#include "Visual/C23/diagnostic.hh"

#include <stdio.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum
    {
        XS_BUILD_OUTPUT_BINARY,
        XS_BUILD_OUTPUT_NONE = XS_BUILD_OUTPUT_BINARY,
        XS_BUILD_OUTPUT_OBJECT,
        XS_BUILD_OUTPUT_CORE,
        XS_BUILD_OUTPUT_XPP,
        XS_BUILD_OUTPUT_XMM,
        XS_BUILD_OUTPUT_ASSEMBLY,
        XS_BUILD_OUTPUT_LLVM_LL,
        XS_BUILD_OUTPUT_LLVM_BC,
    } XsBuildOutput;

    typedef enum
    {
        XS_BUILD_INPUT_VXS,
        XS_BUILD_INPUT_OBJECT,
        XS_BUILD_INPUT_CORE,
        XS_BUILD_INPUT_XPP,
        XS_BUILD_INPUT_XMM,
        XS_BUILD_INPUT_LLVM_LL,
        XS_BUILD_INPUT_LLVM_BC,
    } XsBuildInput;

    typedef enum
    {
        XS_LLVM_OPT_0,
        XS_LLVM_OPT_1,
        XS_LLVM_OPT_2,
        XS_LLVM_OPT_3,
        XS_LLVM_OPT_G,
    } XsLlvmOptLevel;

    typedef enum
    {
        XS_LLVM_COMPILER_AOT,
        XS_LLVM_COMPILER_ORC,
    } XsLlvmCompiler;

    typedef enum
    {
        XS_LLVM_LTO_NONE,
        XS_LLVM_LTO_FAT,
        XS_LLVM_LTO_THIN,
    } XsLlvmLto;

    typedef struct
    {
        XsWarningLevel warning_level;
        bool warnings_as_errors;
        bool experimental_warnings;
        bool shadow_warnings;
        bool undefined_warnings;
        bool type_safe_format;
        bool xpp_optimization_passes;
        bool xmm_optimization_passes;
        XsLlvmOptLevel llvm_opt_level;
        XsLlvmCompiler llvm_compiler;
        XsLlvmLto llvm_lto;
    } XsCompilerSettings;

    typedef struct
    {
        const char *command;
        const char *file_path;
        const char *package_name;
        const char *compiler_version;
        const char *standard;
        XsBuildOutput output;
        XsBuildInput input;
        XsCompilerSettings compiler;
        bool global_install;
        bool output_override;
        bool warning_override;
        bool werror_override;
        bool experimental_override;
        bool shadow_override;
        bool undef_override;
        bool type_safe_format_override;
        bool xpp_optimization_override;
        bool xmm_optimization_override;
        bool llvm_opt_override;
        bool llvm_compiler_override;
        bool llvm_lto_override;
    } XsCliOptions;

    typedef enum
    {
        XS_CLI_PARSE_ERROR,
        XS_CLI_PARSE_READY,
        XS_CLI_PARSE_EXIT,
    } XsCliParseResult;

    XsCompilerSettings xs_cli_default_compiler_settings(void);
    void xs_cli_apply_compiler_overrides(const XsCliOptions *options, XsCompilerSettings *settings);
    const char *xs_cli_warning_level_name(XsWarningLevel level);
    const char *xs_cli_output_extension(XsBuildOutput output);
    XsCliParseResult xs_cli_parse(int argc, char **argv, XsCliOptions *options);
    void xs_cli_options_free(XsCliOptions *options);

#ifdef __cplusplus
}
#endif

#endif
