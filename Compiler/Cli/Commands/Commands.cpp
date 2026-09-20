// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

#include <algorithm>
#include <cerrno>
#include <cstdio>
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
#include "Compiler/Cli/Commands/ExecutionStatus.hpp"
#include "Compiler/Cli/Presentation/Activity.hpp"
#include "Compiler/Driver/CorePipeline.hpp"
#include "Compiler/Linker/NativeLinker.hpp"
#include "Compiler/ProjectSystem/Bridge/ProjectDriver.hpp"

#ifdef _WIN32
#    include <process.h>
#    include <windows.h>
#else
#    include <spawn.h>
#    include <sys/wait.h>
#    include <unistd.h>
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
            // GetTempFileName creates the candidate atomically. Rename that
            // reserved file without MOVEFILE_REPLACE_EXISTING so the required
            // .core suffix remains reserved throughout the hand-off.
            for (unsigned attempt = 0U; attempt < 128U; ++attempt)
            {
                wchar_t candidate[MAX_PATH]{};
                if (GetTempFileNameW(directory.c_str(), L"vxc", 0, candidate) == 0)
                    return;
                auto corePath = std::filesystem::path(candidate).replace_extension(L".core");
                if (MoveFileExW(candidate, corePath.c_str(), MOVEFILE_WRITE_THROUGH) != 0)
                {
                    path_ = std::move(corePath);
                    return;
                }
                std::filesystem::remove(candidate, error);
                error.clear();
            }
#else
            // mkstemps preserves the semantic suffix while providing O_EXCL
            // creation. std::rand-based names allowed another user/process to
            // pre-create the Core hand-off path between selection and write.
            auto pattern = (directory / "vxs-XXXXXX.core").string();
            std::vector<char> candidate(pattern.begin(), pattern.end());
            candidate.push_back('\0');
            const auto descriptor = mkstemps(candidate.data(), 5);
            if (descriptor < 0)
                return;
            close(descriptor);
            path_ = candidate.data();
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
    RunProjectTool(CliCommand command, bool formatterDryRun)
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

        const bool formatting = command == CliCommand::kFormat;
#ifdef _WIN32
        const std::string executable = formatting ? "vfmt.exe" : "vlint.exe";
#else
        const std::string executable = formatting ? "vfmt" : "vlint";
#endif
        if (formatting)
        {
            // Project formatting is one tool invocation. Besides avoiding repeated
            // Kotlin startup, this gives Visual Formatter one authoritative DSL
            // snapshot for the complete source set.
            std::vector<std::string> arguments;
            arguments.emplace_back(formatterDryRun ? "-Dry-Run" : "-In-Place");
            arguments.reserve(sources->size() + 1);
            for (const auto &source : *sources)
                arguments.push_back(PathText(source));
            const int status = RunInstalledTool(executable, arguments);
            if (status == -1)
            {
                fmt::print(stderr, "vxs: {} is not installed or is not available on PATH; install Progmasoft.VisualFormatter\n", executable);
                return 1;
            }
            return status == 0 ? 0 : 1;
        }

        bool succeeded = true;
        for (const auto &source : *sources)
        {
            // Child tools inherit the project-root working directory. Their
            // canonical Visual.Formatter.kts or Visual.Linter.kts lookup therefore
            // has one project-wide owner; when the corresponding file is absent,
            // the installed tool applies its own defaults.
            std::vector<std::string> arguments;
            arguments.push_back(PathText(source));
            const int status = RunInstalledTool(executable, arguments);
            if (status == -1)
            {
                fmt::print(stderr, "vxs: {} is not installed or is not available on PATH; install Progmasoft.VisualLinter\n", executable);
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
    ExecuteNative(const std::filesystem::path &executable, std::span<const std::string> arguments)
    {
        // Check the exact artifact produced by this invocation. This prevents `run`
        // from starting an older executable after a failed build.
        std::error_code error;
        if (!std::filesystem::is_regular_file(executable, error) || error)
        {
            fmt::print(stderr, "vxs: native executable '{}' was not produced\n", PathText(executable));
            return 1;
        }
        const int status = RunInstalledTool(PathText(executable), arguments);
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
        return execute ? ExecuteNative(executable, {}) : 0;
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

    [[nodiscard]] int
    ProcessSource(const std::filesystem::path &source, const CliOptions &options, const EffectiveCompilerOptions &effective)
    {
        if (source.extension() != ".vxs")
        {
            fmt::print(stderr, "vxs: Haskell frontend input must be a .vxs file\n");
            return 1;
        }
        TemporaryCore core;
        if (!core)
        {
            fmt::print(stderr, "vxs: could not allocate a temporary Core artifact\n");
            return 1;
        }
        if (RunFileFrontend(core.Path(), source) != 0)
            return 1;
        const auto sourceText = PathText(source);
        // Every source command crosses the same verified Core consumer. `check` and
        // artifact emission therefore cannot drift into separate validation paths.
        if (options.command == CliCommand::kCheck)
            return Visual::XSharp::Cli::ExecutionStatus::Resolve({
                ProcessCoreArtifactAs(core.Path().string().c_str(), sourceText.c_str(), options.command, effective.output, &effective.compiler, effective.target ? effective.target->c_str() : nullptr),
                std::nullopt,
            });
        if (options.command != CliCommand::kBuild && options.command != CliCommand::kRun)
        {
            fmt::print(stderr, "vxs: this source command is not connected to the compiler pipeline\n");
            return 1;
        }
        const auto output = options.command == CliCommand::kRun ? BuildOutput::kBinary : effective.output;
        if (output == BuildOutput::kCore)
            return Visual::XSharp::Cli::ExecutionStatus::Resolve({ CopyCore(core.Path(), source), std::nullopt });
        const bool built = ProcessCoreArtifactAs(core.Path().string().c_str(), sourceText.c_str(), options.command, output, &effective.compiler, effective.target ? effective.target->c_str() : nullptr);
        if (!built)
            return Visual::XSharp::Cli::ExecutionStatus::Resolve({ false, std::nullopt });
        const auto nativeStatus = options.command == CliCommand::kRun
                                      ? std::optional<int>{ ExecuteNative(OutputPath(source, ".vxse"), options.programArguments) }
                                      : std::nullopt;
        return Visual::XSharp::Cli::ExecutionStatus::Resolve({ true, nativeStatus });
    }

    [[nodiscard]] int
    RunFile(const CliOptions &options)
    {
        const auto effective = ResolveCompilerOptions(options);
        if (options.input == BuildInput::kObject)
        {
            // Checking machine code would bypass all language and IR verifiers;
            // object input therefore belongs only to explicit build operations.
            if (options.command != CliCommand::kBuild)
            {
                fmt::print(stderr, "vxs: check does not accept native object input\n");
                return 2;
            }
            return options.filePath ? LinkObjectInput(*options.filePath, false) : 2;
        }
        if (options.input == BuildInput::kCore)
        {
            if (!options.filePath || options.filePath->extension() != ".core")
            {
                fmt::print(stderr, "vxs: -Build core requires a .core -File\n");
                return 2;
            }
            const auto fileText = PathText(*options.filePath);
            const auto output = options.command == CliCommand::kRun ? BuildOutput::kBinary : effective.output;
            if (!ProcessCoreArtifact(fileText.c_str(), options.command, output, &effective.compiler, effective.target ? effective.target->c_str() : nullptr))
                return 1;
            return options.command == CliCommand::kRun ? ExecuteNative(OutputPath(*options.filePath, ".vxse"), options.programArguments) : 0;
        }
        if (options.input == BuildInput::kXpp || options.input == BuildInput::kXmm)
        {
            const bool isXpp = options.input == BuildInput::kXpp;
            const std::string_view expectedExtension = isXpp ? ".xpp" : ".xmm";
            if (!options.filePath || options.filePath->extension() != expectedExtension)
            {
                fmt::print(stderr, "vxs: -Build {} requires a {} -File\n", isXpp ? "xpp" : "xmm", expectedExtension);
                return 2;
            }
            const auto output = options.command == CliCommand::kRun ? BuildOutput::kBinary : effective.output;
            if (output == BuildOutput::kCore || (!isXpp && output == BuildOutput::kXpp))
            {
                fmt::print(stderr, "vxs: compiler artifacts cannot be raised back to an earlier pipeline stage\n");
                return 2;
            }
            const auto fileText = PathText(*options.filePath);
            const bool built = isXpp
                                   ? ProcessXppArtifactAs(fileText.c_str(), fileText.c_str(), options.command, output, &effective.compiler, effective.target ? effective.target->c_str() : nullptr)
                                   : ProcessXmmArtifactAs(fileText.c_str(), fileText.c_str(), options.command, output, &effective.compiler, effective.target ? effective.target->c_str() : nullptr);
            if (!built)
                return 1;
            return options.command == CliCommand::kRun ? ExecuteNative(OutputPath(*options.filePath, ".vxse"), options.programArguments) : 0;
        }
        if (options.input != BuildInput::kVisualXSharp)
        {
            fmt::print(stderr, "vxs: only vxs, core, xpp, and xmm inputs belong to the renewed pipeline\n");
            return 2;
        }
        return options.filePath ? ProcessSource(*options.filePath, options, effective) : 1;
    }

    [[nodiscard]] int
    RunExecutableTarget(const CliOptions &options,
                        Visual::XSharp::Driver::ResolvedProject project,
                        const Visual::XSharp::Driver::ResolvedSourceTarget &target,
                        const EffectiveCompilerOptions &effective)
    {
        project.entry = *target.entry;
        project.sourceRoots = { target.root };
        project.sourceExcludes = target.excludes;

        TemporaryCore core;
        if (!core)
        {
            fmt::print(stderr, "vxs: could not allocate a temporary Core artifact\n");
            return 1;
        }
        if (RunProjectFrontend(core.Path(), project) != 0)
            return 1;

        std::error_code pathError;
        const auto workingDirectory = std::filesystem::current_path(pathError);
        if (pathError)
        {
            fmt::print(stderr, "vxs: could not resolve the project artifact directory: {}\n", pathError.message());
            return 1;
        }
        const auto artifactBase = workingDirectory / project.outputDirectory / target.name;
        const auto corePathText = core.Path().string();
        const auto artifactBaseText = artifactBase.string();
        if (options.command == CliCommand::kCheck)
            return ProcessCoreArtifactAs(corePathText.c_str(), artifactBaseText.c_str(), options.command, effective.output, &effective.compiler, effective.target ? effective.target->c_str() : nullptr)
                       ? 0
                       : 1;
        std::filesystem::create_directories(artifactBase.parent_path(), pathError);
        if (pathError)
        {
            fmt::print(stderr, "vxs: could not create project artifact directory '{}': {}\n", PathText(artifactBase.parent_path()), pathError.message());
            return 1;
        }
        const auto output = options.command == CliCommand::kRun ? BuildOutput::kBinary : effective.output;
        if (output == BuildOutput::kCore)
            return CopyCore(core.Path(), artifactBase) ? 0 : 1;
        if (!ProcessCoreArtifactAs(corePathText.c_str(), artifactBaseText.c_str(), options.command, output, &effective.compiler, effective.target ? effective.target->c_str() : nullptr))
            return 1;
        return options.command == CliCommand::kRun ? ExecuteNative(OutputPath(artifactBase, ".vxse"), options.programArguments) : 0;
    }

    [[nodiscard]] int
    RunProject(const CliOptions &options)
    {
        const bool testing = options.command == CliCommand::kTest;
        auto project = Visual::XSharp::Driver::ResolveProject(!testing);
        if (!project)
            return 1;
        if (testing)
        {
            fmt::print(stderr,
                       "vxs: named test-suite execution requires the test framework runner, which is not linked yet\n");
            return 1;
        }
        if (!options.selectedViPkgs.empty())
        {
            fmt::print(stderr,
                       "vxs: -ViPkg selection requires the ViPkg assembly stage, which is not linked yet\n");
            return 1;
        }
        if (!options.selectedLibraries.empty())
        {
            fmt::print(stderr,
                       "vxs: library target selection requires the library frontend route, which is not linked yet\n");
            return 1;
        }

        const EffectiveCompilerOptions projectDefaults{
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
        if (options.command == CliCommand::kBuild && (effective.output == BuildOutput::kObject || effective.output == BuildOutput::kAssembly))
        {
            // A project Core module intentionally combines declarations across files. Until
            // Core carries source ownership, emitting one object and pretending it belongs
            // to every source would violate the source-per-artifact naming contract.
            fmt::print(stderr,
                       "vxs: project object and assembly emission require source ownership in Core; binary emission is "
                       "available now\n");
            return 1;
        }

        if (options.command != CliCommand::kBuild && options.command != CliCommand::kRun
            && options.command != CliCommand::kCheck)
        {
            fmt::print(stderr, "vxs: this project command is not connected to the compiler pipeline\n");
            return 1;
        }
        std::vector<const Visual::XSharp::Driver::ResolvedSourceTarget *> selected;
        if (options.selectedExecutables.empty())
        {
            for (const auto &target : project->executables)
                selected.push_back(&target);
        }
        else
        {
            for (const auto &name : options.selectedExecutables)
            {
                const auto found = std::find_if(project->executables.begin(), project->executables.end(), [&](const auto &target) {
                    return target.name == name;
                });
                if (found == project->executables.end())
                {
                    fmt::print(stderr, "vxs: executable target '{}' is not declared by Visual.XSharp.kts\n", name);
                    return 2;
                }
                selected.push_back(&*found);
            }
        }
        if (selected.empty())
        {
            fmt::print(stderr, "vxs: project does not declare an executable target\n");
            return 1;
        }
        for (const auto *target : selected)
            if (RunExecutableTarget(options, *project, *target, effective) != 0)
                return 1;
        return 0;
    }
} // namespace

auto
Visual::XSharp::Cli::Run(int argc, char **argv) -> int
{
    auto parsed = ParseCommandLine(argc, argv);
    if (parsed.result == CliParseResult::kHelp)
    {
        PrintCliHelp(parsed.helpCommand);
        return 0;
    }
    if (parsed.result == CliParseResult::kVersion)
    {
        PrintCliVersion();
        return 0;
    }
    if (parsed.result == CliParseResult::kError)
    {
        fmt::print(stderr, "vxs: {}\n", parsed.diagnostic);
        return 2;
    }
    const auto &options = parsed.options;
    std::optional<Activity> activity;
    if (options.command == CliCommand::kBuild)
        activity.emplace("building compiler pipeline");
    else if (options.command == CliCommand::kCheck)
        activity.emplace("checking compiler pipeline");
    int result{};
    if (options.command == CliCommand::kResolve || options.command == CliCommand::kUpdate)
        result = Visual::XSharp::Driver::RefreshProjectLock() ? 0 : 1;
    else if (options.command == CliCommand::kFormat || options.command == CliCommand::kLint)
        result = RunProjectTool(options.command, options.formatterDryRun);
    else if (options.command == CliCommand::kInstall || options.command == CliCommand::kViGet
             || options.command == CliCommand::kViPkg)
    {
        const char *commandName = options.command == CliCommand::kInstall
                                      ? "install"
                                  : options.command == CliCommand::kViGet ? "viget"
                                                                          : "vipkg";
        fmt::print(stderr, "vxs: {} requires the ViGet client, which is not linked into this build yet\n", commandName);
        result = 1;
    }
    else
        result = options.filePath ? RunFile(options) : RunProject(options);
    if (activity)
    {
        if (result == 0)
            activity->Complete(options.command == CliCommand::kBuild ? "build completed" : "check completed");
        else
            activity->Fail(options.command == CliCommand::kBuild ? "build failed" : "check failed");
    }
    return result;
}
