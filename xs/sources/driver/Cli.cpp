// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

#include "Visual/XSharp/driver.hh"

#include "ProjectDriver.hpp"
#include "core_pipeline.h"
#include "options.h"

#include <fmt/format.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

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
        if(error)
            return;
#ifdef _WIN32
        wchar_t candidate[MAX_PATH]{};
        if(GetTempFileNameW(directory.c_str(), L"vxc", 0, candidate) == 0)
            return;
        path_ = candidate;
        path_.replace_extension(L".core");
        std::filesystem::remove(candidate, error);
#else
        path_ = directory / ("vxs-" + std::to_string(static_cast<unsigned long long>(std::rand())) + ".core");
#endif
    }

    TemporaryCore(const TemporaryCore &) = delete;
    TemporaryCore &operator=(const TemporaryCore &) = delete;
    ~TemporaryCore()
    {
        std::error_code ignored;
        std::filesystem::remove(path_, ignored);
    }

    [[nodiscard]] const std::filesystem::path &Path() const noexcept
    {
        return path_;
    }
    [[nodiscard]] explicit operator bool() const noexcept
    {
        return !path_.empty();
    }

private:
    std::filesystem::path path_;
};

[[nodiscard]] std::optional<std::filesystem::path> ExecutableDirectory()
{
    // Installed and build-tree layouts place the private Haskell frontend next
    // to vxs. Resolve from the running image, never from cwd or PATH, so a project
    // cannot substitute a different compiler stage by dropping in an executable.
#ifdef _WIN32
    std::wstring buffer(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if(length == 0 || length >= buffer.size())
        return std::nullopt;
    buffer.resize(length);
    return std::filesystem::path(buffer).parent_path();
#else
    std::error_code error;
    const auto executable = std::filesystem::read_symlink("/proc/self/exe", error);
    return error ? std::nullopt : std::optional(executable.parent_path());
#endif
}

[[nodiscard]] int RunFrontend(const std::filesystem::path &output, const std::filesystem::path &source)
{
    auto directory = ExecutableDirectory();
    if(!directory)
    {
        std::fputs("vxs: could not locate the compiler executable directory\n", stderr);
        return -1;
    }
#ifdef _WIN32
    // _wspawnv passes an argument vector directly; no shell quoting or command
    // interpolation is involved, including for non-ASCII and spaced paths.
    const auto frontend = *directory / "vxs-frontend.exe";
    const std::wstring outputText = output.wstring();
    const std::wstring sourceText = source.wstring();
    const wchar_t *arguments[] = {frontend.c_str(), L"--output", outputText.c_str(), sourceText.c_str(), nullptr};
    const intptr_t status = _wspawnv(_P_WAIT, frontend.c_str(), arguments);
    if(status == -1)
        fmt::print(stderr, "vxs: could not start Haskell frontend: {}\n",
                   std::error_code(errno, std::generic_category()).message());
    return static_cast<int>(status);
#else
    const auto frontend = *directory / "vxs-frontend";
    const std::string frontendText = frontend.string();
    const std::string outputText = output.string();
    const std::string sourceText = source.string();
    char *arguments[] = {frontendText.data(), const_cast<char *>("--output"), outputText.data(), sourceText.data(),
                         nullptr};
    pid_t process{};
    const int spawnStatus = posix_spawn(&process, frontendText.c_str(), nullptr, nullptr, arguments, environ);
    if(spawnStatus != 0)
    {
        std::fprintf(stderr, "vxs: could not start Haskell frontend: %s\n", std::strerror(spawnStatus));
        return -1;
    }
    int status{};
    if(waitpid(process, &status, 0) < 0)
        return -1;
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
}

[[nodiscard]] bool HasSuffix(std::string_view value, std::string_view suffix)
{
    return value.size() >= suffix.size() && value.substr(value.size() - suffix.size()) == suffix;
}

[[nodiscard]] std::filesystem::path OutputPath(const std::filesystem::path &input, std::string_view extension)
{
    // Artifact naming is a filesystem operation, not a C string manipulation
    // concern. replace_extension handles dotted directories and both separators.
    auto output = input;
    output.replace_extension(extension);
    return output;
}

[[nodiscard]] bool CopyCore(const std::filesystem::path &temporary, const char *source)
{
    const auto output = OutputPath(source, ".core");
    std::error_code error;
    std::filesystem::copy_file(temporary, output, std::filesystem::copy_options::overwrite_existing, error);
    if(error)
    {
        std::fprintf(stderr, "vxs: could not write Core artifact '%s': %s\n", output.string().c_str(),
                     error.message().c_str());
        return false;
    }
    std::fprintf(stderr, "vxs: wrote '%s'\n", output.string().c_str());
    return true;
}

[[nodiscard]] bool ProcessSource(const char *source, const XsCliOptions &options, XsCompilerSettings settings)
{
    if(source == nullptr || !HasSuffix(source, ".vxs"))
    {
        std::fputs("vxs: Haskell frontend input must be a .vxs file\n", stderr);
        return false;
    }
    TemporaryCore core;
    if(!core)
    {
        std::fputs("vxs: could not allocate a temporary Core artifact\n", stderr);
        return false;
    }
    if(RunFrontend(core.Path(), source) != 0)
        return false;
    // Every source command crosses the same verified Core consumer. `check` and
    // artifact emission therefore cannot drift into separate validation paths.
    if(std::string_view(options.command) == "check")
        return xs_driver_process_core_artifact_as(core.Path().string().c_str(), source, "check", options.output,
                                                  &settings);
    if(std::string_view(options.command) != "build")
    {
        std::fputs("vxs: run and test will be reconnected after native object/link ownership moves to C++20\n", stderr);
        return false;
    }
    if(options.output == XS_BUILD_OUTPUT_CORE)
        return CopyCore(core.Path(), source);
    if(options.output == XS_BUILD_OUTPUT_LLVM_LL || options.output == XS_BUILD_OUTPUT_LLVM_BC)
        return xs_driver_process_core_artifact_as(core.Path().string().c_str(), source, "build", options.output,
                                                  &settings);
    std::fputs("vxs: source builds currently emit core, llvmll, or llvmbc during the frontend migration\n", stderr);
    return false;
}

[[nodiscard]] int RunFile(const XsCliOptions &options)
{
    XsCompilerSettings settings = xs_cli_default_compiler_settings();
    xs_cli_apply_compiler_overrides(&options, &settings);
    if(options.input == XS_BUILD_INPUT_CORE)
    {
        if(options.file_path == nullptr || !HasSuffix(options.file_path, ".core"))
        {
            std::fputs("vxs: -Build core requires a .core -File\n", stderr);
            return 2;
        }
        return xs_driver_process_core_artifact(options.file_path, options.command, options.output, &settings) ? 0 : 1;
    }
    if(options.input != XS_BUILD_INPUT_VXS)
    {
        std::fputs("vxs: only vxs and core inputs belong to the renewed pipeline\n", stderr);
        return 2;
    }
    return ProcessSource(options.file_path, options, settings) ? 0 : 1;
}

[[nodiscard]] int RunProject(const XsCliOptions &options)
{
    const bool testing = std::string_view(options.command) == "test";
    auto project = Visual::XSharp::Driver::ResolveProject(!testing);
    if(!project)
        return 1;
    // Kotlin deliberately returns source roots and exclusion policy without
    // walking Visual X# files. The Haskell module loader must discover units and
    // resolve `entry` by namespace/type identity; interpreting a root or entry as
    // a file path here would reintroduce the retired layout convention.
    std::fprintf(stderr,
                 "vxs: project compilation requires the Haskell source-root loader; entry '%s' is not a file path\n",
                 project->entry.c_str());
    return 1;
}
} // namespace

extern "C" int xs_driver_main(int argc, char **argv)
{
    XsCliOptions options{};
    const auto parsed = xs_cli_parse(argc, argv, &options);
    if(parsed != XS_CLI_PARSE_READY)
        return parsed == XS_CLI_PARSE_EXIT ? 0 : 2;
    int result{};
    const std::string_view command(options.command);
    if(command == "resolve" || command == "update")
        result = Visual::XSharp::Driver::RefreshProjectLock() ? 0 : 1;
    else if(command == "install" || command == "viget")
    {
        std::fprintf(stderr, "vxs: %.*s requires the ViGet client, which is not linked into this build yet\n",
                     static_cast<int>(command.size()), command.data());
        result = 1;
    }
    else
        result = options.file_path == nullptr ? RunProject(options) : RunFile(options);
    xs_cli_options_free(&options);
    return result;
}
