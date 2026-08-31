// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "Visual/XSharp/Core/IR.hpp"

namespace Visual::XSharp::Core::Wire
{
    inline constexpr std::uint16_t kCurrentVersion = 3;

    struct Limits final
    {
        std::size_t maximumWireBytes{ 64U * 1024U * 1024U };
        std::size_t maximumTextScalars{ 1024U * 1024U };
        std::size_t maximumFunctions{ 65535U };
        std::size_t maximumParameters{ 65535U };
        std::size_t maximumStatements{ 1048576U };
        std::size_t maximumOperands{ 65535U };
        std::size_t maximumTypeDepth{ 128U };
        std::size_t maximumExpressionDepth{ 4096U };
        std::size_t maximumNumericBytes{ 4096U };
    };

    enum class ErrorKind : std::uint8_t
    {
        InvalidMagic,
        UnsupportedVersion,
        TruncatedInput,
        TrailingInput,
        InvalidTag,
        InvalidBoolean,
        InvalidScalar,
        InvalidCount,
        InvalidSymbol,
        InvalidInteger,
        UnsupportedType,
        LimitExceeded
    };

    struct Error final
    {
        ErrorKind kind{ ErrorKind::InvalidTag };
        std::size_t offset{};
        std::string context;
        std::string message;
    };

    struct EncodeResult final
    {
        std::vector<std::uint8_t> bytes;
        std::optional<Error> error;
        [[nodiscard]] explicit
        operator bool() const noexcept
        {
            return !error.has_value();
        }
    };

    struct DecodeResult final
    {
        std::optional<Module> module;
        std::optional<Error> error;
        [[nodiscard]] explicit
        operator bool() const noexcept
        {
            return module.has_value();
        }
    };

    [[nodiscard]] auto
    Encode(const Module &module, const Limits &limits = {}) -> EncodeResult;
    [[nodiscard]] auto
    Decode(std::span<const std::uint8_t> bytes, const Limits &limits = {}) -> DecodeResult;
} // namespace Visual::XSharp::Core::Wire
