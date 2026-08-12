// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

#include "Visual/XSharp/Backend/LLVM.hpp"

#include <fstream>
#include <system_error>

namespace Visual::XSharp::Backend::LLVM
{
namespace
{
[[nodiscard]] auto FileError(const std::filesystem::path &path, std::string_view action) -> Error
{
    return Error{ErrorKind::FileSystem, "VXL3001",
                 "could not " + std::string(action) + " LLVM artifact '" + path.string() + "'", {}};
}

[[nodiscard]] auto ExtensionError(const std::filesystem::path &path, std::string_view expected) -> Error
{
    return Error{ErrorKind::FileSystem, "VXL3002",
                 "LLVM artifact '" + path.string() + "' must use the " + std::string(expected) + " extension", {}};
}
} // namespace

auto WriteLlvmIr(const std::filesystem::path &path, std::string_view llvmIr) -> std::optional<Error>
{
    if(path.extension() != ".ll")
        return ExtensionError(path, ".ll");
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if(!stream)
        return FileError(path, "open");
    stream.write(llvmIr.data(), static_cast<std::streamsize>(llvmIr.size()));
    if(!stream)
        return FileError(path, "write");
    stream.close();
    if(!stream)
        return FileError(path, "close");
    return std::nullopt;
}

auto WriteBitcode(const std::filesystem::path &path, const std::vector<std::uint8_t> &bitcode)
    -> std::optional<Error>
{
    if(path.extension() != ".bc")
        return ExtensionError(path, ".bc");
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if(!stream)
        return FileError(path, "open");
    stream.write(reinterpret_cast<const char *>(bitcode.data()), static_cast<std::streamsize>(bitcode.size()));
    if(!stream)
        return FileError(path, "write");
    stream.close();
    if(!stream)
        return FileError(path, "close");
    return std::nullopt;
}
} // namespace Visual::XSharp::Backend::LLVM
