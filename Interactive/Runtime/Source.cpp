// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#ifdef _WIN32
#    include <process.h>
#    include <random>
#    include <windows.h>
#else
#    include <spawn.h>
#    include <sys/wait.h>
#    include <unistd.h>
extern char **environ;
#endif

#include <fmt/format.h>

#include "Source.hpp"
#include "Value.hpp"

namespace Visual::XSharp::Interactive::Runtime
{
    namespace
    {
        constexpr std::uintmax_t kMaximumCoreBytes = 256U * 1024U * 1024U;

        [[nodiscard]] auto
        PathText(const std::filesystem::path &path) -> std::string
        {
#ifdef _WIN32
            const auto text = path.u8string();
            return std::string(reinterpret_cast<const char *>(text.data()),
                               text.size());
#else
            return path.string();
#endif
        }

#ifdef _WIN32
        [[nodiscard]] auto
        Utf8ToWide(std::string_view text) -> std::optional<std::wstring>
        {
            if (text.empty())
                return std::wstring{};
            const auto length
                = MultiByteToWideChar(CP_UTF8,
                                      MB_ERR_INVALID_CHARS,
                                      text.data(),
                                      static_cast<int>(text.size()),
                                      nullptr,
                                      0);
            if (length <= 0)
                return std::nullopt;
            std::wstring result(static_cast<std::size_t>(length), L'\0');
            if (MultiByteToWideChar(CP_UTF8,
                                    MB_ERR_INVALID_CHARS,
                                    text.data(),
                                    static_cast<int>(text.size()),
                                    result.data(),
                                    length)
                != length)
                return std::nullopt;
            return result;
        }

        [[nodiscard]] auto
        FrontendPath() -> std::optional<std::filesystem::path>
        {
            std::wstring buffer(32768U, L'\0');
            const auto length
                = GetModuleFileNameW(nullptr,
                                     buffer.data(),
                                     static_cast<DWORD>(buffer.size()));
            if (length == 0U || length >= buffer.size())
                return std::nullopt;
            buffer.resize(length);
            return std::filesystem::path(buffer).parent_path()
                   / L"vxs-frontend.exe";
        }
#else
        [[nodiscard]] auto
        FrontendPath() -> std::optional<std::filesystem::path>
        {
            std::error_code error;
            auto executable
                = std::filesystem::read_symlink("/proc/self/exe", error);
            if (error)
                return std::nullopt;
            return executable.parent_path() / "vxs-frontend";
        }
#endif
    } // namespace

    ScratchCell::ScratchCell()
    {
        std::error_code error;
        const auto root = std::filesystem::temp_directory_path(error);
        if (error)
            return;

        // A Windows directory creation is atomic. Keep the retry bounded, and
        // treat entropy-provider failures as an unavailable cell rather than
        // unwinding out of the interactive command loop.
#ifdef _WIN32
        try
        {
            std::random_device entropy;
            for (unsigned attempt = 0U; attempt < 64U; ++attempt)
            {
                const auto suffix
                    = fmt::format("{:08x}{:08x}", entropy(), entropy());
                auto candidate = root / ("vxsi-" + suffix);
                error.clear();
                if (std::filesystem::create_directory(candidate, error))
                {
                    directory_ = std::move(candidate);
                    source_ = directory_ / "Cell.vxs";
                    core_ = directory_ / "Cell.core";
                    return;
                }
                if (error && error != std::errc::file_exists)
                    return;
            }
        }
        catch (const std::exception &)
        {
            return;
        }
#else
        // mkdtemp atomically reserves a mode-0700 directory. A predictable
        // filename under a shared /tmp would expose source and Core scratch
        // artifacts to other local accounts.
        auto pattern = (root / "vxsi-XXXXXX").string();
        std::vector<char> writable(pattern.begin(), pattern.end());
        writable.push_back('\0');
        if (::mkdtemp(writable.data()) == nullptr)
            return;
        directory_ = writable.data();
        source_ = directory_ / "Cell.vxs";
        core_ = directory_ / "Cell.core";
#endif
    }

    ScratchCell::~ScratchCell()
    {
        if (directory_.empty())
            return;
        std::error_code ignored;
        std::filesystem::remove_all(directory_, ignored);
    }

    auto
    ScratchCell::Valid() const noexcept -> bool
    {
        return !directory_.empty();
    }

    auto
    ScratchCell::SourcePath() const noexcept -> const std::filesystem::path &
    {
        return source_;
    }

    auto
    ScratchCell::CorePath() const noexcept -> const std::filesystem::path &
    {
        return core_;
    }

    auto
    WriteCellSource(const ScratchCell &cell,
                    std::uint64_t cellNumber,
                    std::string_view expression,
                    const std::optional<Backend::LLVM::JitValue> &previous)
        -> std::optional<std::string>
    {
        if (!cell.Valid())
            return "could not reserve a private temporary directory for the "
                   "input cell";
        if (expression.empty())
            return "an empty line is not an expression";
        if (expression.size() > 1024U * 1024U)
            return "one Visual X# expression cannot exceed 1 MiB";

        std::ofstream output(cell.SourcePath(),
                             std::ios::binary | std::ios::trunc);
        if (!output)
            return "could not create the temporary Visual X# source file";
        output << "namespace VisualXSharp.Interactive.Cell" << cellNumber
               << ";\n"
               << "class Session {\n"
               << "    public static auto Evaluate() {\n";
        if (previous)
        {
            auto binding = SourceBinding(*previous);
            if (binding)
                output << "        " << *binding << '\n';
        }
        output << "        " << expression << "\n"
               << "    }\n"
               << "}\n";
        output.close();
        if (!output)
            return "could not finish writing the temporary Visual X# source "
                   "file";
        return std::nullopt;
    }

    auto
    RunFrontend(const std::filesystem::path &source,
                const std::filesystem::path &core) -> int
    {
        const auto frontend = FrontendPath();
        if (!frontend)
        {
            fmt::print(stderr,
                       "vxsi: could not locate the executable directory for "
                       "vxs-frontend\n");
            return -1;
        }
        const auto outputText = PathText(core);
        const auto sourceText = PathText(source);
#ifdef _WIN32
        std::vector<std::wstring> storage;
        storage.reserve(5U);
        storage.push_back(frontend->wstring());
        for (const auto &argument : { std::string("--output"),
                                      outputText,
                                      std::string("--source-file"),
                                      sourceText })
        {
            auto wide = Utf8ToWide(argument);
            if (!wide)
            {
                fmt::print(
                    stderr,
                    "vxsi: generated frontend path is not valid UTF-8\n");
                return -1;
            }
            storage.push_back(std::move(*wide));
        }
        std::vector<const wchar_t *> arguments;
        arguments.reserve(storage.size() + 1U);
        for (const auto &argument : storage)
            arguments.push_back(argument.c_str());
        arguments.push_back(nullptr);
        const auto status
            = _wspawnv(_P_WAIT, frontend->c_str(), arguments.data());
        if (status == -1)
            fmt::print(stderr,
                       "vxsi: could not start the Visual X# Haskell frontend "
                       "(error {})\n",
                       errno);
        return static_cast<int>(status);
#else
        std::vector<std::string> storage{ frontend->string(),
                                          "--output",
                                          outputText,
                                          "--source-file",
                                          sourceText };
        std::vector<char *> arguments;
        arguments.reserve(storage.size() + 1U);
        for (auto &argument : storage)
            arguments.push_back(argument.data());
        arguments.push_back(nullptr);
        pid_t process{};
        const auto status = posix_spawn(&process,
                                        storage.front().c_str(),
                                        nullptr,
                                        nullptr,
                                        arguments.data(),
                                        environ);
        if (status != 0)
        {
            fmt::print(stderr,
                       "vxsi: could not start the Visual X# Haskell frontend "
                       "(error {})\n",
                       status);
            return -1;
        }
        int result{};
        if (waitpid(process, &result, 0) < 0)
            return -1;
        return WIFEXITED(result) ? WEXITSTATUS(result) : -1;
#endif
    }

    auto
    ReadCore(const std::filesystem::path &path)
        -> std::optional<std::vector<std::uint8_t>>
    {
        std::error_code error;
        const auto size = std::filesystem::file_size(path, error);
        if (error || size == 0U || size > kMaximumCoreBytes)
            return std::nullopt;
        std::ifstream input(path, std::ios::binary);
        if (!input)
            return std::nullopt;
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
        input.read(reinterpret_cast<char *>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
        if (!input || input.peek() != std::char_traits<char>::eof())
            return std::nullopt;
        return bytes;
    }

    auto
    EvaluationSymbol(const visual_xsharp::xmm::Module &module,
                     std::uint64_t cellNumber) -> std::optional<std::string>
    {
        auto expectedCell = std::u32string(U"Cell");
        for (const auto digit : std::to_string(cellNumber))
            expectedCell.push_back(static_cast<char32_t>(digit));
        const std::vector<std::u32string> expectedName{ U"VisualXSharp",
                                                        U"Interactive",
                                                        std::move(
                                                            expectedCell) };
        if (module.name != expectedName)
            return std::nullopt;
        const visual_xsharp::xmm::Function *evaluation{};
        for (const auto &function : module.functions)
        {
            if (function.symbol.spelling != U"Evaluate")
                continue;
            if (evaluation != nullptr)
                return std::nullopt;
            evaluation = &function;
        }
        if (evaluation == nullptr || !evaluation->parameter_types.empty())
            return std::nullopt;

        std::string symbol;
        for (const auto &part : module.name)
        {
            if (!symbol.empty())
                symbol.push_back('.');
            for (const auto scalar : part)
            {
                if (scalar > 0x7fU)
                    return std::nullopt;
                symbol.push_back(static_cast<char>(scalar));
            }
        }
        symbol.append(".Evaluate.");
        symbol.append(std::to_string(evaluation->symbol.id));
        return symbol;
    }
} // namespace Visual::XSharp::Interactive::Runtime
