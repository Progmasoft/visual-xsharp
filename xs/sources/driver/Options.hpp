// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

// Raw argv spellings are consumed by the C++20 parser and become this typed
// driver model. Strings remain only for values that are intrinsically textual.
enum XsCliCommand : std::uint8_t
{
    XS_CLI_COMMAND_NONE,
    XS_CLI_COMMAND_BUILD,
    XS_CLI_COMMAND_CHECK,
    XS_CLI_COMMAND_INSTALL,
    XS_CLI_COMMAND_RESOLVE,
    XS_CLI_COMMAND_RUN,
    XS_CLI_COMMAND_TEST,
    XS_CLI_COMMAND_UPDATE,
    XS_CLI_COMMAND_VERSION,
    XS_CLI_COMMAND_VIGET,
};

enum XsViGetAction : std::uint8_t
{
    XS_VIGET_ACTION_NONE,
    XS_VIGET_ACTION_PUSH,
    XS_VIGET_ACTION_UPDATE,
};

enum XsWarningLevel : std::uint8_t
{
    XS_WARNING_ALL,
    XS_WARNING_MEDIUM,
    XS_WARNING_LOW,
    XS_WARNING_NONE,
};

enum XsBuildOutput : std::uint8_t
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
};

enum XsBuildInput : std::uint8_t
{
    XS_BUILD_INPUT_VXS,
    XS_BUILD_INPUT_OBJECT,
    XS_BUILD_INPUT_CORE,
    XS_BUILD_INPUT_XPP,
    XS_BUILD_INPUT_XMM,
    XS_BUILD_INPUT_LLVM_LL,
    XS_BUILD_INPUT_LLVM_BC,
};

enum XsLlvmOptLevel : std::uint8_t
{
    XS_LLVM_OPT_0,
    XS_LLVM_OPT_1,
    XS_LLVM_OPT_2,
    XS_LLVM_OPT_3,
    XS_LLVM_OPT_G,
};

enum XsLlvmCompiler : std::uint8_t
{
    XS_LLVM_COMPILER_AOT,
    XS_LLVM_COMPILER_ORC,
};

enum XsLlvmLto : std::uint8_t
{
    XS_LLVM_LTO_NONE,
    XS_LLVM_LTO_FAT,
    XS_LLVM_LTO_THIN,
};

struct XsCompilerSettings
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
};

struct XsCliOptions
{
    XsCliCommand command;
    XsViGetAction vigetAction;
    std::optional<std::filesystem::path> filePath;
    std::optional<std::string> packageCoordinate;
    std::optional<std::string> target;
    std::string compilerVersion;
    std::string standard;
    XsBuildOutput output;
    XsBuildInput input;
    XsCompilerSettings compiler;
    bool globalInstall;
    bool compilerVersionOverride;
    bool standardOverride;
    bool targetOverride;
    bool outputOverride;
    bool warningOverride;
    bool werrorOverride;
    bool experimentalOverride;
    bool shadowOverride;
    bool undefOverride;
    bool typeSafeFormatOverride;
    bool xppOptimizationOverride;
    bool xmmOptimizationOverride;
    bool llvmOptOverride;
    bool llvmCompilerOverride;
    bool llvmLtoOverride;
};

// Fully resolved values for one compiler invocation. A project evaluation can
// provide the base layer; only CLI fields that were actually present replace it.
struct XsEffectiveCompilerOptions
{
    std::string compilerVersion;
    std::string standard;
    std::optional<std::string> target;
    XsBuildOutput output;
    XsCompilerSettings compiler;
};

enum XsCliParseResult : std::uint8_t
{
    XS_CLI_PARSE_ERROR,
    XS_CLI_PARSE_READY,
    XS_CLI_PARSE_HELP,
    XS_CLI_PARSE_VERSION,
};

struct XsCliParseOutcome
{
    XsCliParseResult result;
    XsCliOptions options;
    std::optional<XsCliCommand> helpCommand;
    std::string diagnostic;
};

extern "C"
{
    [[nodiscard]] XsCompilerSettings xs_cli_default_compiler_settings() noexcept;
    void xs_cli_apply_compiler_overrides(const XsCliOptions *options, XsCompilerSettings *settings) noexcept;
    [[nodiscard]] const char *xs_cli_warning_level_name(XsWarningLevel level) noexcept;
    [[nodiscard]] const char *xs_cli_output_extension(XsBuildOutput output) noexcept;
}

[[nodiscard]] XsCliParseOutcome ParseCommandLine(int argc, char **argv);
[[nodiscard]] XsEffectiveCompilerOptions
ResolveCompilerOptions(const XsCliOptions &options, const XsEffectiveCompilerOptions *projectDefaults = nullptr);
void PrintCliHelp(std::optional<XsCliCommand> command);
void PrintCliVersion();
