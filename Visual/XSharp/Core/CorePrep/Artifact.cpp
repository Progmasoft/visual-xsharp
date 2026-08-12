// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

#include "Visual/XSharp/Core/CorePrep/Artifact.hpp"

#include <fstream>
#include <limits>

namespace visual_xsharp::core
{
namespace
{
auto error(ArtifactErrorKind kind, const std::filesystem::path &path, std::string message,
           std::optional<wire::Error> wire_error = {}) -> ArtifactError
{
    return ArtifactError{kind, path, std::move(message), std::move(wire_error)};
}

auto valid_path(const std::filesystem::path &path) -> bool
{
    return path.has_filename() && path.extension() == ".core";
}

auto read_bytes(const std::filesystem::path &path, const wire::Limits &limits) -> ArtifactBytesResult
{
    if(!valid_path(path))
        return {{}, error(ArtifactErrorKind::InvalidExtension, path, "CorePrep artifact path must end in .core")};

    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if(!stream)
        return {{}, error(ArtifactErrorKind::OpenFailed, path, "could not open CorePrep artifact for reading")};
    const auto end = stream.tellg();
    if(end < 0)
        return {{}, error(ArtifactErrorKind::ReadFailed, path, "could not determine CorePrep artifact length")};
    const auto unsigned_size = static_cast<std::uintmax_t>(end);
    if(unsigned_size > limits.maximum_wire_bytes || unsigned_size > std::numeric_limits<std::size_t>::max())
        return {{}, error(ArtifactErrorKind::ReadFailed, path, "CorePrep artifact exceeds configured byte limit")};

    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(unsigned_size));
    stream.seekg(0, std::ios::beg);
    if(!bytes.empty())
        stream.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if(!stream)
        return {{}, error(ArtifactErrorKind::ReadFailed, path, "could not read the complete CorePrep artifact")};
    return {std::move(bytes), {}};
}
} // namespace

auto read_coreprep_artifact(const std::filesystem::path &path, const wire::Limits &limits) -> ArtifactModuleResult
{
    auto loaded = read_bytes(path, limits);
    if(!loaded)
        return {{}, std::move(loaded.error)};
    auto decoded = wire::decode(loaded.bytes, limits);
    if(!decoded)
        return {{}, error(ArtifactErrorKind::WireError, path, "CorePrep artifact is malformed", std::move(decoded.error))};
    return {std::move(decoded.module), {}};
}

auto write_coreprep_artifact(const std::filesystem::path &path, const CorePrepModule &module,
                             const wire::Limits &limits) -> std::optional<ArtifactError>
{
    if(!valid_path(path))
        return error(ArtifactErrorKind::InvalidExtension, path, "CorePrep artifact path must end in .core");
    auto encoded = wire::encode(module, limits);
    if(!encoded)
        return error(ArtifactErrorKind::WireError, path, "CorePrep module cannot be encoded", std::move(encoded.error));

    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if(!stream)
        return error(ArtifactErrorKind::OpenFailed, path, "could not open CorePrep artifact for writing");
    if(!encoded.bytes.empty())
        stream.write(reinterpret_cast<const char *>(encoded.bytes.data()),
                     static_cast<std::streamsize>(encoded.bytes.size()));
    stream.flush();
    if(!stream)
        return error(ArtifactErrorKind::WriteFailed, path, "could not write the complete CorePrep artifact");
    return {};
}
} // namespace visual_xsharp::core
