// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Visual/XSharp/Core/CorePrep.hpp"

namespace Visual::XSharp::Artifact::Wire
{
    struct Limits final
    {
        std::size_t maximumWireBytes{ 64U * 1024U * 1024U };
        std::size_t maximumTextScalars{ 1024U * 1024U };
        std::size_t maximumFunctions{ 65535U };
        std::size_t maximumParameters{ 65535U };
        std::size_t maximumBlocks{ 1048576U };
        std::size_t maximumInstructions{ 1048576U };
        std::size_t maximumOperands{ 65535U };
        std::size_t maximumTypeDepth{ 128U };
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
        InvalidSymbol,
        InvalidInteger,
        LimitExceeded,
        InvalidModel
    };

    struct Error final
    {
        ErrorKind kind{ ErrorKind::InvalidModel };
        std::size_t offset{};
        std::string context;
        std::string message;

        [[nodiscard]] auto
        operator==(const Error &) const -> bool = default;
    };

    // Writer and Reader own the shared scalar grammar used by Xpp and Xmm.
    // Stage codecs still own their instruction/control-flow schemas, avoiding a
    // generic serializer that could silently erase stage-specific invariants.
    class Writer final
    {
    public:
        explicit Writer(const Limits &limits);

        void
        Byte(std::uint8_t value);
        void
        U16(std::uint16_t value);
        void
        U32(std::uint32_t value);
        void
        U64(std::uint64_t value);
        void
        Boolean(bool value);
        void
        Count(std::size_t value, std::size_t maximum, std::string_view context);
        void
        Text(std::u32string_view value, std::string_view context);
        void
        QualifiedName(const std::vector<std::u32string> &value, std::string_view context);
        void
        Symbol(const ::visual_xsharp::core::SymbolName &value, std::string_view context);
        void
        Type(const ::visual_xsharp::core::Type &value, std::string_view context, std::size_t depth = 0U);
        void
        Literal(const ::visual_xsharp::core::Literal &value, const ::visual_xsharp::core::Type &type, std::string_view context);
        void
        Fail(ErrorKind kind, std::string context, std::string message);

        [[nodiscard]] auto
        Bytes() const -> const std::vector<std::uint8_t> &;
        [[nodiscard]] auto
        TakeBytes() -> std::vector<std::uint8_t>;
        [[nodiscard]] auto
        Failure() const -> const std::optional<Error> &;
        [[nodiscard]] auto
        Offset() const noexcept -> std::size_t;

    private:
        const Limits &limits_;
        std::vector<std::uint8_t> bytes_;
        std::optional<Error> error_;
    };

    class Reader final
    {
    public:
        Reader(std::span<const std::uint8_t> bytes, const Limits &limits);

        [[nodiscard]] auto
        Byte(std::string_view context) -> std::uint8_t;
        [[nodiscard]] auto
        U16(std::string_view context) -> std::uint16_t;
        [[nodiscard]] auto
        U32(std::string_view context) -> std::uint32_t;
        [[nodiscard]] auto
        U64(std::string_view context) -> std::uint64_t;
        [[nodiscard]] auto
        Boolean(std::string_view context) -> bool;
        [[nodiscard]] auto
        Count(std::size_t maximum, std::string_view context) -> std::size_t;
        [[nodiscard]] auto
        Text(std::string_view context) -> std::u32string;
        [[nodiscard]] auto
        QualifiedName(std::string_view context) -> std::vector<std::u32string>;
        [[nodiscard]] auto
        Symbol(std::string_view context) -> ::visual_xsharp::core::SymbolName;
        [[nodiscard]] auto
        Type(std::string_view context, std::size_t depth = 0U) -> ::visual_xsharp::core::Type;
        [[nodiscard]] auto
        Literal(const ::visual_xsharp::core::Type &type, std::string_view context)
            -> ::visual_xsharp::core::Literal;
        void
        Fail(ErrorKind kind, std::string context, std::string message);

        [[nodiscard]] auto
        Failure() const -> const std::optional<Error> &;
        [[nodiscard]] auto
        Offset() const noexcept -> std::size_t;
        [[nodiscard]] auto
        AtEnd() const noexcept -> bool;
        [[nodiscard]] auto
        InputSize() const noexcept -> std::size_t;

    private:
        std::span<const std::uint8_t> bytes_;
        const Limits &limits_;
        std::size_t offset_{};
        std::optional<Error> error_;
    };

    [[nodiscard]] auto
    IsUnicodeScalar(char32_t value) noexcept -> bool;
} // namespace Visual::XSharp::Artifact::Wire
