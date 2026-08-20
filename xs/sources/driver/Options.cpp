// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

#include "Options.hpp"

#include <array>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#ifndef XS_PROJECT_VERSION
#    define XS_PROJECT_VERSION "0.3.1"
#endif

namespace
{
using namespace std::literals;

enum class Option : unsigned
{
    File,
    Standard,
    CompilerVersion,
    Target,
    Emit,
    Build,
    Warnings,
    Werror,
    Wexperimental,
    Wshadow,
    Wundef,
    TypeSafeFormat,
    Backend,
    LlvmOptLevel,
    LlvmCompiler,
    LlvmLto,
    XppOptimization,
    XmmOptimization,
    Global,
    Count,
};

enum class ValueDomain : unsigned
{
    None,
    Path,
    Standard,
    Version,
    TargetTriple,
    Output,
    Input,
    WarningLevel,
    Boolean,
    Backend,
    LlvmOptLevel,
    LlvmCompiler,
    LlvmLto,
};

enum class PositionalKind : unsigned
{
    None,
    PackageCoordinate,
    ViGetAction,
};

enum class ApplyResult : unsigned
{
    Applied,
    Invalid,
};

using CommandMask = unsigned;

constexpr CommandMask Bit(XsCliCommand command)
{
    return 1U << static_cast<unsigned>(command);
}

constexpr CommandMask kCompilerCommands =
    Bit(XS_CLI_COMMAND_BUILD) | Bit(XS_CLI_COMMAND_CHECK) | Bit(XS_CLI_COMMAND_RUN) | Bit(XS_CLI_COMMAND_TEST);

struct CommandSpec
{
    std::string_view name;
    XsCliCommand command;
    PositionalKind positional;
    std::string_view description;
};

struct OptionSpec
{
    std::string_view spelling;
    Option option;
    CommandMask commands;
    ValueDomain domain;
    std::string_view description;
};

template <typename Value> using Choice = std::pair<std::string_view, Value>;

// All accepted spellings live in these typed domains. Both conversion and help
// rendering consume the same tables, preventing documentation from accepting a
// value that the parser rejects (or silently omitting a value it supports).
constexpr std::array kStandardValues = {"26"sv, "latest"sv};
constexpr std::array kBooleanValues = {Choice{"true"sv, true}, Choice{"false"sv, false}};
constexpr std::array kOutputValues = {
    Choice{"binary"sv, XS_BUILD_OUTPUT_BINARY},  Choice{"object"sv, XS_BUILD_OUTPUT_OBJECT},
    Choice{"core"sv, XS_BUILD_OUTPUT_CORE},      Choice{"xpp"sv, XS_BUILD_OUTPUT_XPP},
    Choice{"xmm"sv, XS_BUILD_OUTPUT_XMM},        Choice{"assembly"sv, XS_BUILD_OUTPUT_ASSEMBLY},
    Choice{"llvmll"sv, XS_BUILD_OUTPUT_LLVM_LL}, Choice{"llvmbc"sv, XS_BUILD_OUTPUT_LLVM_BC},
};
constexpr std::array kInputValues = {
    Choice{"object"sv, XS_BUILD_INPUT_OBJECT},  Choice{"vxs"sv, XS_BUILD_INPUT_VXS},
    Choice{"core"sv, XS_BUILD_INPUT_CORE},      Choice{"xpp"sv, XS_BUILD_INPUT_XPP},
    Choice{"xmm"sv, XS_BUILD_INPUT_XMM},        Choice{"llvmll"sv, XS_BUILD_INPUT_LLVM_LL},
    Choice{"llvmbc"sv, XS_BUILD_INPUT_LLVM_BC},
};
constexpr std::array kWarningValues = {
    Choice{"all"sv, XS_WARNING_ALL},
    Choice{"medium"sv, XS_WARNING_MEDIUM},
    Choice{"low"sv, XS_WARNING_LOW},
    Choice{"none"sv, XS_WARNING_NONE},
};
constexpr std::array kBackendValues = {"llvm"sv};
constexpr std::array kLlvmOptValues = {
    Choice{"1"sv, XS_LLVM_OPT_1},
    Choice{"2"sv, XS_LLVM_OPT_2},
    Choice{"3"sv, XS_LLVM_OPT_3},
    Choice{"g"sv, XS_LLVM_OPT_G},
};
constexpr std::array kLlvmCompilerValues = {
    Choice{"aot"sv, XS_LLVM_COMPILER_AOT},
    Choice{"orc"sv, XS_LLVM_COMPILER_ORC},
};
constexpr std::array kLlvmLtoValues = {
    Choice{"fat"sv, XS_LLVM_LTO_FAT},
    Choice{"thin"sv, XS_LLVM_LTO_THIN},
    Choice{"none"sv, XS_LLVM_LTO_NONE},
};
constexpr std::array kViGetActions = {
    Choice{"push"sv, XS_VIGET_ACTION_PUSH},
    Choice{"update"sv, XS_VIGET_ACTION_UPDATE},
};

constexpr CommandSpec kCommands[] = {
    {"check", XS_CLI_COMMAND_CHECK, PositionalKind::None, "validate a project or source artifact"},
    {"build", XS_CLI_COMMAND_BUILD, PositionalKind::None, "build a project or source artifact"},
    {"run", XS_CLI_COMMAND_RUN, PositionalKind::None, "build and run a project or source file"},
    {"test", XS_CLI_COMMAND_TEST, PositionalKind::None, "run the project's named test suites"},
    {"resolve", XS_CLI_COMMAND_RESOLVE, PositionalKind::None, "resolve project dependencies"},
    {"update", XS_CLI_COMMAND_UPDATE, PositionalKind::None, "update system and project dependencies"},
    {"install", XS_CLI_COMMAND_INSTALL, PositionalKind::PackageCoordinate, "install a ViGet package"},
    {"viget", XS_CLI_COMMAND_VIGET, PositionalKind::ViGetAction, "publish or update a ViGet package"},
    {"version", XS_CLI_COMMAND_VERSION, PositionalKind::None, "print the compiler version"},
};

// Option spelling, arity (through domain), command scope, and help description
// are deliberately one record. New options must not grow ad-hoc argv branches.
constexpr OptionSpec kOptions[] = {
    {"-File", Option::File, kCompilerCommands, ValueDomain::Path, "compile one explicit file instead of the project"},
    {"-Standard", Option::Standard, kCompilerCommands, ValueDomain::Standard, "select the language standard"},
    {"-Compiler-Version", Option::CompilerVersion, kCompilerCommands, ValueDomain::Version,
     "select the compiler version"},
    {"-Target", Option::Target, kCompilerCommands, ValueDomain::TargetTriple, "select the LLVM target triple"},
    {"-Emit", Option::Emit, Bit(XS_CLI_COMMAND_BUILD), ValueDomain::Output, "select the emitted artifact"},
    {"-Build", Option::Build, Bit(XS_CLI_COMMAND_BUILD) | Bit(XS_CLI_COMMAND_CHECK), ValueDomain::Input,
     "select the explicit input artifact kind"},
    {"-Warnings", Option::Warnings, kCompilerCommands, ValueDomain::WarningLevel, "select the warning level"},
    {"-Werror", Option::Werror, kCompilerCommands, ValueDomain::Boolean, "treat warnings as errors"},
    {"-Wexperimental", Option::Wexperimental, kCompilerCommands, ValueDomain::Boolean, "enable experimental warnings"},
    {"-Wshadow", Option::Wshadow, kCompilerCommands, ValueDomain::Boolean, "enable shadowing warnings"},
    {"-Wundef", Option::Wundef, kCompilerCommands, ValueDomain::Boolean, "enable undefined-name warnings"},
    {"-Type-Safe-Format", Option::TypeSafeFormat, kCompilerCommands, ValueDomain::Boolean,
     "enable type-safe format checks"},
    {"-Backend", Option::Backend, kCompilerCommands, ValueDomain::Backend, "select the compiler backend"},
    {"-Llvm-OptLevel", Option::LlvmOptLevel, kCompilerCommands, ValueDomain::LlvmOptLevel, "select LLVM optimization"},
    {"-Llvm-Compiler", Option::LlvmCompiler, kCompilerCommands, ValueDomain::LlvmCompiler,
     "select LLVM execution mode"},
    {"-Llvm-Lto", Option::LlvmLto, kCompilerCommands, ValueDomain::LlvmLto, "select LLVM link-time optimization"},
    {"-Xpp-Optimization-Passes", Option::XppOptimization, kCompilerCommands, ValueDomain::Boolean,
     "enable Xpp optimization passes"},
    {"-Xmm-Optimization-Passes", Option::XmmOptimization, kCompilerCommands, ValueDomain::Boolean,
     "enable Xmm optimization passes"},
    {"-Global", Option::Global, Bit(XS_CLI_COMMAND_INSTALL), ValueDomain::None,
     "install into the system package store"},
};

constexpr XsCompilerSettings kCompilerDefaults{
    .warning_level = XS_WARNING_MEDIUM,
    .warnings_as_errors = false,
    .experimental_warnings = false,
    .shadow_warnings = false,
    .undefined_warnings = true,
    .type_safe_format = true,
    .xpp_optimization_passes = true,
    .xmm_optimization_passes = true,
    .llvm_opt_level = XS_LLVM_OPT_2,
    .llvm_compiler = XS_LLVM_COMPILER_AOT,
    .llvm_lto = XS_LLVM_LTO_NONE,
};

[[nodiscard]] std::optional<CommandSpec> FindCommand(std::string_view spelling)
{
    for(const auto &spec : kCommands)
        if(spec.name == spelling)
            return spec;
    return std::nullopt;
}

[[nodiscard]] const OptionSpec *FindOption(std::string_view spelling)
{
    for(const auto &spec : kOptions)
        if(spec.spelling == spelling)
            return &spec;
    return nullptr;
}

template <typename Value, std::size_t Size>
[[nodiscard]] bool ParseChoice(std::string_view value, Value &output, const std::array<Choice<Value>, Size> &choices)
{
    for(const auto &[spelling, parsed] : choices)
    {
        if(value == spelling)
        {
            output = parsed;
            return true;
        }
    }
    return false;
}

template <std::size_t Size>
[[nodiscard]] bool Contains(std::string_view value, const std::array<std::string_view, Size> &choices)
{
    for(const auto choice : choices)
        if(value == choice)
            return true;
    return false;
}

template <typename Value, std::size_t Size>
void AppendChoices(std::string &output, const std::array<Choice<Value>, Size> &choices)
{
    for(std::size_t index = 0; index < choices.size(); ++index)
    {
        if(index != 0U)
            output.push_back('|');
        output.append(choices[index].first);
    }
}

template <std::size_t Size> void AppendChoices(std::string &output, const std::array<std::string_view, Size> &choices)
{
    for(std::size_t index = 0; index < choices.size(); ++index)
    {
        if(index != 0U)
            output.push_back('|');
        output.append(choices[index]);
    }
}

[[nodiscard]] std::string DomainText(ValueDomain domain)
{
    std::string result;
    switch(domain)
    {
    case ValueDomain::None:
        break;
    case ValueDomain::Path:
        result = "PATH";
        break;
    case ValueDomain::Version:
        result = "VERSION|latest";
        break;
    case ValueDomain::TargetTriple:
        result = "TARGET-TRIPLE";
        break;
    case ValueDomain::Standard:
        AppendChoices(result, kStandardValues);
        break;
    case ValueDomain::Output:
        AppendChoices(result, kOutputValues);
        break;
    case ValueDomain::Input:
        AppendChoices(result, kInputValues);
        break;
    case ValueDomain::WarningLevel:
        AppendChoices(result, kWarningValues);
        break;
    case ValueDomain::Boolean:
        AppendChoices(result, kBooleanValues);
        break;
    case ValueDomain::Backend:
        AppendChoices(result, kBackendValues);
        break;
    case ValueDomain::LlvmOptLevel:
        AppendChoices(result, kLlvmOptValues);
        break;
    case ValueDomain::LlvmCompiler:
        AppendChoices(result, kLlvmCompilerValues);
        break;
    case ValueDomain::LlvmLto:
        AppendChoices(result, kLlvmLtoValues);
        break;
    }
    return result;
}

[[nodiscard]] std::string_view BoolText(bool value)
{
    return value ? "true"sv : "false"sv;
}

template <typename Value, std::size_t Size>
[[nodiscard]] std::string_view ChoiceText(Value value, const std::array<Choice<Value>, Size> &choices)
{
    for(const auto &[spelling, candidate] : choices)
        if(candidate == value)
            return spelling;
    return {};
}

// Help defaults are rendered from the same typed state used by parsing. This
// avoids a second prose-only default table that can drift from execution.
[[nodiscard]] std::string_view DefaultText(Option option)
{
    switch(option)
    {
    case Option::Standard:
    case Option::CompilerVersion:
        return "latest"sv;
    case Option::Target:
        return "host"sv;
    case Option::Emit:
        return ChoiceText(XS_BUILD_OUTPUT_BINARY, kOutputValues);
    case Option::Build:
        return ChoiceText(XS_BUILD_INPUT_VXS, kInputValues);
    case Option::Warnings:
        return ChoiceText(kCompilerDefaults.warning_level, kWarningValues);
    case Option::Werror:
        return BoolText(kCompilerDefaults.warnings_as_errors);
    case Option::Wexperimental:
        return BoolText(kCompilerDefaults.experimental_warnings);
    case Option::Wshadow:
        return BoolText(kCompilerDefaults.shadow_warnings);
    case Option::Wundef:
        return BoolText(kCompilerDefaults.undefined_warnings);
    case Option::TypeSafeFormat:
        return BoolText(kCompilerDefaults.type_safe_format);
    case Option::Backend:
        return "llvm"sv;
    case Option::LlvmOptLevel:
        return ChoiceText(kCompilerDefaults.llvm_opt_level, kLlvmOptValues);
    case Option::LlvmCompiler:
        return ChoiceText(kCompilerDefaults.llvm_compiler, kLlvmCompilerValues);
    case Option::LlvmLto:
        return ChoiceText(kCompilerDefaults.llvm_lto, kLlvmLtoValues);
    case Option::XppOptimization:
        return BoolText(kCompilerDefaults.xpp_optimization_passes);
    case Option::XmmOptimization:
        return BoolText(kCompilerDefaults.xmm_optimization_passes);
    case Option::File:
    case Option::Global:
    case Option::Count:
        return {};
    }
    return {};
}

[[nodiscard]] XsCliParseOutcome Failure(XsCliOptions options, std::string message)
{
    return {XS_CLI_PARSE_ERROR, std::move(options), std::nullopt, std::move(message)};
}

[[nodiscard]] XsCliParseOutcome FailOption(XsCliOptions options, const OptionSpec &spec, std::string_view value)
{
    std::string message = "invalid value '";
    message.append(value);
    message.append("' for ");
    message.append(spec.spelling);
    message.append("; expected ");
    message.append(DomainText(spec.domain));
    return Failure(std::move(options), std::move(message));
}

[[nodiscard]] std::filesystem::path Utf8Path(std::string_view value)
{
    const auto *begin = reinterpret_cast<const char8_t *>(value.data());
    return std::filesystem::path(std::u8string_view(begin, value.size()));
}

[[nodiscard]] bool IsTargetTriple(std::string_view value)
{
    std::size_t segmentCount{};
    std::size_t segmentLength{};
    for(const char character : value)
    {
        if(character == '-')
        {
            if(segmentLength == 0U)
                return false;
            ++segmentCount;
            segmentLength = 0U;
            continue;
        }
        const bool accepted = (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z') ||
                              (character >= '0' && character <= '9') || character == '_' || character == '+' ||
                              character == '.';
        if(!accepted)
            return false;
        ++segmentLength;
    }
    return segmentLength != 0U && segmentCount >= 2U;
}

[[nodiscard]] std::string PositionalText(PositionalKind positional)
{
    switch(positional)
    {
    case PositionalKind::None:
        return {};
    case PositionalKind::PackageCoordinate:
        return " Publisher.Name";
    case PositionalKind::ViGetAction:
        return " push|update";
    }
    return {};
}

void PrintHelp(const CommandSpec *command)
{
    if(command == nullptr)
    {
        std::puts("Visual X# compiler, project, and ViGet command-line interface.\n\n"
                  "Usage: vxs <command> [options]\n\n"
                  "Commands:");
        for(const auto &spec : kCommands)
            std::printf("  %-9.*s %.*s\n", static_cast<int>(spec.name.size()), spec.name.data(),
                        static_cast<int>(spec.description.size()), spec.description.data());
        std::puts("\nRun 'vxs <command> --help' for command-specific options.");
        return;
    }

    const auto positional = PositionalText(command->positional);
    std::printf("Usage: vxs %.*s [options]%s\n", static_cast<int>(command->name.size()), command->name.data(),
                positional.c_str());
    bool headingPrinted = false;
    for(const auto &spec : kOptions)
    {
        if((spec.commands & Bit(command->command)) == 0U)
            continue;
        if(!headingPrinted)
        {
            std::puts("\nOptions:");
            headingPrinted = true;
        }
        std::string signature(spec.spelling);
        const auto domain = DomainText(spec.domain);
        if(!domain.empty())
        {
            signature.push_back(' ');
            signature.append(domain);
        }
        const auto defaultValue = DefaultText(spec.option);
        std::printf("  %-52s %.*s", signature.c_str(), static_cast<int>(spec.description.size()),
                    spec.description.data());
        if(!defaultValue.empty())
            std::printf(" [default: %.*s]", static_cast<int>(defaultValue.size()), defaultValue.data());
        std::putchar('\n');
    }
    std::puts("  --help                                               show this command help");
}

[[nodiscard]] ApplyResult ApplyBoolean(Option option, std::string_view value, XsCliOptions &options)
{
    bool parsed{};
    if(!ParseChoice(value, parsed, kBooleanValues))
        return ApplyResult::Invalid;
    bool *destination{};
    bool *overrideFlag{};
    switch(option)
    {
    case Option::Werror:
        destination = &options.compiler.warnings_as_errors;
        overrideFlag = &options.werrorOverride;
        break;
    case Option::Wexperimental:
        destination = &options.compiler.experimental_warnings;
        overrideFlag = &options.experimentalOverride;
        break;
    case Option::Wshadow:
        destination = &options.compiler.shadow_warnings;
        overrideFlag = &options.shadowOverride;
        break;
    case Option::Wundef:
        destination = &options.compiler.undefined_warnings;
        overrideFlag = &options.undefOverride;
        break;
    case Option::TypeSafeFormat:
        destination = &options.compiler.type_safe_format;
        overrideFlag = &options.typeSafeFormatOverride;
        break;
    case Option::XppOptimization:
        destination = &options.compiler.xpp_optimization_passes;
        overrideFlag = &options.xppOptimizationOverride;
        break;
    case Option::XmmOptimization:
        destination = &options.compiler.xmm_optimization_passes;
        overrideFlag = &options.xmmOptimizationOverride;
        break;
    default:
        return ApplyResult::Invalid;
    }
    *destination = parsed;
    *overrideFlag = true;
    return ApplyResult::Applied;
}

// Values become typed at this boundary. The rest of the driver never interprets
// raw argv text or carries compatibility aliases into project configuration.
[[nodiscard]] ApplyResult ApplyOption(const OptionSpec &spec, std::string_view value, XsCliOptions &options)
{
    switch(spec.option)
    {
    case Option::File:
        if(value.empty())
            return ApplyResult::Invalid;
        options.filePath = Utf8Path(value);
        return ApplyResult::Applied;
    case Option::Standard:
        if(!Contains(value, kStandardValues))
            return ApplyResult::Invalid;
        options.standard = value;
        options.standardOverride = true;
        return ApplyResult::Applied;
    case Option::CompilerVersion:
        if(value.empty())
            return ApplyResult::Invalid;
        options.compilerVersion = value;
        options.compilerVersionOverride = true;
        return ApplyResult::Applied;
    case Option::Target:
        if(!IsTargetTriple(value))
            return ApplyResult::Invalid;
        options.target = value;
        options.targetOverride = true;
        return ApplyResult::Applied;
    case Option::Emit:
        if(!ParseChoice(value, options.output, kOutputValues))
            return ApplyResult::Invalid;
        options.outputOverride = true;
        return ApplyResult::Applied;
    case Option::Build:
        return ParseChoice(value, options.input, kInputValues) ? ApplyResult::Applied : ApplyResult::Invalid;
    case Option::Warnings:
        if(!ParseChoice(value, options.compiler.warning_level, kWarningValues))
            return ApplyResult::Invalid;
        options.warningOverride = true;
        return ApplyResult::Applied;
    case Option::Backend:
        return Contains(value, kBackendValues) ? ApplyResult::Applied : ApplyResult::Invalid;
    case Option::LlvmOptLevel:
        if(!ParseChoice(value, options.compiler.llvm_opt_level, kLlvmOptValues))
            return ApplyResult::Invalid;
        options.llvmOptOverride = true;
        return ApplyResult::Applied;
    case Option::LlvmCompiler:
        if(!ParseChoice(value, options.compiler.llvm_compiler, kLlvmCompilerValues))
            return ApplyResult::Invalid;
        options.llvmCompilerOverride = true;
        return ApplyResult::Applied;
    case Option::LlvmLto:
        if(!ParseChoice(value, options.compiler.llvm_lto, kLlvmLtoValues))
            return ApplyResult::Invalid;
        options.llvmLtoOverride = true;
        return ApplyResult::Applied;
    case Option::Global:
        options.globalInstall = true;
        return ApplyResult::Applied;
    default:
        return ApplyBoolean(spec.option, value, options);
    }
}

void ApplyInitialDefaults(XsCliOptions &options)
{
    options.compiler = kCompilerDefaults;
    options.input = XS_BUILD_INPUT_VXS;
    options.output = XS_BUILD_OUTPUT_BINARY;
    options.standard = DefaultText(Option::Standard);
    options.compilerVersion = DefaultText(Option::CompilerVersion);
}

[[nodiscard]] bool IsPackageCoordinate(std::string_view value)
{
    const auto separator = value.find('.');
    return separator != std::string_view::npos && separator != 0U && separator + 1U < value.size() &&
           value.find(".."sv) == std::string_view::npos;
}

[[nodiscard]] bool ApplyPositional(const CommandSpec &command, std::string_view value, XsCliOptions &options)
{
    if(command.positional == PositionalKind::None || value.empty())
        return false;
    if(command.positional == PositionalKind::ViGetAction)
        return ParseChoice(value, options.vigetAction, kViGetActions);
    if(!IsPackageCoordinate(value))
        return false;
    options.packageCoordinate = value;
    return true;
}

[[nodiscard]] std::optional<std::string> ValidateCombination(const CommandSpec &command, const XsCliOptions &options)
{
    if(command.positional != PositionalKind::None && !options.packageCoordinate)
    {
        if(command.positional == PositionalKind::ViGetAction && options.vigetAction != XS_VIGET_ACTION_NONE)
            return std::nullopt;
        const auto expected =
            command.positional == PositionalKind::PackageCoordinate ? "Publisher.Name" : "push|update";
        return std::string(command.name) + " requires " + expected;
    }
    if(options.input != XS_BUILD_INPUT_VXS && !options.filePath)
        return "-Build with an artifact input requires -File";
    return std::nullopt;
}
} // namespace

extern "C" XsCompilerSettings xs_cli_default_compiler_settings(void) noexcept
{
    return kCompilerDefaults;
}

extern "C" void xs_cli_apply_compiler_overrides(const XsCliOptions *options, XsCompilerSettings *settings) noexcept
{
    if(options == nullptr || settings == nullptr)
        return;
    // Project settings are the base layer. Only command-line fields explicitly
    // present in the schema override them, so defaults never erase DSL choices.
    if(options->warningOverride)
        settings->warning_level = options->compiler.warning_level;
    if(options->werrorOverride)
        settings->warnings_as_errors = options->compiler.warnings_as_errors;
    if(options->experimentalOverride)
        settings->experimental_warnings = options->compiler.experimental_warnings;
    if(options->shadowOverride)
        settings->shadow_warnings = options->compiler.shadow_warnings;
    if(options->undefOverride)
        settings->undefined_warnings = options->compiler.undefined_warnings;
    if(options->typeSafeFormatOverride)
        settings->type_safe_format = options->compiler.type_safe_format;
    if(options->xppOptimizationOverride)
        settings->xpp_optimization_passes = options->compiler.xpp_optimization_passes;
    if(options->xmmOptimizationOverride)
        settings->xmm_optimization_passes = options->compiler.xmm_optimization_passes;
    if(options->llvmOptOverride)
        settings->llvm_opt_level = options->compiler.llvm_opt_level;
    if(options->llvmCompilerOverride)
        settings->llvm_compiler = options->compiler.llvm_compiler;
    if(options->llvmLtoOverride)
        settings->llvm_lto = options->compiler.llvm_lto;
}

XsEffectiveCompilerOptions ResolveCompilerOptions(const XsCliOptions &options,
                                                  const XsEffectiveCompilerOptions *projectDefaults)
{
    // Kotlin materializes both explicit DSL values and DSL defaults. Therefore
    // any project layer outranks CLI fallbacks, while the parser's presence bits
    // ensure only argv values explicitly supplied by the user can replace it.
    XsEffectiveCompilerOptions result;
    if(projectDefaults != nullptr)
        result = *projectDefaults;
    else
    {
        result.compilerVersion = options.compilerVersion;
        result.standard = options.standard;
        result.target = std::nullopt;
        result.output = options.output;
        result.compiler = kCompilerDefaults;
    }

    if(options.compilerVersionOverride)
        result.compilerVersion = options.compilerVersion;
    if(options.standardOverride)
        result.standard = options.standard;
    if(options.targetOverride)
        result.target = options.target;
    if(options.outputOverride)
        result.output = options.output;
    xs_cli_apply_compiler_overrides(&options, &result.compiler);
    return result;
}

extern "C" const char *xs_cli_warning_level_name(XsWarningLevel level) noexcept
{
    const auto spelling = ChoiceText(level, kWarningValues);
    return spelling.empty() ? "medium" : spelling.data();
}

extern "C" const char *xs_cli_output_extension(XsBuildOutput output) noexcept
{
    constexpr const char *extensions[] = {".vxse", ".obj", ".core", ".xpp", ".xmm", ".asm", ".ll", ".bc"};
    const auto index = static_cast<unsigned>(output);
    return index < 8U ? extensions[index] : "";
}

XsCliParseOutcome ParseCommandLine(int argc, char **argv)
{
    XsCliOptions options{};
    ApplyInitialDefaults(options);
    if(argc < 0 || (argc > 0 && argv == nullptr))
        return Failure(std::move(options), "invalid process argument vector");

    if(argc == 2 && argv[1] != nullptr && std::string_view(argv[1]) == "--version")
        return {XS_CLI_PARSE_VERSION, std::move(options), std::nullopt, {}};
    if(argc < 2 || argv[1] == nullptr)
        return Failure(std::move(options), "a command is required; use --help to list commands");
    if(std::string_view(argv[1]) == "--help")
        return {XS_CLI_PARSE_HELP, std::move(options), std::nullopt, {}};

    const auto command = FindCommand(argv[1]);
    if(!command)
        return Failure(std::move(options), std::string("unknown command '") + argv[1] + "'");
    if(command->command == XS_CLI_COMMAND_VERSION)
    {
        if(argc == 3 && argv[2] != nullptr && std::string_view(argv[2]) == "--help")
            return {XS_CLI_PARSE_HELP, std::move(options), command->command, {}};
        if(argc != 2)
            return Failure(std::move(options), "version does not accept arguments");
        return {XS_CLI_PARSE_VERSION, std::move(options), std::nullopt, {}};
    }
    options.command = command->command;

    std::array<bool, static_cast<unsigned>(Option::Count)> seen{};
    bool positionalSeen = false;
    for(int index = 2; index < argc; ++index)
    {
        if(argv[index] == nullptr)
            return Failure(std::move(options), "process argument vector contains null");
        const std::string_view argument(argv[index]);
        if(argument == "--help")
        {
            return {XS_CLI_PARSE_HELP, std::move(options), command->command, {}};
        }
        if(!argument.starts_with('-'))
        {
            if(positionalSeen)
                return Failure(std::move(options),
                               std::string("unexpected positional argument '") + std::string(argument) + "'");
            if(!ApplyPositional(*command, argument, options))
            {
                if(command->positional == PositionalKind::ViGetAction)
                    return Failure(std::move(options), std::string("invalid viget action '") + std::string(argument) +
                                                           "'; expected push|update");
                if(command->positional == PositionalKind::PackageCoordinate)
                    return Failure(std::move(options), std::string("invalid package coordinate '") +
                                                           std::string(argument) + "'; expected Publisher.Name");
                return Failure(std::move(options),
                               std::string("unexpected positional argument '") + std::string(argument) + "'");
            }
            positionalSeen = true;
            continue;
        }

        const OptionSpec *spec = FindOption(argument);
        if(spec == nullptr)
            return Failure(std::move(options), std::string("unknown option '") + std::string(argument) + "'");
        if((spec->commands & Bit(command->command)) == 0U)
            return Failure(std::move(options),
                           std::string(spec->spelling) + " is not valid for " + std::string(command->name));
        const auto optionIndex = static_cast<unsigned>(spec->option);
        if(seen[optionIndex])
            return Failure(std::move(options), std::string(spec->spelling) + " was specified more than once");
        seen[optionIndex] = true;

        std::string_view value;
        if(spec->domain != ValueDomain::None)
        {
            if(index + 1 >= argc || argv[index + 1] == nullptr || std::string_view(argv[index + 1]).starts_with('-'))
                return Failure(std::move(options),
                               std::string(spec->spelling) + " requires " + DomainText(spec->domain));
            value = argv[++index];
        }
        const auto applied = ApplyOption(*spec, value, options);
        if(applied == ApplyResult::Invalid)
            return FailOption(std::move(options), *spec, value);
    }

    if(auto diagnostic = ValidateCombination(*command, options))
        return Failure(std::move(options), std::move(*diagnostic));
    return {XS_CLI_PARSE_READY, std::move(options), std::nullopt, {}};
}

void PrintCliHelp(std::optional<XsCliCommand> command)
{
    if(!command)
    {
        PrintHelp(nullptr);
        return;
    }
    for(const auto &spec : kCommands)
    {
        if(spec.command == *command)
        {
            PrintHelp(&spec);
            return;
        }
    }
}

void PrintCliVersion()
{
    std::printf("vxs %s\n", XS_PROJECT_VERSION);
}
