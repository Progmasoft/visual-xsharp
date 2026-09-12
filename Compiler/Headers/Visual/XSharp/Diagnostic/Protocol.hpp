// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "Visual/XSharp/Artifact/WireSupport.hpp"

namespace Visual::XSharp::Diagnostic
{
    inline constexpr std::uint16_t kProtocolVersion = 1U;

    enum class Stage : std::uint8_t
    {
        SourceLoader,
        Lexer,
        Parser,
        Renamer,
        NameResolution,
        TypeChecker,
        Desugarer,
        Core,
        CoreOptimizer,
        CorePrep,
        XppLowering,
        XppOptimizer,
        XmmLowering,
        XmmOptimizer,
        LlvmBackend
    };

    enum class Severity : std::uint8_t
    {
        Error,
        Warning,
        Information,
        Hint
    };

    struct Position final
    {
        std::uint32_t line{};
        std::uint32_t column{};

        [[nodiscard]] auto
        operator==(const Position &) const -> bool = default;
    };

    struct Range final
    {
        Position start{};
        Position end{};

        [[nodiscard]] auto
        operator==(const Range &) const -> bool = default;
    };

    struct Location final
    {
        // `source` is a compiler source identity, normally a project-relative or
        // absolute path. It is deliberately not forced into a URI by the wire layer.
        std::u32string source;
        Range range{};

        [[nodiscard]] auto
        operator==(const Location &) const -> bool = default;
    };

    struct MessageArgument final
    {
        std::u32string name;
        std::u32string value;

        [[nodiscard]] auto
        operator==(const MessageArgument &) const -> bool = default;
    };

    struct RelatedLocation final
    {
        Location location;
        std::u32string message;

        [[nodiscard]] auto
        operator==(const RelatedLocation &) const -> bool = default;
    };

    struct TextEdit final
    {
        Location location;
        std::u32string replacement;

        [[nodiscard]] auto
        operator==(const TextEdit &) const -> bool = default;
    };

    struct Fix final
    {
        std::u32string title;
        std::vector<TextEdit> edits;

        [[nodiscard]] auto
        operator==(const Fix &) const -> bool = default;
    };

    struct Record final
    {
        Stage stage{ Stage::Parser };
        Severity severity{ Severity::Error };
        std::u32string code;
        std::u32string message;
        std::vector<MessageArgument> arguments;
        std::optional<Location> primary;
        std::vector<RelatedLocation> related;
        std::vector<Fix> fixes;

        [[nodiscard]] auto
        operator==(const Record &) const -> bool = default;
    };

    struct Document final
    {
        std::vector<Record> records;

        [[nodiscard]] auto
        operator==(const Document &) const -> bool = default;
    };

    struct Limits final
    {
        std::size_t maximumWireBytes{ 16U * 1024U * 1024U };
        std::size_t maximumRecords{ 65535U };
        std::size_t maximumTextScalars{ 1024U * 1024U };
        std::size_t maximumArguments{ 256U };
        std::size_t maximumRelatedLocations{ 256U };
        std::size_t maximumFixes{ 128U };
        std::size_t maximumEditsPerFix{ 4096U };
    };

    using Error = Artifact::Wire::Error;
    using ErrorKind = Artifact::Wire::ErrorKind;

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
        std::optional<Document> document;
        std::optional<Error> error;

        [[nodiscard]] explicit
        operator bool() const noexcept
        {
            return document.has_value() && !error.has_value();
        }
    };

    // The protocol is an IDE/compiler side channel. Human stderr wording may
    // evolve independently because editor integrations consume this document.
    [[nodiscard]] auto
    Encode(const Document &document, const Limits &limits = {}) -> EncodeResult;

    [[nodiscard]] auto
    Decode(const std::vector<std::uint8_t> &bytes, const Limits &limits = {}) -> DecodeResult;
} // namespace Visual::XSharp::Diagnostic
