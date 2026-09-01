// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fmt/format.h>
#include <fstream>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "Compiler/Cli/Arguments/Options.hpp"
#include "Compiler/Cli/Commands/Commands.hpp"
#include "Compiler/Driver/CorePipeline.hpp"
#include "Compiler/Linker/NativeLinker.hpp"
#include "Compiler/ProjectSystem/Bridge/ProjectDriver.hpp"

#ifdef _WIN32
#    include <process.h>
#    include <windows.h>
#else
#    include <spawn.h>
#    include <sys/wait.h>
extern char **environ;
#endif

namespace
{
    class TemporaryCore final
    {
    public:
        TemporaryCore()
        {
            // The frontend/backend hand-off is private and short-lived. A unique OS
            // temporary file avoids exposing CorePrep or creating project artifacts
            // during `check`, while the destructor provides one cleanup owner.
            std::error_code error;
            const auto directory = std::filesystem::temp_directory_path(error);
            if (error)
                return;
#ifdef _WIN32
            wchar_t candidate[MAX_PATH]{};
            if (GetTempFileNameW(directory.c_str(), L"vxc", 0, candidate) == 0)
                return;
            path_ = candidate;
            path_.replace_extension(L".core");
            std::filesystem::remove(candidate, error);
#else
            path_ = directory / ("vxs-" + std::to_string(static_cast<unsigned long long>(std::rand())) + ".core");
#endif
        }

        TemporaryCore(const TemporaryCore &) = delete;
        TemporaryCore &
        operator=(const TemporaryCore &) = delete;
        ~TemporaryCore()
        {
            std::error_code ignored;
            std::filesystem::remove(path_, ignored);
        }

        [[nodiscard]] const std::filesystem::path &
        Path() const noexcept
        {
            return path_;
        }
        [[nodiscard]] explicit
        operator bool() const noexcept
        {
            return !path_.empty();
        }

    private:
        std::filesystem::path path_;
    };

    [[nodiscard]] std::optional<std::filesystem::path>
    ExecutableDirectory()
    {
        // Installed and build-tree layouts place the private Haskell frontend next
        // to vxs. Resolve from the running image, never from cwd or PATH, so a project
        // cannot substitute a different compiler stage by dropping in an executable.
#ifdef _WIN32
        std::wstring buffer(32768, L'\0');
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0 || length >= buffer.size())
            return std::nullopt;
        buffer.resize(length);
        return std::filesystem::path(buffer).parent_path();
#else
        std::error_code error;
        const auto executable = std::filesystem::read_symlink("/proc/self/exe", error);
        return error ? std::nullopt : std::optional(executable.parent_path());
#endif
    }

    [[nodiscard]] std::string
    PathText(const std::filesystem::path &path)
    {
#ifdef _WIN32
        const auto text = path.u8string();
        return std::string(reinterpret_cast<const char *>(text.data()), text.size());
#else
        return path.string();
#endif
    }

#ifdef _WIN32
    [[nodiscard]] std::optional<std::wstring>
    Utf8ToWide(std::string_view text)
    {
        if (text.empty())
            return std::wstring{};
        const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
        if (length <= 0)
            return std::nullopt;
        std::wstring result(static_cast<std::size_t>(length), L'\0');
        if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), result.data(), length) != length)
            return std::nullopt;
        return result;
    }
#endif

    [[nodiscard]] int
    RunFrontend(std::span<const std::string> commandArguments)
    {
        auto directory = ExecutableDirectory();
        if (!directory)
        {
            fmt::print(stderr, "vxs: could not locate the compiler executable directory\n");
            return -1;
        }
#ifdef _WIN32
        // _wspawnv passes an argument vector directly; no shell quoting or glob
        // expansion is involved. Convert the private UTF-8 protocol explicitly so
        // namespace and filesystem characters do not depend on the active codepage.
        const auto frontend = *directory / "vxs-frontend.exe";
        std::vector<std::wstring> storage;
        storage.reserve(commandArguments.size() + 1);
        storage.push_back(frontend.wstring());
        for (const auto &argument : commandArguments)
        {
            auto wide = Utf8ToWide(argument);
            if (!wide)
            {
                fmt::print(stderr, "vxs: private frontend argument is not valid UTF-8\n");
                return -1;
            }
            storage.push_back(std::move(*wide));
        }
        std::vector<const wchar_t *> arguments;
        arguments.reserve(storage.size() + 1);
        for (const auto &argument : storage)
            arguments.push_back(argument.c_str());
        arguments.push_back(nullptr);
        const intptr_t status = _wspawnv(_P_WAIT, frontend.c_str(), arguments.data());
        if (status == -1)
            fmt::print(stderr, "vxs: could not start Haskell frontend: {}\n", std::error_code(errno, std::generic_category()).message());
        return static_cast<int>(status);
#else
        const auto frontend = *directory / "vxs-frontend";
        const std::string frontendText = frontend.string();
        std::vector<std::string> storage;
        storage.reserve(commandArguments.size() + 1);
        storage.push_back(frontendText);
        storage.insert(storage.end(), commandArguments.begin(), commandArguments.end());
        std::vector<char *> arguments;
        arguments.reserve(storage.size() + 1);
        for (auto &argument : storage)
            arguments.push_back(argument.data());
        arguments.push_back(nullptr);
        pid_t process{};
        const int spawnStatus = posix_spawn(&process, frontendText.c_str(), nullptr, nullptr, arguments.data(), environ);
        if (spawnStatus != 0)
        {
            fmt::print(stderr, "vxs: could not start Haskell frontend: {}\n", std::strerror(spawnStatus));
            return -1;
        }
        int status{};
        if (waitpid(process, &status, 0) < 0)
            return -1;
        return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
    }

    [[nodiscard]] int
    RunFileFrontend(const std::filesystem::path &output, const std::filesystem::path &source)
    {
        const std::vector<std::string> arguments{ "--output", PathText(output), "--source-file", PathText(source) };
        return RunFrontend(arguments);
    }

    [[nodiscard]] int
    RunProjectFrontend(const std::filesystem::path &output,
                       const Visual::XSharp::Driver::ResolvedProject &project)
    {
        std::error_code error;
        const auto projectRoot = std::filesystem::current_path(error);
        if (error)
        {
            fmt::print(stderr, "vxs: could not resolve the project working directory: {}\n", error.message());
            return -1;
        }
        std::vector<std::string> arguments{ "--output", PathText(output), "--project-root", PathText(projectRoot), "--entry", project.entry };
        arguments.reserve(arguments.size() + project.sourceRoots.size() * 2 + project.sourceExcludes.size() * 2);
        for (const auto &root : project.sourceRoots)
        {
            arguments.push_back("--source-root");
            arguments.push_back(PathText(root));
        }
        for (const auto &pattern : project.sourceExcludes)
        {
            arguments.push_back("--exclude");
            arguments.push_back(pattern);
        }
        return RunFrontend(arguments);
    }

    [[nodiscard]] int
    WriteProjectSourceList(const std::filesystem::path &output,
                           const Visual::XSharp::Driver::ResolvedProject &project)
    {
        std::error_code error;
        const auto projectRoot = std::filesystem::current_path(error);
        if (error)
        {
            fmt::print(stderr, "vxs: could not resolve the project working directory: {}\n", error.message());
            return -1;
        }
        std::vector<std::string> arguments{ "--output", PathText(output), "--project-root", PathText(projectRoot), "--list-sources" };
        arguments.reserve(arguments.size() + project.sourceRoots.size() * 2 + project.sourceExcludes.size() * 2);
        for (const auto &root : project.sourceRoots)
        {
            arguments.push_back("--source-root");
            arguments.push_back(PathText(root));
        }
        for (const auto &pattern : project.sourceExcludes)
        {
            arguments.push_back("--exclude");
            arguments.push_back(pattern);
        }
        return RunFrontend(arguments);
    }

    [[nodiscard]] std::optional<std::vector<std::filesystem::path>>
    ReadProjectSourceList(const std::filesystem::path &path)
    {
        std::ifstream stream(path, std::ios::binary);
        if (!stream)
            return std::nullopt;
        const std::string bytes{ std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() };
        std::vector<std::filesystem::path> sources;
        std::size_t offset{};
        while (offset < bytes.size())
        {
            const auto end = bytes.find('\0', offset);
            if (end == std::string::npos)
                return std::nullopt;
            const std::string_view encoded(bytes.data() + offset, end - offset);
#ifdef _WIN32
            auto wide = Utf8ToWide(encoded);
            if (!wide)
                return std::nullopt;
            sources.emplace_back(std::move(*wide));
#else
            sources.emplace_back(encoded);
#endif
            offset = end + 1;
        }
        return sources;
    }

    [[nodiscard]] int
    RunInstalledTool(std::string_view executable, std::span<const std::string> commandArguments)
    {
#ifdef _WIN32
        auto executableWide = Utf8ToWide(executable);
        if (!executableWide)
            return -1;
        std::vector<std::wstring> storage;
        storage.reserve(commandArguments.size() + 1);
        storage.push_back(*executableWide);
        for (const auto &argument : commandArguments)
        {
            auto wide = Utf8ToWide(argument);
            if (!wide)
                return -1;
            storage.push_back(std::move(*wide));
        }
        std::vector<const wchar_t *> arguments;
        arguments.reserve(storage.size() + 1);
        for (const auto &argument : storage)
            arguments.push_back(argument.c_str());
        arguments.push_back(nullptr);
        return static_cast<int>(_wspawnvp(_P_WAIT, executableWide->c_str(), arguments.data()));
#else
        std::vector<std::string> storage;
        storage.reserve(commandArguments.size() + 1);
        storage.emplace_back(executable);
        storage.insert(storage.end(), commandArguments.begin(), commandArguments.end());
        std::vector<char *> arguments;
        arguments.reserve(storage.size() + 1);
        for (auto &argument : storage)
            arguments.push_back(argument.data());
        arguments.push_back(nullptr);
        pid_t process{};
        const int spawnStatus = posix_spawnp(&process, storage.front().c_str(), nullptr, nullptr, arguments.data(), environ);
        if (spawnStatus != 0)
            return -1;
        int status{};
        if (waitpid(process, &status, 0) < 0)
            return -1;
        return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
    }

    [[nodiscard]] int
    RunProjectTool(XsCliCommand command)
    {
        auto project = Visual::XSharp::Driver::ResolveProject(true);
        if (!project)
            return 1;
        TemporaryCore sourceList;
        if (!sourceList || WriteProjectSourceList(sourceList.Path(), *project) != 0)
            return 1;
        auto sources = ReadProjectSourceList(sourceList.Path());
        if (!sources)
        {
            fmt::print(stderr, "vxs: compiler frontend returned an invalid project source list\n");
            return 1;
        }

        const bool formatting = command == XS_CLI_COMMAND_FORMAT;
#ifdef _WIN32
        const std::string executable = formatting ? "vfmt.exe" : "vlint.exe";
#else
        const std::string executable = formatting ? "vfmt" : "vlint";
#endif
        bool succeeded = true;
        for (const auto &source : *sources)
        {
            // Child tools inherit the project-root working directory. Their
            // canonical Visual.Formatter.kts or Visual.Linter.kts lookup therefore
            // has one project-wide owner; when the corresponding file is absent,
            // the installed tool applies its own defaults.
            std::vector<std::string> arguments;
            if (formatting)
                arguments.emplace_back("-In-Place");
            arguments.push_back(PathText(source));
            const int status = RunInstalledTool(executable, arguments);
            if (status == -1)
            {
                fmt::print(stderr, "vxs: {} is not installed or is not available on PATH; install {}\n", executable, formatting ? "Progmasoft.VisualFormatter" : "Progmasoft.VisualLinter");
                return 1;
            }
            succeeded = status == 0 && succeeded;
        }
        return succeeded ? 0 : 1;
    }

    [[nodiscard]] std::filesystem::path
    OutputPath(const std::filesystem::path &input, std::string_view extension)
    {
        // Artifact naming is a filesystem operation, not a C string manipulation
        // concern. replace_extension handles dotted directories and both separators.
        auto output = input;
        output.replace_extension(extension);
        return output;
    }

    [[nodiscard]] int
    ExecuteNative(const std::filesystem::path &executable)
    {
        // Check the exact artifact produced by this invocation. This prevents `run`
        // from starting an older executable after a failed build.
        std::error_code error;
        if (!std::filesystem::is_regular_file(executable, error) || error)
        {
            fmt::print(stderr, "vxs: native executable '{}' was not produced\n", PathText(executable));
            return 1;
        }
        const int status = RunInstalledTool(PathText(executable), {});
        if (status == -1)
        {
            fmt::print(stderr, "vxs: could not start native executable '{}': {}\n", PathText(executable), std::error_code(errno, std::generic_category()).message());
            return 1;
        }
        if (status != 0)
            fmt::print(stderr, "vxs: native executable exited with status {}\n", status);
        return status;
    }

    [[nodiscard]] int
    LinkObjectInput(const std::filesystem::path &object, bool execute)
    {
        // Direct object input is an advanced link route and accepts the canonical
        // Visual X# `.o` spelling rather than a host-specific `.obj` alias.
        if (object.extension() != ".o")
        {
            fmt::print(stderr, "vxs: -Build object requires a .o -File\n");
            return 2;
        }
        const auto executable = OutputPath(object, ".vxse");
        const Visual::XSharp::Driver::NativeLinkRequest request{
            executable,
            { object },
            Visual::XSharp::Backend::LLVM::ObjectFormat::Coff
        };
        const auto linked = Visual::XSharp::Driver::LinkNativeExecutable(request);
        if (!linked)
        {
            fmt::print(stderr, "vxs: native link failed: {}\n", linked.diagnostic);
            return 1;
        }
        fmt::print(stderr, "vxs: linked native executable '{}'\n", PathText(executable));
        return execute ? ExecuteNative(executable) : 0;
    }

    [[nodiscard]] bool
    CopyCore(const std::filesystem::path &temporary, const std::filesystem::path &source)
    {
        const auto output = OutputPath(source, ".core");
        std::error_code error;
        std::filesystem::copy_file(temporary, output, std::filesystem::copy_options::overwrite_existing, error);
        if (error)
        {
            fmt::print(stderr, "vxs: could not write Core artifact '{}': {}\n", PathText(output), error.message());
            return false;
        }
        fmt::print(stderr, "vxs: wrote '{}'\n", PathText(output));
        return true;
    }

    [[nodiscard]] bool
    ProcessSource(const std::filesystem::path &source, const XsCliOptions &options, const XsEffectiveCompilerOptions &effective)
    {
        if (source.extension() != ".vxs")
        {
            fmt::print(stderr, "vxs: Haskell frontend input must be a .vxs file\n");
            return false;
        }
        TemporaryCore core;
        if (!core)
        {
            fmt::print(stderr, "vxs: could not allocate a temporary Core artifact\n");
            return false;
        }
        if (RunFileFrontend(core.Path(), source) != 0)
            return false;
        const auto sourceText = PathText(source);
        // Every source command crosses the same verified Core consumer. `check` and
        // artifact emission therefore cannot drift into separate validation paths.
        if (options.command == XS_CLI_COMMAND_CHECK)
            return xs_driver_process_core_artifact_as(core.Path().string().c_str(), sourceText.c_str(), options.command, effective.output, &effective.compiler, effective.target ? effective.target->c_str() : nullptr);
        if (options.command != XS_CLI_COMMAND_BUILD && options.command != XS_CLI_COMMAND_RUN)
        {
            fmt::print(stderr, "vxs: this source command is not connected to the compiler pipeline\n");
            return false;
        }
        const auto output = options.command == XS_CLI_COMMAND_RUN ? XS_BUILD_OUTPUT_BINARY : effective.output;
        if (output == XS_BUILD_OUTPUT_CORE)
            return CopyCore(core.Path(), source);
        const bool built = xs_driver_process_core_artifact_as(core.Path().string().c_str(), sourceText.c_str(), options.command, output, &effective.compiler, effective.target ? effective.target->c_str() : nullptr);
        if (!built)
            return false;
        return options.command != XS_CLI_COMMAND_RUN || ExecuteNative(OutputPath(source, ".vxse")) == 0;
    }

    [[nodiscard]] int
    RunFile(const XsCliOptions &options)
    {
        const auto effective = ResolveCompilerOptions(options);
        if (options.input == XS_BUILD_INPUT_OBJECT)
        {
            // Checking machine code would bypass all language and IR verifiers;
            // object input therefore belongs only to explicit build operations.
            if (options.command != XS_CLI_COMMAND_BUILD)
            {
                fmt::print(stderr, "vxs: check does not accept native object input\n");
                return 2;
            }
            return options.filePath ? LinkObjectInput(*options.filePath, false) : 2;
        }
        if (options.input == XS_BUILD_INPUT_CORE)
        {
            if (!options.filePath || options.filePath->extension() != ".core")
            {
                fmt::print(stderr, "vxs: -Build core requires a .core -File\n");
                return 2;
            }
            const auto fileText = PathText(*options.filePath);
            const auto output = options.command == XS_CLI_COMMAND_RUN ? XS_BUILD_OUTPUT_BINARY : effective.output;
            if (!xs_driver_process_core_artifact(fileText.c_str(), options.command, output, &effective.compiler, effective.target ? effective.target->c_str() : nullptr))
                return 1;
            return options.command == XS_CLI_COMMAND_RUN ? ExecuteNative(OutputPath(*options.filePath, ".vxse")) : 0;
        }
        if (options.input == XS_BUILD_INPUT_XPP || options.input == XS_BUILD_INPUT_XMM)
        {
            const bool isXpp = options.input == XS_BUILD_INPUT_XPP;
            const std::string_view expectedExtension = isXpp ? ".xpp" : ".xmm";
            if (!options.filePath || options.filePath->extension() != expectedExtension)
            {
                fmt::print(stderr, "vxs: -Build {} requires a {} -File\n", isXpp ? "xpp" : "xmm", expectedExtension);
                return 2;
            }
            const auto output = options.command == XS_CLI_COMMAND_RUN ? XS_BUILD_OUTPUT_BINARY : effective.output;
            if (output == XS_BUILD_OUTPUT_CORE || (!isXpp && output == XS_BUILD_OUTPUT_XPP))
            {
                fmt::print(stderr, "vxs: compiler artifacts cannot be raised back to an earlier pipeline stage\n");
                return 2;
            }
            const auto fileText = PathText(*options.filePath);
            const bool built = isXpp
                                   ? xs_driver_process_xpp_artifact_as(fileText.c_str(), fileText.c_str(), options.command, output, &effective.compiler, effective.target ? effective.target->c_str() : nullptr)
                                   : xs_driver_process_xmm_artifact_as(fileText.c_str(), fileText.c_str(), options.command, output, &effective.compiler, effective.target ? effective.target->c_str() : nullptr);
            if (!built)
                return 1;
            return options.command == XS_CLI_COMMAND_RUN ? ExecuteNative(OutputPath(*options.filePath, ".vxse")) : 0;
        }
        if (options.input != XS_BUILD_INPUT_VXS)
        {
            fmt::print(stderr, "vxs: only vxs, core, xpp, and xmm inputs belong to the renewed pipeline\n");
            return 2;
        }
        return options.filePath && ProcessSource(*options.filePath, options, effective) ? 0 : 1;
    }

    [[nodiscard]] int
    RunProject(const XsCliOptions &options)
    {
        const bool testing = options.command == XS_CLI_COMMAND_TEST;
        auto project = Visual::XSharp::Driver::ResolveProject(!testing);
        if (!project)
            return 1;
        if (testing)
        {
            fmt::print(stderr,
                       "vxs: named test-suite execution requires the test framework runner, which is not linked yet\n");
            return 1;
        }

        const XsEffectiveCompilerOptions projectDefaults{
            .compilerVersion = project->compilerVersion,
            .standard = project->standard,
            .target = std::nullopt,
            .output = project->output,
            .compiler = project->settings,
        };
        const auto effective = ResolveCompilerOptions(options, &projectDefaults);
        if (effective.target && !project->targets.empty() && std::find(project->targets.begin(), project->targets.end(), *effective.target) == project->targets.end())
        {
            fmt::print(stderr, "vxs: target '{}' is not declared by Visual.XSharp.kts\n", *effective.target);
            return 2;
        }
        if (options.command == XS_CLI_COMMAND_BUILD && (effective.output == XS_BUILD_OUTPUT_OBJECT || effective.output == XS_BUILD_OUTPUT_ASSEMBLY))
        {
            // A project Core module intentionally combines declarations across files. Until
            // Core carries source ownership, emitting one object and pretending it belongs
            // to every source would violate the source-per-artifact naming contract.
            fmt::print(stderr,
                       "vxs: project object and assembly emission require source ownership in Core; binary emission is "
                       "available now\n");
            return 1;
        }

        TemporaryCore core;
        if (!core)
        {
            fmt::print(stderr, "vxs: could not allocate a temporary Core artifact\n");
            return 1;
        }
        if (RunProjectFrontend(core.Path(), *project) != 0)
            return 1;

        const auto separator = project->entry.find_last_of('.');
        const std::string className = separator == std::string::npos ? project->entry : project->entry.substr(separator + 1);
        std::error_code pathError;
        const auto workingDirectory = std::filesystem::current_path(pathError);
        if (pathError)
        {
            fmt::print(stderr, "vxs: could not resolve the project artifact directory: {}\n", pathError.message());
            return 1;
        }
        const auto artifactBase = workingDirectory / project->outputDirectory / className;
        const auto corePathText = core.Path().string();
        const auto artifactBaseText = artifactBase.string();
        if (options.command == XS_CLI_COMMAND_CHECK)
            return xs_driver_process_core_artifact_as(corePathText.c_str(), artifactBaseText.c_str(), options.command, effective.output, &effective.compiler, effective.target ? effective.target->c_str() : nullptr)
                       ? 0
                       : 1;
        if (options.command != XS_CLI_COMMAND_BUILD && options.command != XS_CLI_COMMAND_RUN)
        {
            fmt::print(stderr, "vxs: this project command is not connected to the compiler pipeline\n");
            return 1;
        }
        std::filesystem::create_directories(artifactBase.parent_path(), pathError);
        if (pathError)
        {
            fmt::print(stderr, "vxs: could not create project artifact directory '{}': {}\n", PathText(artifactBase.parent_path()), pathError.message());
            return 1;
        }
        // `run` always requests a fresh runnable artifact, regardless of a project's
        // ordinary emit preference, and then executes precisely that output path.
        const auto output = options.command == XS_CLI_COMMAND_RUN ? XS_BUILD_OUTPUT_BINARY : effective.output;
        if (output == XS_BUILD_OUTPUT_CORE)
            return CopyCore(core.Path(), artifactBase) ? 0 : 1;
        if (!xs_driver_process_core_artifact_as(corePathText.c_str(), artifactBaseText.c_str(), options.command, output, &effective.compiler, effective.target ? effective.target->c_str() : nullptr))
            return 1;
        return options.command == XS_CLI_COMMAND_RUN ? ExecuteNative(OutputPath(artifactBase, ".vxse")) : 0;
    }
} // namespace

auto
Visual::XSharp::Cli::Run(int argc, char **argv) -> int
{
    auto parsed = ParseCommandLine(argc, argv);
    if (parsed.result == XS_CLI_PARSE_HELP)
    {
        PrintCliHelp(parsed.helpCommand);
        return 0;
    }
    if (parsed.result == XS_CLI_PARSE_VERSION)
    {
        PrintCliVersion();
        return 0;
    }
    if (parsed.result == XS_CLI_PARSE_ERROR)
    {
        fmt::print(stderr, "vxs: {}\n", parsed.diagnostic);
        return 2;
    }
    const auto &options = parsed.options;
    int result{};
    if (options.command == XS_CLI_COMMAND_RESOLVE || options.command == XS_CLI_COMMAND_UPDATE)
        result = Visual::XSharp::Driver::RefreshProjectLock() ? 0 : 1;
    else if (options.command == XS_CLI_COMMAND_FORMAT || options.command == XS_CLI_COMMAND_LINT)
        result = RunProjectTool(options.command);
    else if (options.command == XS_CLI_COMMAND_INSTALL || options.command == XS_CLI_COMMAND_VIGET)
    {
        const char *commandName = options.command == XS_CLI_COMMAND_INSTALL ? "install" : "viget";
        fmt::print(stderr, "vxs: {} requires the ViGet client, which is not linked into this build yet\n", commandName);
        result = 1;
    }
    else
        result = options.filePath ? RunFile(options) : RunProject(options);
    return result;
}
