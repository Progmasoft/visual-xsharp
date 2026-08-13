// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

#include "options.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <string_view>
#include <utility>

#ifndef XS_PROJECT_VERSION
#    define XS_PROJECT_VERSION "0.3.1"
#endif

namespace
{
using namespace std::literals;

enum class Command : unsigned
{
    Build,
    Check,
    Install,
    Resolve,
    Run,
    Test,
    Update,
    Version,
    ViGet,
    Count,
};

enum class Option : unsigned
{
    File,
    Standard,
    CompilerVersion,
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

using CommandMask = unsigned;

constexpr CommandMask Bit(Command command)
{
    return 1U << static_cast<unsigned>(command);
}

constexpr CommandMask kCompilerCommands =
    Bit(Command::Build) | Bit(Command::Check) | Bit(Command::Run) | Bit(Command::Test);

struct CommandSpec
{
    std::string_view name;
    Command command;
    bool acceptsPositional;
};

struct OptionSpec
{
    std::string_view spelling;
    Option option;
    CommandMask commands;
    bool requiresValue;
};

// The schema is the single authority for option spelling, arity, and command
// scope. Parsing cannot accidentally make a build-only flag valid for install.
constexpr CommandSpec kCommands[] = {
    {"build", Command::Build, false},     {"check", Command::Check, false},     {"install", Command::Install, true},
    {"resolve", Command::Resolve, false}, {"run", Command::Run, false},         {"test", Command::Test, false},
    {"update", Command::Update, false},   {"version", Command::Version, false}, {"viget", Command::ViGet, true},
};

constexpr OptionSpec kOptions[] = {
    {"-File", Option::File, kCompilerCommands, true},
    {"-Standard", Option::Standard, kCompilerCommands, true},
    {"-Compiler-Version", Option::CompilerVersion, kCompilerCommands, true},
    {"-Emit", Option::Emit, Bit(Command::Build), true},
    {"-Build", Option::Build, Bit(Command::Build) | Bit(Command::Check), true},
    {"-Warnings", Option::Warnings, kCompilerCommands, true},
    {"-Werror", Option::Werror, kCompilerCommands, true},
    {"-Wexperimental", Option::Wexperimental, kCompilerCommands, true},
    {"-Wshadow", Option::Wshadow, kCompilerCommands, true},
    {"-Wundef", Option::Wundef, kCompilerCommands, true},
    {"-Type-Safe-Format", Option::TypeSafeFormat, kCompilerCommands, true},
    {"-Backend", Option::Backend, kCompilerCommands, true},
    {"-Llvm-OptLevel", Option::LlvmOptLevel, kCompilerCommands, true},
    {"-Llvm-Compiler", Option::LlvmCompiler, kCompilerCommands, true},
    {"-Llvm-Lto", Option::LlvmLto, kCompilerCommands, true},
    {"-Xpp-Optimization-Passes", Option::XppOptimization, kCompilerCommands, true},
    {"-Xmm-Optimization-Passes", Option::XmmOptimization, kCompilerCommands, true},
    {"-Global", Option::Global, Bit(Command::Install), false},
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

[[nodiscard]] char *Copy(std::string_view value)
{
    auto *result = static_cast<char *>(std::malloc(value.size() + 1U));
    if(result == nullptr)
        return nullptr;
    std::memcpy(result, value.data(), value.size());
    result[value.size()] = '\0';
    return result;
}

void Release(XsCliOptions &options)
{
    std::free(const_cast<char *>(options.file_path));
    std::free(const_cast<char *>(options.package_name));
    std::free(const_cast<char *>(options.compiler_version));
    std::free(const_cast<char *>(options.standard));
    options = {};
}

[[nodiscard]] XsCliParseResult Fail(XsCliOptions &options, std::string_view message)
{
    std::fprintf(stderr, "vxs: %.*s\n", static_cast<int>(message.size()), message.data());
    Release(options);
    return XS_CLI_PARSE_ERROR;
}

void PrintHelp(std::string_view command)
{
    if(command.empty())
    {
        std::puts("Visual X# compiler, project, and ViGet command-line interface.\n\n"
                  "Usage: vxs <command> [options]\n\n"
                  "Commands:\n"
                  "  check  build  run  test  resolve  update  install  viget  version\n"
                  "Run 'vxs <command> --help' for command options.");
        return;
    }
    std::printf("Usage: vxs %.*s [options]\n", static_cast<int>(command.size()), command.data());
    if(command == "check" || command == "build" || command == "run" || command == "test")
        std::puts("\nCompiler options:\n"
                  "  -File PATH\n"
                  "  -Build vxs|core\n"
                  "  -Emit core|llvmll|llvmbc\n"
                  "  -Standard 26|latest  -Compiler-Version VERSION|latest\n"
                  "  -Warnings all|medium|low|none  -Werror BOOL\n"
                  "  -Wexperimental BOOL  -Wshadow BOOL  -Wundef BOOL\n"
                  "  -Type-Safe-Format BOOL  -Backend llvm\n"
                  "  -Llvm-OptLevel 1|2|3|g  -Llvm-Compiler aot|orc  -Llvm-Lto fat|thin|none\n"
                  "  -Xpp-Optimization-Passes BOOL  -Xmm-Optimization-Passes BOOL");
}

[[nodiscard]] bool ParseBool(std::string_view value, bool &output)
{
    if(value == "true")
        output = true;
    else if(value == "false")
        output = false;
    else
        return false;
    return true;
}

template <typename Enum, std::size_t Size>
[[nodiscard]] bool ParseEnum(std::string_view value, Enum &output,
                             const std::array<std::pair<std::string_view, Enum>, Size> &values)
{
    for(const auto &[spelling, parsed] : values)
    {
        if(value == spelling)
        {
            output = parsed;
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool ParseOutput(std::string_view value, XsBuildOutput &output)
{
    constexpr std::array values = {
        std::pair{"binary"sv, XS_BUILD_OUTPUT_BINARY},  std::pair{"object"sv, XS_BUILD_OUTPUT_OBJECT},
        std::pair{"core"sv, XS_BUILD_OUTPUT_CORE},      std::pair{"xpp"sv, XS_BUILD_OUTPUT_XPP},
        std::pair{"xmm"sv, XS_BUILD_OUTPUT_XMM},        std::pair{"assembly"sv, XS_BUILD_OUTPUT_ASSEMBLY},
        std::pair{"llvmll"sv, XS_BUILD_OUTPUT_LLVM_LL}, std::pair{"llvmbc"sv, XS_BUILD_OUTPUT_LLVM_BC},
    };
    return ParseEnum(value, output, values);
}

[[nodiscard]] bool ParseInput(std::string_view value, XsBuildInput &input)
{
    constexpr std::array values = {
        std::pair{"vxs"sv, XS_BUILD_INPUT_VXS},        std::pair{"core"sv, XS_BUILD_INPUT_CORE},
        std::pair{"object"sv, XS_BUILD_INPUT_OBJECT},  std::pair{"xpp"sv, XS_BUILD_INPUT_XPP},
        std::pair{"xmm"sv, XS_BUILD_INPUT_XMM},        std::pair{"llvmll"sv, XS_BUILD_INPUT_LLVM_LL},
        std::pair{"llvmbc"sv, XS_BUILD_INPUT_LLVM_BC},
    };
    return ParseEnum(value, input, values);
}

[[nodiscard]] bool Assign(char const *&destination, std::string_view value)
{
    destination = Copy(value);
    return destination != nullptr;
}

[[nodiscard]] bool ApplyBoolean(Option option, std::string_view value, XsCliOptions &options)
{
    bool *destination{};
    bool *overrideFlag{};
    switch(option)
    {
    case Option::Werror:
        destination = &options.compiler.warnings_as_errors;
        overrideFlag = &options.werror_override;
        break;
    case Option::Wexperimental:
        destination = &options.compiler.experimental_warnings;
        overrideFlag = &options.experimental_override;
        break;
    case Option::Wshadow:
        destination = &options.compiler.shadow_warnings;
        overrideFlag = &options.shadow_override;
        break;
    case Option::Wundef:
        destination = &options.compiler.undefined_warnings;
        overrideFlag = &options.undef_override;
        break;
    case Option::TypeSafeFormat:
        destination = &options.compiler.type_safe_format;
        overrideFlag = &options.type_safe_format_override;
        break;
    case Option::XppOptimization:
        destination = &options.compiler.xpp_optimization_passes;
        overrideFlag = &options.xpp_optimization_override;
        break;
    case Option::XmmOptimization:
        destination = &options.compiler.xmm_optimization_passes;
        overrideFlag = &options.xmm_optimization_override;
        break;
    default:
        return false;
    }
    if(!ParseBool(value, *destination))
        return false;
    *overrideFlag = true;
    return true;
}

// Values become typed at this boundary. The rest of the driver never interprets
// raw argv text or carries compatibility aliases into project configuration.
[[nodiscard]] bool ApplyOption(Option option, std::string_view value, XsCliOptions &options)
{
    switch(option)
    {
    case Option::File:
        return !value.empty() && Assign(options.file_path, value);
    case Option::Standard:
        return (value == "26" || value == "latest") && Assign(options.standard, value);
    case Option::CompilerVersion:
        return !value.empty() && Assign(options.compiler_version, value);
    case Option::Emit:
        options.output_override = ParseOutput(value, options.output);
        return options.output_override;
    case Option::Build:
        return ParseInput(value, options.input);
    case Option::Warnings:
    {
        constexpr std::array values = {
            std::pair{"all"sv, XS_WARNING_ALL},
            std::pair{"medium"sv, XS_WARNING_MEDIUM},
            std::pair{"low"sv, XS_WARNING_LOW},
            std::pair{"none"sv, XS_WARNING_NONE},
        };
        options.warning_override = ParseEnum(value, options.compiler.warning_level, values);
        return options.warning_override;
    }
    case Option::Backend:
        return value == "llvm";
    case Option::LlvmOptLevel:
    {
        constexpr std::array values = {
            std::pair{"1"sv, XS_LLVM_OPT_1},
            std::pair{"2"sv, XS_LLVM_OPT_2},
            std::pair{"3"sv, XS_LLVM_OPT_3},
            std::pair{"g"sv, XS_LLVM_OPT_G},
        };
        options.llvm_opt_override = ParseEnum(value, options.compiler.llvm_opt_level, values);
        return options.llvm_opt_override;
    }
    case Option::LlvmCompiler:
    {
        constexpr std::array values = {std::pair{"aot"sv, XS_LLVM_COMPILER_AOT},
                                       std::pair{"orc"sv, XS_LLVM_COMPILER_ORC}};
        options.llvm_compiler_override = ParseEnum(value, options.compiler.llvm_compiler, values);
        return options.llvm_compiler_override;
    }
    case Option::LlvmLto:
    {
        constexpr std::array values = {
            std::pair{"none"sv, XS_LLVM_LTO_NONE},
            std::pair{"fat"sv, XS_LLVM_LTO_FAT},
            std::pair{"thin"sv, XS_LLVM_LTO_THIN},
        };
        options.llvm_lto_override = ParseEnum(value, options.compiler.llvm_lto, values);
        return options.llvm_lto_override;
    }
    case Option::Global:
        options.global_install = true;
        return true;
    default:
        return ApplyBoolean(option, value, options);
    }
}
} // namespace

extern "C" XsCompilerSettings xs_cli_default_compiler_settings(void)
{
    return {.warning_level = XS_WARNING_MEDIUM,
            .warnings_as_errors = false,
            .experimental_warnings = false,
            .shadow_warnings = false,
            .undefined_warnings = true,
            .type_safe_format = true,
            .xpp_optimization_passes = true,
            .xmm_optimization_passes = true,
            .llvm_opt_level = XS_LLVM_OPT_2,
            .llvm_compiler = XS_LLVM_COMPILER_AOT,
            .llvm_lto = XS_LLVM_LTO_NONE};
}

extern "C" void xs_cli_apply_compiler_overrides(const XsCliOptions *options, XsCompilerSettings *settings)
{
    // Project settings are the base layer. Only command-line fields explicitly
    // present in the schema override them, so defaults never erase DSL choices.
    if(options->warning_override)
        settings->warning_level = options->compiler.warning_level;
    if(options->werror_override)
        settings->warnings_as_errors = options->compiler.warnings_as_errors;
    if(options->experimental_override)
        settings->experimental_warnings = options->compiler.experimental_warnings;
    if(options->shadow_override)
        settings->shadow_warnings = options->compiler.shadow_warnings;
    if(options->undef_override)
        settings->undefined_warnings = options->compiler.undefined_warnings;
    if(options->type_safe_format_override)
        settings->type_safe_format = options->compiler.type_safe_format;
    if(options->xpp_optimization_override)
        settings->xpp_optimization_passes = options->compiler.xpp_optimization_passes;
    if(options->xmm_optimization_override)
        settings->xmm_optimization_passes = options->compiler.xmm_optimization_passes;
    if(options->llvm_opt_override)
        settings->llvm_opt_level = options->compiler.llvm_opt_level;
    if(options->llvm_compiler_override)
        settings->llvm_compiler = options->compiler.llvm_compiler;
    if(options->llvm_lto_override)
        settings->llvm_lto = options->compiler.llvm_lto;
}

extern "C" const char *xs_cli_warning_level_name(XsWarningLevel level)
{
    constexpr const char *names[] = {"all", "medium", "low", "none"};
    const auto index = static_cast<unsigned>(level);
    return index < 4U ? names[index] : "medium";
}

extern "C" const char *xs_cli_output_extension(XsBuildOutput output)
{
    constexpr const char *extensions[] = {".vxse", ".obj", ".core", ".xpp", ".xmm", ".asm", ".ll", ".bc"};
    const auto index = static_cast<unsigned>(output);
    return index < 8U ? extensions[index] : "";
}

extern "C" XsCliParseResult xs_cli_parse(int argc, char **argv, XsCliOptions *options)
{
    *options = {};
    options->compiler = xs_cli_default_compiler_settings();
    options->input = XS_BUILD_INPUT_VXS;
    if(argc == 2 && std::string_view(argv[1]) == "--version")
    {
        std::printf("vxs %s\n", XS_PROJECT_VERSION);
        return XS_CLI_PARSE_EXIT;
    }
    if(argc < 2 || std::string_view(argv[1]) == "--help")
    {
        PrintHelp({});
        return argc < 2 ? XS_CLI_PARSE_ERROR : XS_CLI_PARSE_EXIT;
    }

    const auto command = FindCommand(argv[1]);
    if(!command)
        return Fail(*options, "unknown command");
    if(command->command == Command::Version)
    {
        if(argc != 2)
            return Fail(*options, "version does not accept arguments");
        std::printf("vxs %s\n", XS_PROJECT_VERSION);
        return XS_CLI_PARSE_EXIT;
    }
    options->command = argv[1];

    std::array<bool, static_cast<unsigned>(Option::Count)> seen{};
    bool positionalSeen = false;
    for(int index = 2; index < argc; ++index)
    {
        const std::string_view argument(argv[index]);
        if(argument == "--help")
        {
            PrintHelp(command->name);
            Release(*options);
            return XS_CLI_PARSE_EXIT;
        }
        if(!argument.starts_with('-'))
        {
            if(!command->acceptsPositional || positionalSeen)
                return Fail(*options, "unexpected positional argument");
            if(!Assign(options->package_name, argument))
                return Fail(*options, "out of memory while retaining the positional argument");
            positionalSeen = true;
            continue;
        }

        const OptionSpec *spec = FindOption(argument);
        if(spec == nullptr)
            return Fail(*options, "unknown option");
        if((spec->commands & Bit(command->command)) == 0U)
            return Fail(*options, "option is not valid for this command");
        const auto optionIndex = static_cast<unsigned>(spec->option);
        if(seen[optionIndex])
            return Fail(*options, "option was specified more than once");
        seen[optionIndex] = true;

        std::string_view value;
        if(spec->requiresValue)
        {
            if(index + 1 >= argc || std::string_view(argv[index + 1]).starts_with('-'))
                return Fail(*options, "option requires a value");
            value = argv[++index];
        }
        if(!ApplyOption(spec->option, value, *options))
            return Fail(*options, "option value is invalid");
    }

    if(command->acceptsPositional && !positionalSeen)
        return Fail(*options, "command requires a positional argument");
    if(command->command == Command::ViGet && options->package_name != nullptr &&
       std::string_view(options->package_name) != "push" && std::string_view(options->package_name) != "update")
        return Fail(*options, "viget expects push or update");
    if(options->standard == nullptr && !Assign(options->standard, "latest"))
        return Fail(*options, "out of memory while applying defaults");
    if(options->compiler_version == nullptr && !Assign(options->compiler_version, "latest"))
        return Fail(*options, "out of memory while applying defaults");
    return XS_CLI_PARSE_READY;
}

extern "C" void xs_cli_options_free(XsCliOptions *options)
{
    if(options != nullptr)
        Release(*options);
}
