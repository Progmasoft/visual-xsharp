/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "options.h"

#include "dimcli/cli.h"

#include <fmt/format.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace
{
struct ParsedValues
{
    std::string file;
    std::string package;
    std::string viget_action;
    std::string standard;
    std::string compiler_version;
    std::string warning;
    std::string werror;
    std::string experimental;
    std::string shadow;
    std::string undef;
    std::string type_safe_format;
    std::string backend;
    std::string llvm_opt;
    std::string llvm_compiler;
    std::string llvm_lto;
    std::string xpp_optimization;
    std::string xmm_optimization;
    std::string emit;
    std::string build;
    bool global_install = false;
    bool version_option = false;
};

[[nodiscard]] char *copy_string(const std::string &value)
{
    if(value.empty())
        return nullptr;
    auto *copy = static_cast<char *>(std::malloc(value.size() + 1U));
    if(copy != nullptr)
        std::memcpy(copy, value.c_str(), value.size() + 1U);
    return copy;
}

[[nodiscard]] const char *command_name(std::string_view command)
{
    static constexpr std::string_view commands[] = {"build", "check",  "install", "resolve", "run",
                                                    "test",  "update", "version", "viget"};
    for(const auto candidate : commands)
        if(command == candidate)
            return candidate.data();
    return nullptr;
}

[[nodiscard]] bool parse_bool(std::string_view text, bool &value)
{
    if(text == "true")
        value = true;
    else if(text == "false")
        value = false;
    else
        return false;
    return true;
}

[[nodiscard]] bool parse_warning(std::string_view text, XsWarningLevel &level)
{
    if(text == "all")
        level = XS_WARNING_ALL;
    else if(text == "medium")
        level = XS_WARNING_MEDIUM;
    else if(text == "low")
        level = XS_WARNING_LOW;
    else if(text == "none")
        level = XS_WARNING_NONE;
    else
        return false;
    return true;
}

[[nodiscard]] bool parse_emit(std::string_view text, XsBuildOutput &output)
{
    static const std::unordered_map<std::string_view, XsBuildOutput> outputs = {
        {"binary", XS_BUILD_OUTPUT_BINARY},  {"object", XS_BUILD_OUTPUT_OBJECT},
        {"core", XS_BUILD_OUTPUT_CORE},      {"xpp", XS_BUILD_OUTPUT_XPP},
        {"xmm", XS_BUILD_OUTPUT_XMM},        {"assembly", XS_BUILD_OUTPUT_ASSEMBLY},
        {"llvmll", XS_BUILD_OUTPUT_LLVM_LL}, {"llvmbc", XS_BUILD_OUTPUT_LLVM_BC},
    };
    const auto found = outputs.find(text);
    if(found == outputs.end())
        return false;
    output = found->second;
    return true;
}

[[nodiscard]] bool parse_build(std::string_view text, XsBuildInput &input)
{
    static const std::unordered_map<std::string_view, XsBuildInput> inputs = {
        {"vxs", XS_BUILD_INPUT_VXS},        {"object", XS_BUILD_INPUT_OBJECT}, {"core", XS_BUILD_INPUT_CORE},
        {"xpp", XS_BUILD_INPUT_XPP},        {"xmm", XS_BUILD_INPUT_XMM},       {"llvmll", XS_BUILD_INPUT_LLVM_LL},
        {"llvmbc", XS_BUILD_INPUT_LLVM_BC},
    };
    const auto found = inputs.find(text);
    if(found == inputs.end())
        return false;
    input = found->second;
    return true;
}

void normalize_visual_flags(std::vector<std::string> &arguments)
{
    static const std::unordered_map<std::string_view, std::string_view> names = {
        {"-Standard", "--standard"},
        {"-Compiler-Version", "--compiler-version"},
        {"-Werror", "--werror"},
        {"-Warnings", "--warnings"},
        {"-Wexperimental", "--wexperimental"},
        {"-Wshadow", "--wshadow"},
        {"-Wundef", "--wundef"},
        {"-Type-Safe-Format", "--type-safe-format"},
        {"-Backend", "--backend"},
        {"-Llvm-OptLevel", "--llvm-opt-level"},
        {"-Llvm-Compiler", "--llvm-compiler"},
        {"-Llvm-Lto", "--llvm-lto"},
        {"-Xpp-Optimization-Passes", "--xpp-optimization-passes"},
        {"-Xmm-Optimization-Passes", "--xmm-optimization-passes"},
        {"-Emit", "--emit"},
        {"-Build", "--build"},
        {"-File", "--file"},
        {"-Global", "--global"},
    };
    for(auto &argument : arguments)
    {
        const auto found = names.find(argument);
        if(found != names.end())
            argument = found->second;
    }
}

void add_compiler_options(Dim::Cli &cli, ParsedValues &values, bool allow_file)
{
    if(allow_file)
        cli.opt(&values.file, "file")
            .valueDesc("PATH")
            .desc("Compile one .vxs file; projects are discovered automatically.");
    cli.opt(&values.standard, "standard").valueDesc("26|latest").desc("Select the Visual X# language standard.");
    cli.opt(&values.compiler_version, "compiler-version")
        .valueDesc("VERSION|latest")
        .desc("Select the compiler version.");
    cli.opt(&values.werror, "werror").valueDesc("BOOL").desc("Treat warnings as errors.");
    cli.opt(&values.warning, "warnings").valueDesc("LEVEL").desc("Select all, medium, low, or none.");
    cli.opt(&values.experimental, "wexperimental").valueDesc("BOOL").desc("Enable experimental warnings.");
    cli.opt(&values.shadow, "wshadow").valueDesc("BOOL").desc("Enable shadowing warnings.");
    cli.opt(&values.undef, "wundef").valueDesc("BOOL").desc("Enable undefined-name warnings.");
    cli.opt(&values.type_safe_format, "type-safe-format").valueDesc("BOOL").desc("Enable type-safe format checking.");
    cli.opt(&values.backend, "backend").valueDesc("llvm").desc("Select the compiler backend.");
    cli.opt(&values.llvm_opt, "llvm-opt-level").valueDesc("1|2|3|g").desc("Select LLVM optimization level.");
    cli.opt(&values.llvm_compiler, "llvm-compiler").valueDesc("aot|orc").desc("Select LLVM AOT or ORC compilation.");
    cli.opt(&values.llvm_lto, "llvm-lto").valueDesc("fat|thin|none").desc("Select LLVM link-time optimization.");
    cli.opt(&values.xpp_optimization, "xpp-optimization-passes")
        .valueDesc("BOOL")
        .desc("Enable Xpp optimization passes.");
    cli.opt(&values.xmm_optimization, "xmm-optimization-passes")
        .valueDesc("BOOL")
        .desc("Enable Xmm optimization passes.");
}

void configure_cli(Dim::Cli &cli, ParsedValues &values)
{
    cli.responseFiles(false);
    cli.header("Visual X# compiler, project, and ViGet command-line interface.");
    cli.before([](Dim::Cli &, std::vector<std::string> &arguments) { normalize_visual_flags(arguments); });

    cli.command("check").desc("Check a project or one Visual X# source file without emitting artifacts.");
    add_compiler_options(cli, values, true);
    cli.opt(&values.build, "build")
        .valueDesc("INPUT")
        .desc("Read object, vxs, core, xpp, xmm, llvmll, or llvmbc input.");

    cli.command("build").desc("Build a project or one file through the renewed compiler pipeline.");
    add_compiler_options(cli, values, true);
    cli.opt(&values.emit, "emit")
        .valueDesc("FORMAT")
        .desc("Emit binary, object, core, xpp, xmm, assembly, llvmll, or llvmbc.");
    cli.opt(&values.build, "build")
        .valueDesc("INPUT")
        .desc("Read object, vxs, core, xpp, xmm, llvmll, or llvmbc input.");
    cli.command("run").desc("Build and run a Visual X# executable.");
    add_compiler_options(cli, values, true);

    cli.command("test").desc("Build and run project tests.");
    add_compiler_options(cli, values, true);

    cli.command("resolve").desc("Resolve project dependencies and refresh Visual.XSharp.Lockfile.sqlite3.");
    cli.command("update").desc("Update system and project dependencies.");

    cli.command("install").desc("Install a ViGet package into the project or system.");
    cli.opt(&values.global_install, "global.").desc("Install the package for the current system.");
    cli.opt(&values.package, "<PACKAGE>").valueDesc("Publisher.Name");

    cli.command("viget").desc("Publish or update a ViPkg in ViGet.");
    cli.opt(&values.viget_action, "<ACTION>").valueDesc("push|update");

    cli.command("version").desc("Show the compiler version.");
    cli.command("");
    cli.opt(&values.version_option, "version.").desc("Show the compiler version.");
}

[[nodiscard]] XsCliParseResult error(std::string_view message)
{
    fmt::print(stderr, "vxs: {}\n", message);
    return XS_CLI_PARSE_ERROR;
}

void print_help(std::string_view command)
{
    if(command.empty())
    {
        fmt::print("Visual X# compiler, project, and ViGet command-line interface.\n\n"
                   "Usage: vxs <command> [options]\n\n"
                   "Commands:\n"
                   "  check  build  run  test  resolve  update  install  viget  version\n"
                   "Run 'vxs <command> --help' for command options.\n");
        return;
    }
    fmt::print("Usage: vxs {} [options]\n\n", command);
    if(command == "check" || command == "run" || command == "test" || command == "build")
    {
        fmt::print("Compiler options:\n"
                   "  -File PATH\n"
                   "  -Standard 26|latest\n"
                   "  -Compiler-Version VERSION|latest\n"
                   "  -Warnings all|medium|low|none\n"
                   "  -Werror BOOL  -Wexperimental BOOL  -Wshadow BOOL  -Wundef BOOL\n"
                   "  -Type-Safe-Format BOOL  -Backend llvm\n"
                   "  -Llvm-OptLevel 1|2|3|g  -Llvm-Compiler aot|orc\n"
                   "  -Llvm-Lto fat|thin|none\n"
                   "  -Xpp-Optimization-Passes BOOL  -Xmm-Optimization-Passes BOOL\n");
        if(command == "build")
            fmt::print("  -Emit binary|object|core|xpp|xmm|assembly|llvmll|llvmbc\n"
                       "  -Build object|vxs|core|xpp|xmm|llvmll|llvmbc\n");
        else if(command == "check")
            fmt::print("  -Build object|vxs|core|xpp|xmm|llvmll|llvmbc\n");
    }
    else if(command == "install")
        fmt::print("  -Global\n  PACKAGE uses Publisher.Name coordinates.\n");
    else if(command == "viget")
        fmt::print("  ACTION is push or update.\n");
}

[[nodiscard]] bool parse_compiler_values(const ParsedValues &values, XsCliOptions &options)
{
    auto boolean = [](const std::string &text, bool &value, bool &overridden)
    {
        if(text.empty())
            return true;
        overridden = true;
        return parse_bool(text, value);
    };
    if(!values.standard.empty() && values.standard != "26" && values.standard != "latest")
        return false;
    if(!values.backend.empty() && values.backend != "llvm")
        return false;
    if(!values.warning.empty())
    {
        options.warning_override = true;
        if(!parse_warning(values.warning, options.compiler.warning_level))
            return false;
    }
    if(!boolean(values.werror, options.compiler.warnings_as_errors, options.werror_override) ||
       !boolean(values.experimental, options.compiler.experimental_warnings, options.experimental_override) ||
       !boolean(values.shadow, options.compiler.shadow_warnings, options.shadow_override) ||
       !boolean(values.undef, options.compiler.undefined_warnings, options.undef_override) ||
       !boolean(values.type_safe_format, options.compiler.type_safe_format, options.type_safe_format_override) ||
       !boolean(values.xpp_optimization, options.compiler.xpp_optimization_passes, options.xpp_optimization_override) ||
       !boolean(values.xmm_optimization, options.compiler.xmm_optimization_passes, options.xmm_optimization_override))
        return false;
    if(!values.llvm_opt.empty())
    {
        options.llvm_opt_override = true;
        if(values.llvm_opt == "1")
            options.compiler.llvm_opt_level = XS_LLVM_OPT_1;
        else if(values.llvm_opt == "2")
            options.compiler.llvm_opt_level = XS_LLVM_OPT_2;
        else if(values.llvm_opt == "3")
            options.compiler.llvm_opt_level = XS_LLVM_OPT_3;
        else if(values.llvm_opt == "g")
            options.compiler.llvm_opt_level = XS_LLVM_OPT_G;
        else
            return false;
    }
    if(!values.llvm_compiler.empty())
    {
        options.llvm_compiler_override = true;
        if(values.llvm_compiler == "aot")
            options.compiler.llvm_compiler = XS_LLVM_COMPILER_AOT;
        else if(values.llvm_compiler == "orc")
            options.compiler.llvm_compiler = XS_LLVM_COMPILER_ORC;
        else
            return false;
    }
    if(!values.llvm_lto.empty())
    {
        options.llvm_lto_override = true;
        if(values.llvm_lto == "none")
            options.compiler.llvm_lto = XS_LLVM_LTO_NONE;
        else if(values.llvm_lto == "fat")
            options.compiler.llvm_lto = XS_LLVM_LTO_FAT;
        else if(values.llvm_lto == "thin")
            options.compiler.llvm_lto = XS_LLVM_LTO_THIN;
        else
            return false;
    }
    return true;
}
} // namespace

extern "C" XsCompilerSettings xs_cli_default_compiler_settings(void)
{
    return XsCompilerSettings{.warning_level = XS_WARNING_MEDIUM,
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

extern "C" const char *xs_cli_warning_level_name(XsWarningLevel level)
{
    switch(level)
    {
    case XS_WARNING_ALL:
        return "all";
    case XS_WARNING_MEDIUM:
        return "medium";
    case XS_WARNING_LOW:
        return "low";
    case XS_WARNING_NONE:
        return "none";
    }
    return "medium";
}

extern "C" void xs_cli_apply_compiler_overrides(const XsCliOptions *options, XsCompilerSettings *settings)
{
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

extern "C" const char *xs_cli_output_extension(XsBuildOutput output)
{
    switch(output)
    {
    case XS_BUILD_OUTPUT_BINARY:
        return ".vxse";
    case XS_BUILD_OUTPUT_OBJECT:
        return ".obj";
    case XS_BUILD_OUTPUT_CORE:
        return ".core";
    case XS_BUILD_OUTPUT_XPP:
        return ".xpp";
    case XS_BUILD_OUTPUT_XMM:
        return ".xmm";
    case XS_BUILD_OUTPUT_ASSEMBLY:
        return ".asm";
    case XS_BUILD_OUTPUT_LLVM_LL:
        return ".ll";
    case XS_BUILD_OUTPUT_LLVM_BC:
        return ".bc";
    }
    return "";
}

extern "C" XsCliParseResult xs_cli_parse(int argc, char **argv, XsCliOptions *options)
{
    *options = XsCliOptions{};
    options->compiler = xs_cli_default_compiler_settings();
    options->input = XS_BUILD_INPUT_VXS;
    for(int index = 1; index < argc; ++index)
    {
        const std::string_view argument{argv[index]};
        if(argument == "--help")
        {
            print_help(index > 1 ? std::string_view{argv[1]} : std::string_view{});
            return XS_CLI_PARSE_EXIT;
        }
        if(argument == "--version")
        {
            fmt::print("vxs {}\n", XS_PROJECT_VERSION);
            return XS_CLI_PARSE_EXIT;
        }
        if(argument.starts_with("--"))
        {
            fmt::print(stderr, "vxs: legacy long option '{}' is not supported\n", argument);
            return XS_CLI_PARSE_ERROR;
        }
    }
    ParsedValues values;
    Dim::CliLocal cli;
    configure_cli(cli, values);
    std::vector<std::string> arguments;
    for(int index = 0; index < argc; ++index)
    {
        arguments.emplace_back(argv[index]);
    }
    if(!cli.parse(std::move(arguments)))
    {
        if(cli.exitCode() == Dim::kExitOk)
            return XS_CLI_PARSE_EXIT;
        std::string parse_error;
        cli.printError(&parse_error);
        fmt::print(stderr, "{}", parse_error);
        return XS_CLI_PARSE_ERROR;
    }
    const auto matched = std::string_view{cli.commandMatched()};
    if(values.version_option || matched == "version")
    {
        fmt::print("vxs {}\n", XS_PROJECT_VERSION);
        return XS_CLI_PARSE_EXIT;
    }
    options->command = command_name(matched);
    if(options->command == nullptr)
        return error("a command is required");
    if(!parse_compiler_values(values, *options))
        return error("a compiler option has an invalid value");
    if(!values.emit.empty() && !parse_emit(values.emit, options->output))
        return error("-Emit expects binary, object, core, xpp, xmm, assembly, llvmll, or llvmbc");
    options->output_override = !values.emit.empty();
    if(!values.build.empty() && !parse_build(values.build, options->input))
        return error("-Build expects object, vxs, core, xpp, xmm, llvmll, or llvmbc");

    if(matched == "install")
    {
        options->global_install = values.global_install;
        options->package_name = copy_string(values.package);
    }
    else if(matched == "viget")
    {
        if(values.viget_action != "push" && values.viget_action != "update")
            return error("viget expects push or update");
        options->package_name = copy_string(values.viget_action);
    }
    options->file_path = copy_string(values.file);
    options->compiler_version = copy_string(values.compiler_version.empty() ? "latest" : values.compiler_version);
    options->standard = copy_string(values.standard.empty() ? "latest" : values.standard);
    if((!values.file.empty() && options->file_path == nullptr) || options->compiler_version == nullptr ||
       options->standard == nullptr || (matched == "install" && options->package_name == nullptr) ||
       (matched == "viget" && options->package_name == nullptr))
    {
        xs_cli_options_free(options);
        return error("out of memory while retaining command-line arguments");
    }
    return XS_CLI_PARSE_READY;
}

extern "C" void xs_cli_options_free(XsCliOptions *options)
{
    std::free(const_cast<char *>(options->file_path));
    std::free(const_cast<char *>(options->package_name));
    std::free(const_cast<char *>(options->compiler_version));
    std::free(const_cast<char *>(options->standard));
    options->file_path = nullptr;
    options->package_name = nullptr;
    options->compiler_version = nullptr;
    options->standard = nullptr;
}
