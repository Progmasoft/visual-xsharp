// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1
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
    /**
     * @brief Current VXCR schema version.
     *
     * Decoders require an exact match; they do not negotiate or reinterpret
     * artifacts from older revisions.
     */
    inline constexpr std::uint16_t kCurrentVersion = 5;

    /**
     * @brief Per-call resource ceilings for encoding and decoding.
     *
     * Decoding treats input bytes as untrusted and checks these bounds before
     * reserving variable-sized containers or descending into recursive values.
     */
    struct Limits final
    {
        /// Total document size.
        std::size_t maximumWireBytes{ 64U * 1024U * 1024U };
        /// Unicode scalars in one text field.
        std::size_t maximumTextScalars{ 1024U * 1024U };
        /// Functions in a module.
        std::size_t maximumFunctions{ 65535U };
        /// Parameters in one function or closure signature.
        std::size_t maximumParameters{ 65535U };
        /// Statements in one function, branch, or closure body list.
        std::size_t maximumStatements{ 1048576U };
        /// Values in one operand or template-argument list.
        std::size_t maximumOperands{ 65535U };
        /// Recursive type nesting.
        std::size_t maximumTypeDepth{ 128U };
        /// Recursive expression nesting.
        std::size_t maximumExpressionDepth{ 4096U };
        /// Magnitude bytes in one numeric literal.
        std::size_t maximumNumericBytes{ 4096U };
    };

    /** @brief Stable categories for malformed or unrepresentable wire values. */
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

    /**
     * @brief One encode/decode failure, with its byte offset and field context.
     *
     * The offset identifies the reader or writer position at which the
     * contract violation was detected; presentation belongs to the caller.
     */
    struct Error final
    {
        ErrorKind kind{ ErrorKind::InvalidTag };
        std::size_t offset{};
        std::string context;
        std::string message;
    };

    /** @brief Encoded bytes or the error that prevented serialization. */
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

    /** @brief A structurally decoded module or the error that rejected it. */
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

    /**
     * @brief Serialize a Core module using the current VXCR schema and limits.
     * @note Successful encoding does not replace semantic verification.
     */
    [[nodiscard]] auto
    Encode(const Module &module, const Limits &limits = {}) -> EncodeResult;

    /**
     * @brief Decode bounded VXCR bytes without granting them semantic trust.
     * @note Call the Core verifier before optimization, adaptation, or lowering.
     */
    [[nodiscard]] auto
    Decode(std::span<const std::uint8_t> bytes, const Limits &limits = {}) -> DecodeResult;
} // namespace Visual::XSharp::Core::Wire
