// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

#include <cerrno>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fmt/format.h>
#include <fstream>
#include <limits>
#include <span>
#include <string_view>
#include <system_error>

#include "Compiler/ProjectSystem/Bridge/ProjectDriver.hpp"

#ifdef _WIN32
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <process.h>
#    include <windows.h>
#else
#    include <spawn.h>
#    include <sys/wait.h>
#    include <unistd.h>
extern char **environ;
#endif

namespace Visual::XSharp::Driver
{
    namespace
    {
        constexpr std::string_view kRegistryVersion = "visual-xsharp-sources-v5";
        constexpr std::size_t kHeaderRecordCount = 23;

#ifndef XS_PROJECT_EVALUATOR_CLASSPATH_DEFAULT
#    define XS_PROJECT_EVALUATOR_CLASSPATH_DEFAULT "libexec/xs/project/lib/*"
#endif

        class TemporaryRegistry final
        {
        public:
            TemporaryRegistry()
            {
                std::error_code error;
                const auto directory = std::filesystem::temp_directory_path(error);
                if (error)
                    return;
#ifdef _WIN32
                wchar_t candidate[MAX_PATH]{};
                if (GetTempFileNameW(directory.c_str(), L"xsr", 0, candidate) != 0)
                    path_ = candidate;
#else
                std::string candidate = (directory / "vxs-project-sources-XXXXXX").string();
                const int descriptor = mkstemp(candidate.data());
                if (descriptor >= 0)
                {
                    close(descriptor);
                    path_ = candidate;
                }
#endif
            }

            TemporaryRegistry(const TemporaryRegistry &) = delete;
            TemporaryRegistry &
            operator=(const TemporaryRegistry &) = delete;
            ~TemporaryRegistry()
            {
                std::error_code ignored;
                std::filesystem::remove(path_, ignored);
            }

            [[nodiscard]] explicit
            operator bool() const noexcept
            {
                return !path_.empty();
            }
            [[nodiscard]] const std::filesystem::path &
            Path() const noexcept
            {
                return path_;
            }

        private:
            std::filesystem::path path_;
        };

        [[nodiscard]] std::optional<std::filesystem::path>
        ExecutableDirectory()
        {
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

        [[nodiscard]] std::optional<std::string>
        Environment(const char *name)
        {
#ifdef _WIN32
            char *value{};
            std::size_t length{};
            if (_dupenv_s(&value, &length, name) != 0 || value == nullptr || length <= 1)
            {
                std::free(value);
                return std::nullopt;
            }
            std::string result(value, length - 1);
            std::free(value);
            return result;
#else
            const char *value = std::getenv(name);
            return value == nullptr || *value == '\0' ? std::nullopt : std::optional<std::string>(value);
#endif
        }

        [[nodiscard]] std::filesystem::path
        JavaProgram()
        {
            if (const auto javaHome = Environment("JAVA_HOME"))
            {
                auto executable = std::filesystem::path(*javaHome) / "bin" / "java";
#ifdef _WIN32
                executable.replace_extension(".exe");
#endif
                return executable;
            }
            return "java";
        }

        [[nodiscard]] std::string
        ProjectEvaluatorClasspath()
        {
            if (const auto configured = Environment("XS_PROJECT_EVALUATOR_CLASSPATH"))
                return *configured;
#ifdef XS_PROJECT_EVALUATOR_CLASSPATH_BUILD
            {
                const std::filesystem::path buildLibraryDirectory = std::filesystem::path(XS_PROJECT_EVALUATOR_CLASSPATH_BUILD).parent_path();
                std::error_code error;
                if (std::filesystem::is_directory(buildLibraryDirectory, error))
                    return XS_PROJECT_EVALUATOR_CLASSPATH_BUILD;
            }
#endif
            if (const auto directory = ExecutableDirectory())
            {
                const auto installed = *directory / ".." / "libexec" / "xs" / "project" / "lib";
                std::error_code error;
                if (std::filesystem::is_directory(installed, error))
                    return (installed.lexically_normal() / "*").string();
            }
            return XS_PROJECT_EVALUATOR_CLASSPATH_DEFAULT;
        }

        [[nodiscard]] int
        RunProgram(const std::filesystem::path &program, std::span<const std::string> arguments)
        {
#ifdef _WIN32
            std::vector<std::wstring> wideArguments;
            wideArguments.reserve(arguments.size() + 1);
            wideArguments.push_back(program.wstring());
            for (const auto &argument : arguments)
                wideArguments.emplace_back(std::filesystem::path(argument).wstring());
            std::vector<const wchar_t *> pointers;
            pointers.reserve(wideArguments.size() + 1);
            for (const auto &argument : wideArguments)
                pointers.push_back(argument.c_str());
            pointers.push_back(nullptr);
            const intptr_t status = _wspawnvp(_P_WAIT, program.c_str(), pointers.data());
            return status == -1 ? -1 : static_cast<int>(status);
#else
            const std::string executable = program.string();
            std::vector<std::string> storage;
            storage.reserve(arguments.size() + 1);
            storage.push_back(executable);
            storage.insert(storage.end(), arguments.begin(), arguments.end());
            std::vector<char *> pointers;
            pointers.reserve(storage.size() + 1);
            for (auto &argument : storage)
                pointers.push_back(argument.data());
            pointers.push_back(nullptr);
            pid_t process{};
            const int spawnStatus = posix_spawnp(&process, executable.c_str(), nullptr, nullptr, pointers.data(), environ);
            if (spawnStatus != 0)
            {
                errno = spawnStatus;
                return -1;
            }
            int status{};
            if (waitpid(process, &status, 0) < 0 || !WIFEXITED(status))
                return -1;
            return WEXITSTATUS(status);
#endif
        }

        [[nodiscard]] int
        RunProjectEvaluator(std::span<const std::string> commandArguments)
        {
            const auto program = JavaProgram();
            std::vector<std::string> arguments{
                "--enable-native-access=ALL-UNNAMED",
                "-cp",
                ProjectEvaluatorClasspath(),
                "com.progmasoft.visual.xsharp.project.MainKt",
            };
            arguments.insert(arguments.end(), commandArguments.begin(), commandArguments.end());
            const int status = RunProgram(program, arguments);
            if (status < 0)
            {
                fmt::print(stderr, "vxs: could not start bundled Kotlin project evaluator with '{}': {}\n", program.string(), std::error_code(errno, std::generic_category()).message());
            }
            return status;
        }

        [[nodiscard]] bool
        RunResolver(std::string_view mode, const std::filesystem::path &output)
        {
            const std::vector<std::string> arguments{ std::string(mode), ".", output.string() };
            return RunProjectEvaluator(arguments) == 0;
        }

        [[nodiscard]] std::optional<std::vector<char>>
        ReadRegistry(const std::filesystem::path &path)
        {
            std::ifstream input(path, std::ios::binary | std::ios::ate);
            if (!input)
                return std::nullopt;
            const auto end = static_cast<std::streamoff>(input.tellg());
            if (end <= 0 || static_cast<std::uintmax_t>(end) > std::numeric_limits<std::size_t>::max())
                return std::nullopt;
            std::vector<char> bytes(static_cast<std::size_t>(end));
            input.seekg(0);
            if (!input.read(bytes.data(), static_cast<std::streamsize>(bytes.size())) || bytes.back() != '\0')
                return std::nullopt;
            return bytes;
        }

        [[nodiscard]] std::optional<std::vector<std::string_view>>
        SplitRecords(const std::vector<char> &bytes)
        {
            std::vector<std::string_view> records;
            std::size_t start{};
            for (std::size_t index = 0; index < bytes.size(); ++index)
            {
                if (bytes[index] != '\0')
                    continue;
                records.emplace_back(bytes.data() + start, index - start);
                start = index + 1;
            }
            if (start != bytes.size())
                return std::nullopt;
            return records;
        }

        class RecordReader final
        {
        public:
            explicit RecordReader(std::span<const std::string_view> records)
                : records_(records)
            {}

            [[nodiscard]] std::optional<std::string_view>
            Next()
            {
                if (position_ == records_.size())
                    return std::nullopt;
                return records_[position_++];
            }

            [[nodiscard]] std::optional<std::size_t>
            Size()
            {
                const auto text = Next();
                if (!text || text->empty())
                    return std::nullopt;
                std::size_t result{};
                const auto conversion = std::from_chars(text->data(), text->data() + text->size(), result);
                if (conversion.ec != std::errc{} || conversion.ptr != text->data() + text->size())
                    return std::nullopt;
                return result;
            }

            [[nodiscard]] std::optional<bool>
            Boolean()
            {
                const auto text = Next();
                if (text == "true")
                    return true;
                if (text == "false")
                    return false;
                return std::nullopt;
            }

            [[nodiscard]] bool
            Complete() const noexcept
            {
                return position_ == records_.size();
            }

        private:
            std::span<const std::string_view> records_;
            std::size_t position_{};
        };

        [[nodiscard]] std::optional<XsBuildOutput>
        ParseOutput(std::string_view text)
        {
            constexpr std::string_view names[]{ "binary", "object", "core", "xpp", "xmm", "assembly", "llvm_ll", "llvm_bc" };
            for (std::size_t index = 0; index < std::size(names); ++index)
                if (text == names[index])
                    return static_cast<XsBuildOutput>(index);
            return std::nullopt;
        }

        [[nodiscard]] bool
        ParseCompilerHeader(RecordReader &reader, ResolvedProject &project)
        {
            const auto entry = reader.Next();
            const auto compilerVersion = reader.Next();
            const auto standard = reader.Next();
            const auto backend = reader.Next();
            const auto buildMode = reader.Next();
            const auto output = reader.Next();
            const auto warning = reader.Next();
            if (!entry || entry->empty() || !compilerVersion || !standard || backend != "llvm" || (buildMode != "debug" && buildMode != "release") || !output || !warning)
                return false;

            project.entry = *entry;
            project.compilerVersion = *compilerVersion;
            project.standard = *standard;
            project.settings = xs_cli_default_compiler_settings();
            const auto parsedOutput = ParseOutput(*output);
            if (!parsedOutput)
                return false;
            project.output = *parsedOutput;

            bool warningFound = false;
            for (int value = static_cast<int>(XS_WARNING_ALL); value <= static_cast<int>(XS_WARNING_NONE); ++value)
            {
                const auto level = static_cast<XsWarningLevel>(value);
                if (*warning == xs_cli_warning_level_name(level))
                {
                    project.settings.warning_level = level;
                    warningFound = true;
                    break;
                }
            }
            const auto warningsAsErrors = reader.Boolean();
            const auto experimental = reader.Boolean();
            const auto shadow = reader.Boolean();
            const auto undefined = reader.Boolean();
            const auto typeSafeFormat = reader.Boolean();
            const auto xpp = reader.Boolean();
            const auto xmm = reader.Boolean();
            const auto optLevel = reader.Next();
            const auto llvmCompiler = reader.Next();
            const auto lto = reader.Next();
            if (!warningFound || !warningsAsErrors || !experimental || !shadow || !undefined || !typeSafeFormat || !xpp || !xmm || !optLevel || !llvmCompiler || !lto)
                return false;
            project.settings.warnings_as_errors = *warningsAsErrors;
            project.settings.experimental_warnings = *experimental;
            project.settings.shadow_warnings = *shadow;
            project.settings.undefined_warnings = *undefined;
            project.settings.type_safe_format = *typeSafeFormat;
            project.settings.xpp_optimization_passes = *xpp;
            project.settings.xmm_optimization_passes = *xmm;

            if (*optLevel == "0")
                project.settings.llvm_opt_level = XS_LLVM_OPT_0;
            else if (*optLevel == "1")
                project.settings.llvm_opt_level = XS_LLVM_OPT_1;
            else if (*optLevel == "2")
                project.settings.llvm_opt_level = XS_LLVM_OPT_2;
            else if (*optLevel == "3")
                project.settings.llvm_opt_level = XS_LLVM_OPT_3;
            else if (*optLevel == "g")
                project.settings.llvm_opt_level = XS_LLVM_OPT_G;
            else
                return false;

            if (*llvmCompiler == "aot")
                project.settings.llvm_compiler = XS_LLVM_COMPILER_AOT;
            else if (*llvmCompiler == "orc")
                project.settings.llvm_compiler = XS_LLVM_COMPILER_ORC;
            else
                return false;

            if (*lto == "none")
                project.settings.llvm_lto = XS_LLVM_LTO_NONE;
            else if (*lto == "fat")
                project.settings.llvm_lto = XS_LLVM_LTO_FAT;
            else if (*lto == "thin")
                project.settings.llvm_lto = XS_LLVM_LTO_THIN;
            else
                return false;
            return true;
        }

        [[nodiscard]] std::optional<ResolvedProject>
        ParseRegistry(const std::vector<char> &bytes, bool requireSources)
        {
            const auto split = SplitRecords(bytes);
            if (!split || split->size() < kHeaderRecordCount || split->front() != kRegistryVersion)
                return std::nullopt;
            RecordReader reader(std::span(*split).subspan(1));
            ResolvedProject project;
            if (!ParseCompilerHeader(reader, project))
                return std::nullopt;
            const auto outputDirectory = reader.Next();
            const auto targetCount = reader.Size();
            const auto sourceCount = reader.Size();
            const auto sourceExcludeCount = reader.Size();
            const auto suiteCount = reader.Size();
            if (!outputDirectory || outputDirectory->empty() || !targetCount || !sourceCount || !sourceExcludeCount || !suiteCount || (requireSources && *sourceCount == 0))
                return std::nullopt;
            project.outputDirectory = *outputDirectory;

            project.targets.reserve(*targetCount);
            for (std::size_t index = 0; index < *targetCount; ++index)
            {
                const auto value = reader.Next();
                if (!value || value->empty())
                    return std::nullopt;
                project.targets.emplace_back(*value);
            }

            for (std::size_t index = 0; index < *sourceCount; ++index)
            {
                const auto value = reader.Next();
                if (!value || value->empty())
                    return std::nullopt;
                project.sourceRoots.emplace_back(*value);
            }
            for (std::size_t index = 0; index < *sourceExcludeCount; ++index)
            {
                const auto value = reader.Next();
                if (!value)
                    return std::nullopt;
                project.sourceExcludes.emplace_back(*value);
            }
            for (std::size_t index = 0; index < *suiteCount; ++index)
            {
                const auto name = reader.Next();
                const auto framework = reader.Next();
                const auto root = reader.Next();
                const auto excludeCount = reader.Size();
                if (!name || name->empty() || !framework || !root || root->empty() || !excludeCount)
                    return std::nullopt;
                ResolvedTestSuite suite;
                suite.name = *name;
                if (!framework->empty())
                    suite.framework = std::string(*framework);
                suite.root = *root;
                for (std::size_t exclude = 0; exclude < *excludeCount; ++exclude)
                {
                    const auto value = reader.Next();
                    if (!value)
                        return std::nullopt;
                    suite.excludes.emplace_back(*value);
                }
                project.testSuites.push_back(std::move(suite));
            }
            return reader.Complete() ? std::optional(std::move(project)) : std::nullopt;
        }
    } // namespace

    std::optional<ResolvedProject>
    ResolveProject(bool requireSources)
    {
        TemporaryRegistry registry;
        if (!registry)
        {
            fmt::print(stderr, "vxs: could not create the project source registry\n");
            return std::nullopt;
        }
        if (!RunResolver("sources0", registry.Path()))
            return std::nullopt;
        const auto bytes = ReadRegistry(registry.Path());
        if (!bytes)
        {
            fmt::print(stderr, "vxs: bundled project evaluator produced an invalid source registry\n");
            return std::nullopt;
        }
        auto project = ParseRegistry(*bytes, requireSources);
        if (!project)
            fmt::print(stderr, "vxs: bundled project evaluator returned invalid compiler/source/test-suite records\n");
        return project;
    }

    bool
    RefreshProjectLock()
    {
        const std::vector<std::string> arguments{ "resolve", "." };
        const int status = RunProjectEvaluator(arguments);
        if (status != 0)
            return false;
        fmt::print(stderr, "vxs: refreshed binary lock file 'Visual.XSharp.Lockfile.sqlite3'\n");
        return true;
    }
} // namespace Visual::XSharp::Driver
