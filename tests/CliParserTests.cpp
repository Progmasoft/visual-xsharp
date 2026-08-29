// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

#include "xs/sources/driver/Options.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <utility>
#include <vector>

namespace
{
class ParsedInvocation final
{
public:
    ParsedInvocation(std::initializer_list<std::string> arguments) : storage_(arguments)
    {
        argv_.reserve(storage_.size());
        for(auto &argument : storage_)
            argv_.push_back(argument.data());
        outcome_ = ParseCommandLine(static_cast<int>(argv_.size()), argv_.data());
    }

    ParsedInvocation(const ParsedInvocation &) = delete;
    ParsedInvocation &operator=(const ParsedInvocation &) = delete;
    [[nodiscard]] XsCliParseResult Result() const noexcept
    {
        return outcome_.result;
    }

    [[nodiscard]] const XsCliOptions &Options() const noexcept
    {
        return outcome_.options;
    }

    [[nodiscard]] const std::string &Diagnostic() const noexcept
    {
        return outcome_.diagnostic;
    }

    [[nodiscard]] std::optional<XsCliCommand> HelpCommand() const noexcept
    {
        return outcome_.helpCommand;
    }

private:
    std::vector<std::string> storage_;
    std::vector<char *> argv_;
    XsCliParseOutcome outcome_{};
};
} // namespace

TEST_CASE("CLI defaults become typed compiler settings", "[cli][parser]")
{
    const ParsedInvocation parsed{"vxs", "check"};

    REQUIRE(parsed.Result() == XS_CLI_PARSE_READY);
    const auto &options = parsed.Options();
    REQUIRE(options.command == XS_CLI_COMMAND_CHECK);
    REQUIRE_FALSE(options.filePath);
    REQUIRE(std::string(options.standard) == "latest");
    REQUIRE(options.compilerVersion == "latest");
    REQUIRE_FALSE(options.target);
    REQUIRE(options.input == XS_BUILD_INPUT_VXS);
    REQUIRE(options.output == XS_BUILD_OUTPUT_BINARY);
    REQUIRE(options.compiler.warning_level == XS_WARNING_MEDIUM);
    REQUIRE_FALSE(options.compiler.warnings_as_errors);
    REQUIRE_FALSE(options.compiler.experimental_warnings);
    REQUIRE_FALSE(options.compiler.shadow_warnings);
    REQUIRE(options.compiler.undefined_warnings);
    REQUIRE(options.compiler.type_safe_format);
    REQUIRE(options.compiler.xpp_optimization_passes);
    REQUIRE(options.compiler.xmm_optimization_passes);
    REQUIRE(options.compiler.llvm_opt_level == XS_LLVM_OPT_2);
    REQUIRE(options.compiler.llvm_compiler == XS_LLVM_COMPILER_AOT);
    REQUIRE(options.compiler.llvm_lto == XS_LLVM_LTO_NONE);
    REQUIRE_FALSE(options.compilerVersionOverride);
    REQUIRE_FALSE(options.standardOverride);
    REQUIRE_FALSE(options.targetOverride);
}

TEST_CASE("help and version are parser outcomes rather than parser side effects", "[cli][parser]")
{
    const ParsedInvocation globalHelp{"vxs", "--help"};
    REQUIRE(globalHelp.Result() == XS_CLI_PARSE_HELP);
    REQUIRE_FALSE(globalHelp.Options().filePath);
    REQUIRE_FALSE(globalHelp.HelpCommand());

    const ParsedInvocation buildHelp{"vxs", "build", "--help"};
    REQUIRE(buildHelp.Result() == XS_CLI_PARSE_HELP);
    REQUIRE(buildHelp.HelpCommand() == XS_CLI_COMMAND_BUILD);

    const ParsedInvocation version{"vxs", "version"};
    REQUIRE(version.Result() == XS_CLI_PARSE_VERSION);

    const ParsedInvocation versionHelp{"vxs", "version", "--help"};
    REQUIRE(versionHelp.Result() == XS_CLI_PARSE_HELP);
    REQUIRE(versionHelp.HelpCommand() == XS_CLI_COMMAND_VERSION);
}

TEST_CASE("compiler arguments are converted to typed values", "[cli][parser]")
{
    const ParsedInvocation parsed{
        "vxs",
        "build",
        "-File",
        "Program.vxs",
        "-Standard",
        "26",
        "-Compiler-Version",
        "0.3.1",
        "-Target",
        "x86_64-pc-windows-msvc",
        "-Emit",
        "llvmbc",
        "-Build",
        "vxs",
        "-Warnings",
        "all",
        "-Werror",
        "true",
        "-Wexperimental",
        "true",
        "-Wshadow",
        "true",
        "-Wundef",
        "false",
        "-Type-Safe-Format",
        "false",
        "-Backend",
        "llvm",
        "-Llvm-OptLevel",
        "g",
        "-Llvm-Compiler",
        "orc",
        "-Llvm-Lto",
        "thin",
        "-Xpp-Optimization-Passes",
        "false",
        "-Xmm-Optimization-Passes",
        "false",
    };

    REQUIRE(parsed.Result() == XS_CLI_PARSE_READY);
    const auto &options = parsed.Options();
    REQUIRE(options.command == XS_CLI_COMMAND_BUILD);
    REQUIRE(options.filePath == std::filesystem::path("Program.vxs"));
    REQUIRE(std::string(options.standard) == "26");
    REQUIRE(options.compilerVersion == "0.3.1");
    REQUIRE(options.target == "x86_64-pc-windows-msvc");
    REQUIRE(options.input == XS_BUILD_INPUT_VXS);
    REQUIRE(options.output == XS_BUILD_OUTPUT_LLVM_BC);
    REQUIRE(options.compiler.warning_level == XS_WARNING_ALL);
    REQUIRE(options.compiler.warnings_as_errors);
    REQUIRE(options.compiler.experimental_warnings);
    REQUIRE(options.compiler.shadow_warnings);
    REQUIRE_FALSE(options.compiler.undefined_warnings);
    REQUIRE_FALSE(options.compiler.type_safe_format);
    REQUIRE_FALSE(options.compiler.xpp_optimization_passes);
    REQUIRE_FALSE(options.compiler.xmm_optimization_passes);
    REQUIRE(options.compiler.llvm_opt_level == XS_LLVM_OPT_G);
    REQUIRE(options.compiler.llvm_compiler == XS_LLVM_COMPILER_ORC);
    REQUIRE(options.compiler.llvm_lto == XS_LLVM_LTO_THIN);
    REQUIRE(options.outputOverride);
    REQUIRE(options.compilerVersionOverride);
    REQUIRE(options.standardOverride);
    REQUIRE(options.targetOverride);
    REQUIRE(options.warningOverride);
    REQUIRE(options.werrorOverride);
    REQUIRE(options.llvmOptOverride);
}

TEST_CASE("command and option spellings are case-sensitive", "[cli][parser]")
{
    REQUIRE(ParsedInvocation{"vxs", "Build"}.Result() == XS_CLI_PARSE_ERROR);
    REQUIRE(ParsedInvocation{"vxs", "check", "-warnings", "all"}.Result() == XS_CLI_PARSE_ERROR);
    REQUIRE(ParsedInvocation{"vxs", "check", "-Warnings", "ALL"}.Result() == XS_CLI_PARSE_ERROR);
    REQUIRE(ParsedInvocation{"vxs", "check", "--module", "Sources"}.Result() == XS_CLI_PARSE_ERROR);
}

TEST_CASE("format and lint are project-wide tool commands", "[cli][parser][tools]")
{
    const ParsedInvocation format{"vxs", "format"};
    REQUIRE(format.Result() == XS_CLI_PARSE_READY);
    REQUIRE(format.Options().command == XS_CLI_COMMAND_FORMAT);
    REQUIRE_FALSE(format.Options().filePath);

    const ParsedInvocation lint{"vxs", "lint"};
    REQUIRE(lint.Result() == XS_CLI_PARSE_READY);
    REQUIRE(lint.Options().command == XS_CLI_COMMAND_LINT);
    REQUIRE_FALSE(lint.Options().filePath);

    REQUIRE(ParsedInvocation{"vxs", "format", "-File", "Program.vxs"}.Result() == XS_CLI_PARSE_ERROR);
    REQUIRE(ParsedInvocation{"vxs", "lint", "Program.vxs"}.Result() == XS_CLI_PARSE_ERROR);
}

TEST_CASE("schema enforces arity command scope and duplicate policy", "[cli][parser]")
{
    REQUIRE(ParsedInvocation{"vxs", "build", "-File"}.Result() == XS_CLI_PARSE_ERROR);
    REQUIRE(ParsedInvocation{"vxs", "check", "-Emit", "core"}.Result() == XS_CLI_PARSE_ERROR);
    REQUIRE(ParsedInvocation{"vxs", "check", "-File", "A.vxs", "-File", "B.vxs"}.Result() == XS_CLI_PARSE_ERROR);
    REQUIRE(ParsedInvocation{"vxs", "resolve", "-Warnings", "all"}.Result() == XS_CLI_PARSE_ERROR);
    REQUIRE(ParsedInvocation{"vxs", "version", "extra"}.Result() == XS_CLI_PARSE_ERROR);
}

TEST_CASE("every typed value domain rejects unknown values", "[cli][parser]")
{
    REQUIRE(ParsedInvocation{"vxs", "build", "-Emit", "hir"}.Result() == XS_CLI_PARSE_ERROR);
    REQUIRE(ParsedInvocation{"vxs", "check", "-Build", "source"}.Result() == XS_CLI_PARSE_ERROR);
    REQUIRE(ParsedInvocation{"vxs", "check", "-Standard", "23"}.Result() == XS_CLI_PARSE_ERROR);
    REQUIRE(ParsedInvocation{"vxs", "check", "-Werror", "yes"}.Result() == XS_CLI_PARSE_ERROR);
    REQUIRE(ParsedInvocation{"vxs", "check", "-Backend", "vpi"}.Result() == XS_CLI_PARSE_ERROR);
    REQUIRE(ParsedInvocation{"vxs", "check", "-Llvm-OptLevel", "0"}.Result() == XS_CLI_PARSE_ERROR);
    REQUIRE(ParsedInvocation{"vxs", "check", "-Llvm-Compiler", "jit"}.Result() == XS_CLI_PARSE_ERROR);
    REQUIRE(ParsedInvocation{"vxs", "check", "-Llvm-Lto", "full"}.Result() == XS_CLI_PARSE_ERROR);
    REQUIRE(ParsedInvocation{"vxs", "check", "-Target", "windows"}.Result() == XS_CLI_PARSE_ERROR);
    REQUIRE(ParsedInvocation{"vxs", "check", "-Target", "x86_64/windows/msvc"}.Result() == XS_CLI_PARSE_ERROR);

    const ParsedInvocation warning{"vxs", "check", "-Warnings", "urgent"};
    REQUIRE(warning.Diagnostic() == "invalid value 'urgent' for -Warnings; expected all|medium|low|none");
}

TEST_CASE("explicit artifact input cannot silently become project mode", "[cli][parser]")
{
    REQUIRE(ParsedInvocation{"vxs", "check", "-Build", "core"}.Result() == XS_CLI_PARSE_ERROR);

    const ParsedInvocation direct{"vxs", "check", "-Build", "core", "-File", "Module.core"};
    REQUIRE(direct.Result() == XS_CLI_PARSE_READY);
    REQUIRE(direct.Options().input == XS_BUILD_INPUT_CORE);
    REQUIRE(direct.Options().filePath == std::filesystem::path("Module.core"));
}

TEST_CASE("install and ViGet have distinct typed positional contracts", "[cli][parser]")
{
    const ParsedInvocation localInstall{"vxs", "install", "Publisher.Name"};
    REQUIRE(localInstall.Result() == XS_CLI_PARSE_READY);
    REQUIRE(localInstall.Options().command == XS_CLI_COMMAND_INSTALL);
    REQUIRE(localInstall.Options().packageCoordinate == "Publisher.Name");
    REQUIRE_FALSE(localInstall.Options().globalInstall);

    const ParsedInvocation globalInstall{"vxs", "install", "-Global", "Publisher.Name"};
    REQUIRE(globalInstall.Result() == XS_CLI_PARSE_READY);
    REQUIRE(globalInstall.Options().globalInstall);

    const ParsedInvocation push{"vxs", "viget", "push"};
    REQUIRE(push.Result() == XS_CLI_PARSE_READY);
    REQUIRE(push.Options().command == XS_CLI_COMMAND_VIGET);
    REQUIRE(push.Options().vigetAction == XS_VIGET_ACTION_PUSH);
    REQUIRE_FALSE(push.Options().packageCoordinate);

    const ParsedInvocation update{"vxs", "viget", "update"};
    REQUIRE(update.Result() == XS_CLI_PARSE_READY);
    REQUIRE(update.Options().vigetAction == XS_VIGET_ACTION_UPDATE);

    REQUIRE(ParsedInvocation{"vxs", "install"}.Result() == XS_CLI_PARSE_ERROR);
    REQUIRE(ParsedInvocation{"vxs", "viget"}.Result() == XS_CLI_PARSE_ERROR);
    REQUIRE(ParsedInvocation{"vxs", "viget", "publish"}.Result() == XS_CLI_PARSE_ERROR);
    REQUIRE(ParsedInvocation{"vxs", "install", "Publisher"}.Result() == XS_CLI_PARSE_ERROR);
    REQUIRE(ParsedInvocation{"vxs", "install", ".Name"}.Result() == XS_CLI_PARSE_ERROR);
    REQUIRE(ParsedInvocation{"vxs", "install", "Publisher..Name"}.Result() == XS_CLI_PARSE_ERROR);
    REQUIRE(ParsedInvocation{"vxs", "install", "Publisher.Name", "extra"}.Result() == XS_CLI_PARSE_ERROR);
}

TEST_CASE("project settings are overridden only by explicitly present CLI values", "[cli][parser]")
{
    const ParsedInvocation parsed{"vxs", "check", "-Wundef", "false", "-Llvm-Compiler", "orc"};
    REQUIRE(parsed.Result() == XS_CLI_PARSE_READY);

    XsCompilerSettings project{
        .warning_level = XS_WARNING_LOW,
        .warnings_as_errors = true,
        .experimental_warnings = true,
        .shadow_warnings = true,
        .undefined_warnings = true,
        .type_safe_format = false,
        .xpp_optimization_passes = false,
        .xmm_optimization_passes = false,
        .llvm_opt_level = XS_LLVM_OPT_3,
        .llvm_compiler = XS_LLVM_COMPILER_AOT,
        .llvm_lto = XS_LLVM_LTO_FAT,
    };
    xs_cli_apply_compiler_overrides(&parsed.Options(), &project);

    REQUIRE(project.warning_level == XS_WARNING_LOW);
    REQUIRE(project.warnings_as_errors);
    REQUIRE(project.experimental_warnings);
    REQUIRE(project.shadow_warnings);
    REQUIRE_FALSE(project.undefined_warnings);
    REQUIRE_FALSE(project.type_safe_format);
    REQUIRE_FALSE(project.xpp_optimization_passes);
    REQUIRE_FALSE(project.xmm_optimization_passes);
    REQUIRE(project.llvm_opt_level == XS_LLVM_OPT_3);
    REQUIRE(project.llvm_compiler == XS_LLVM_COMPILER_ORC);
    REQUIRE(project.llvm_lto == XS_LLVM_LTO_FAT);
}

TEST_CASE("effective compiler options follow CLI project and fallback precedence", "[cli][parser]")
{
    const ParsedInvocation fallbackInvocation{"vxs", "build"};
    REQUIRE(fallbackInvocation.Result() == XS_CLI_PARSE_READY);
    const auto fallback = ResolveCompilerOptions(fallbackInvocation.Options());
    REQUIRE(fallback.compilerVersion == "latest");
    REQUIRE(fallback.standard == "latest");
    REQUIRE_FALSE(fallback.target);
    REQUIRE(fallback.output == XS_BUILD_OUTPUT_BINARY);
    REQUIRE(fallback.compiler.llvm_opt_level == XS_LLVM_OPT_2);

    XsEffectiveCompilerOptions project{
        .compilerVersion = "0.3.1",
        .standard = "26",
        .target = std::nullopt,
        .output = XS_BUILD_OUTPUT_OBJECT,
        .compiler = xs_cli_default_compiler_settings(),
    };
    // Kotlin materializes explicit project settings and DSL defaults alike. Both
    // outrank CLI fallbacks; only argv presence bits can replace this layer.
    project.compiler.llvm_opt_level = XS_LLVM_OPT_0;
    const auto fromProject = ResolveCompilerOptions(fallbackInvocation.Options(), &project);
    REQUIRE(fromProject.compilerVersion == "0.3.1");
    REQUIRE(fromProject.standard == "26");
    REQUIRE(fromProject.output == XS_BUILD_OUTPUT_OBJECT);
    REQUIRE(fromProject.compiler.llvm_opt_level == XS_LLVM_OPT_0);

    const ParsedInvocation explicitInvocation{
        "vxs",   "build",  "-Compiler-Version", "latest", "-Standard", "latest", "-Target", "aarch64-unknown-linux-gnu",
        "-Emit", "llvmll", "-Llvm-OptLevel",    "3",
    };
    REQUIRE(explicitInvocation.Result() == XS_CLI_PARSE_READY);
    const auto explicitResult = ResolveCompilerOptions(explicitInvocation.Options(), &project);
    REQUIRE(explicitResult.compilerVersion == "latest");
    REQUIRE(explicitResult.standard == "latest");
    REQUIRE(explicitResult.target == "aarch64-unknown-linux-gnu");
    REQUIRE(explicitResult.output == XS_BUILD_OUTPUT_LLVM_LL);
    REQUIRE(explicitResult.compiler.llvm_opt_level == XS_LLVM_OPT_3);
}

TEST_CASE("parser rejects malformed process argument vectors safely", "[cli][parser]")
{
    REQUIRE(ParseCommandLine(-1, nullptr).result == XS_CLI_PARSE_ERROR);
    REQUIRE(ParseCommandLine(1, nullptr).result == XS_CLI_PARSE_ERROR);

    char program[] = "vxs";
    char *arguments[]{program, nullptr};
    REQUIRE(ParseCommandLine(2, arguments).result == XS_CLI_PARSE_ERROR);
}
