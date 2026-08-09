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
#include <vector>

namespace
{
struct ParsedValues
{
    std::string file;
    std::string module;
    std::string output;
    std::string warning;
    std::string werror;
    std::string verbose;
    bool hir = false;
    bool mir = false;
    bool xlil = false;
    bool version = false;
};

[[nodiscard]] char *copy_string(const std::string &value)
{
    if(value.empty())
        return nullptr;
    auto *copy = static_cast<char *>(std::malloc(value.size() + 1));
    if(copy != nullptr)
        std::memcpy(copy, value.c_str(), value.size() + 1);
    return copy;
}

[[nodiscard]] const char *command_name(std::string_view command)
{
    if(command == "build")
        return "build";
    if(command == "check")
        return "check";
    if(command == "resolve")
        return "resolve";
    if(command == "run")
        return "run";
    if(command == "test")
        return "test";
    return nullptr;
}

[[nodiscard]] bool parse_bool(std::string_view text, bool &value)
{
    if(text == "true")
    {
        value = true;
        return true;
    }
    if(text == "false")
    {
        value = false;
        return true;
    }
    return false;
}

[[nodiscard]] bool parse_warning_level(std::string_view text, XsWarningLevel &level)
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

[[nodiscard]] bool parse_output(std::string_view text, XsBuildOutput &output)
{
    if(text == "hir")
        output = XS_BUILD_OUTPUT_HIR;
    else if(text == "mir")
        output = XS_BUILD_OUTPUT_MIR;
    else if(text == "xlil")
        output = XS_BUILD_OUTPUT_XLIL;
    else
        return false;
    return true;
}

void add_common_options(Dim::Cli &cli, ParsedValues &values, bool allow_file)
{
    if(allow_file)
        cli.opt(&values.file, "file")
            .valueDesc("PATH")
            .desc("Compile one Visual X# or intermediate-language file; -file is the canonical spelling.");
    cli.opt(&values.module, "module").valueDesc("DIRECTORY").desc("Add a Visual X# module root.");
    cli.opt(&values.warning, "warning").valueDesc("LEVEL").desc("Set warning level: all, medium, low, or none.");
    cli.opt(&values.werror, "werror").valueDesc("BOOL").desc("Treat warnings as errors (true or false).");
    cli.opt(&values.verbose, "verbose").valueDesc("BOOL").desc("Enable verbose compiler output (true or false).");
}

void configure_cli(Dim::Cli &cli, ParsedValues &values)
{
    cli.responseFiles(false);
    cli.header("Visual X# compiler and project command-line interface.");
    cli.before(
        [](Dim::Cli &, std::vector<std::string> &args)
        {
            for(auto &argument : args)
            {
                if(argument == "-file")
                    argument = "--file";
            }
        });

    cli.command("check").desc("Parse, expand, and type-check Visual X# sources without emitting artifacts.");
    add_common_options(cli, values, true);

    cli.command("build").desc("Build Visual X# sources or emit a selected intermediate representation.");
    add_common_options(cli, values, true);
    cli.opt(&values.output, "output").valueDesc("IR").desc("Emit hir, mir, or xlil text.");
    cli.opt(&values.hir, "hir.").desc("Compile a direct XHIR input or emit XHIR.");
    cli.opt(&values.mir, "mir.").desc("Compile a direct XMIR input or emit XMIR.");
    cli.opt(&values.xlil, "xlil.").desc("Compile a direct XLIL input or emit XLIL.");

    cli.command("run").desc("Build and run a Visual X# native executable.");
    add_common_options(cli, values, true);

    cli.command("test").desc("Build and execute Visual X# tests.");
    add_common_options(cli, values, true);

    cli.command("resolve").desc("Resolve project dependencies and refresh Visual.XSharp.Lockfile.sqlite3.");

    cli.command("");
    cli.opt(&values.version, "version.").desc("Show the compiler version and exit.");
}

[[nodiscard]] XsCliParseResult report_error(Dim::Cli &cli, std::string_view message)
{
    static_cast<void>(cli);
    fmt::print(stderr, "vxs: {}\n", message);
    return XS_CLI_PARSE_ERROR;
}

[[nodiscard]] bool apply_compiler_options(const ParsedValues &values, XsCliOptions &options)
{
    if(!values.warning.empty())
    {
        if(!parse_warning_level(values.warning, options.compiler.warning_level))
            return false;
        options.warning_override = true;
    }
    if(!values.werror.empty())
    {
        if(!parse_bool(values.werror, options.compiler.warnings_as_errors))
            return false;
        options.werror_override = true;
    }
    if(!values.verbose.empty())
    {
        if(!parse_bool(values.verbose, options.compiler.verbose))
            return false;
        options.verbose_override = true;
    }
    return true;
}
} // namespace

extern "C" XsCompilerSettings xs_cli_default_compiler_settings(void)
{
    return XsCompilerSettings{.warning_level = XS_WARNING_MEDIUM, .warnings_as_errors = false, .verbose = true};
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
    if(options->verbose_override)
        settings->verbose = options->compiler.verbose;
}

extern "C" const char *xs_cli_output_extension(XsBuildOutput output)
{
    switch(output)
    {
    case XS_BUILD_OUTPUT_HIR:
        return ".xhir";
    case XS_BUILD_OUTPUT_MIR:
        return ".xmir";
    case XS_BUILD_OUTPUT_XLIL:
        return ".xlil";
    case XS_BUILD_OUTPUT_NONE:
        return "";
    }
    return "";
}

extern "C" XsCliParseResult xs_cli_parse(int argc, char **argv, XsCliOptions *options)
{
    *options = XsCliOptions{};
    options->compiler = xs_cli_default_compiler_settings();
    ParsedValues values;
    Dim::CliLocal cli;
    configure_cli(cli, values);

    std::vector<std::string> arguments;
    arguments.reserve(static_cast<std::size_t>(argc));
    for(int index = 0; index < argc; ++index)
        arguments.emplace_back(argv[index]);

    if(!cli.parse(std::move(arguments)))
    {
        if(cli.exitCode() == Dim::kExitOk)
            return XS_CLI_PARSE_EXIT;
        std::string error;
        cli.printError(&error);
        fmt::print(stderr, "{}", error);
        return XS_CLI_PARSE_ERROR;
    }

    if(values.version)
    {
        fmt::print("vxs {}\n", XS_PROJECT_VERSION);
        return XS_CLI_PARSE_EXIT;
    }

    options->command = command_name(cli.commandMatched());
    if(options->command == nullptr)
        return report_error(cli, "a command is required");
    if(!apply_compiler_options(values, *options))
        return report_error(cli, "invalid compiler option value");

    const unsigned short_output_count =
        static_cast<unsigned>(values.hir) + static_cast<unsigned>(values.mir) + static_cast<unsigned>(values.xlil);
    if(short_output_count > 1 || (!values.output.empty() && short_output_count != 0))
        return report_error(cli, "select exactly one intermediate output");
    if(!values.output.empty() && !parse_output(values.output, options->output))
        return report_error(cli, "--output expects hir, mir, or xlil");
    if(values.hir)
        options->output = XS_BUILD_OUTPUT_HIR;
    else if(values.mir)
        options->output = XS_BUILD_OUTPUT_MIR;
    else if(values.xlil)
        options->output = XS_BUILD_OUTPUT_XLIL;

    const std::string_view command{options->command};
    if(values.file.empty())
    {
        if(options->output != XS_BUILD_OUTPUT_NONE && command != "build")
            return report_error(cli, "intermediate output is only valid for build");
    }
    else if(!values.module.empty())
        return report_error(cli, "-file and --module cannot be combined");
    else if((command == "check" || command == "test") && options->output != XS_BUILD_OUTPUT_NONE)
        return report_error(cli, "check and test do not emit intermediate output");
    else if(command == "run" && options->output != XS_BUILD_OUTPUT_NONE)
        return report_error(cli, "run does not emit intermediate output");

    options->file_path = copy_string(values.file);
    options->module_path = copy_string(values.module);
    if((!values.file.empty() && options->file_path == nullptr) ||
       (!values.module.empty() && options->module_path == nullptr))
    {
        xs_cli_options_free(options);
        return report_error(cli, "out of memory while retaining command-line arguments");
    }
    return XS_CLI_PARSE_READY;
}

extern "C" void xs_cli_options_free(XsCliOptions *options)
{
    std::free(const_cast<char *>(options->file_path));
    std::free(const_cast<char *>(options->module_path));
    options->file_path = nullptr;
    options->module_path = nullptr;
}
