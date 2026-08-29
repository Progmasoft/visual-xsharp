// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

#include "Compiler/Linker/NativeLinker.hpp"

#include <cerrno>
#include <system_error>
#include <utility>

#ifdef _WIN32
#    include <process.h>
#endif

namespace Visual::XSharp::Driver
{
namespace
{
[[nodiscard]] auto ValidateRequest(const NativeLinkRequest &request) -> std::string
{
    // Validate the complete typed request before touching an older output. A bad
    // invocation must never destroy the last successfully linked executable.
    if(request.outputPath.empty() || request.outputPath.extension() != ".vxse")
        return "native executable output must use the .vxse extension";
    if(request.objectPaths.empty())
        return "native executable link requires at least one object file";
    if(request.objectFormat != Backend::LLVM::ObjectFormat::Coff)
        return "the Windows native linker requires COFF object input";
    for(const auto &object : request.objectPaths)
    {
        std::error_code error;
        if(object.extension() != ".o" || !std::filesystem::is_regular_file(object, error) || error)
            return "native executable link input is not a readable .o object file";
    }
    return {};
}
} // namespace

auto LinkNativeExecutable(const NativeLinkRequest &request) -> NativeLinkResult
{
    if(auto diagnostic = ValidateRequest(request); !diagnostic.empty())
        return {-1, std::move(diagnostic)};

#ifdef _WIN32
    std::error_code removeError;
    // Replacement starts only after all inputs pass validation. If LLD fails,
    // no stale `.vxse` remains that could be mistaken for the current build.
    std::filesystem::remove(request.outputPath, removeError);
    if(removeError)
        return {-1, "could not replace the existing native executable: " + removeError.message()};

    // The generated bridge is a freestanding PE entry and currently needs no CRT.
    // Avoiding implicit default libraries keeps the first executable boundary small,
    // deterministic, and independent from Visual Studio's compiler/linker binaries.
    const std::wstring program = L"lld-link.exe";
    std::vector<std::wstring> storage{program,
                                      L"/nologo",
                                      L"/entry:mainCRTStartup",
                                      L"/subsystem:console",
                                      L"/nodefaultlib",
                                      L"/out:" + request.outputPath.wstring()};
    storage.reserve(storage.size() + request.objectPaths.size());
    for(const auto &object : request.objectPaths)
        storage.push_back(object.wstring());
    std::vector<const wchar_t *> arguments;
    // Owning strings remain separate from argv pointers. Reserving both vectors
    // prevents growth from invalidating memory passed to `_wspawnvp`.
    arguments.reserve(storage.size() + 1U);
    for(const auto &argument : storage)
        arguments.push_back(argument.c_str());
    arguments.push_back(nullptr);

    const auto status = _wspawnvp(_P_WAIT, program.c_str(), arguments.data());
    // Waiting is deliberate: success cannot be reported, and `run` cannot begin,
    // until LLD has closed and finalized the PE image.
    if(status == -1)
        return {-1, "could not start lld-link: " + std::error_code(errno, std::generic_category()).message()};
    if(status != 0)
        return {static_cast<int>(status), "lld-link failed with exit code " + std::to_string(status)};

    std::error_code sizeError;
    const auto size = std::filesystem::file_size(request.outputPath, sizeError);
    if(sizeError || size == 0U)
        return {-1, "lld-link did not produce a non-empty .vxse executable"};
    return {0, {}};
#else
    return {-1, "native executable linking is currently implemented for Windows COFF targets"};
#endif
}
} // namespace Visual::XSharp::Driver
