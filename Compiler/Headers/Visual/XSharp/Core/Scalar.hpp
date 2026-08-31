// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "Visual/XSharp/Core/CorePrep.hpp"

namespace visual_xsharp::core
{
    enum class ScalarFamily : std::uint8_t
    {
        None,
        Boolean,
        Character,
        SignedInteger,
        UnsignedInteger,
        Floating
    };

    struct ScalarDescription final
    {
        ScalarFamily family{ ScalarFamily::None };
        std::uint16_t bit_width{};
        std::string_view spelling;

        [[nodiscard]] auto
        is_numeric() const noexcept -> bool;
        [[nodiscard]] auto
        is_integer() const noexcept -> bool;
        [[nodiscard]] auto
        is_signed() const noexcept -> bool;
        [[nodiscard]] auto
        is_floating() const noexcept -> bool;
    };

    // Returns no description for aggregate, callable, named, and type-variable shapes.
    // The function is the one canonical mapping from Core type tags to language scalars.
    [[nodiscard]] auto
    describe_scalar(const Type &type) noexcept -> std::optional<ScalarDescription>;
    [[nodiscard]] auto
    is_numeric(const Type &type) noexcept -> bool;
    [[nodiscard]] auto
    is_integer(const Type &type) noexcept -> bool;
    [[nodiscard]] auto
    is_signed_integer(const Type &type) noexcept -> bool;
    [[nodiscard]] auto
    is_unsigned_integer(const Type &type) noexcept -> bool;
    [[nodiscard]] auto
    is_floating(const Type &type) noexcept -> bool;
    [[nodiscard]] auto
    accepts_boolean_context(const Type &type) noexcept -> bool;

    // Canonical integer helpers are shared by codecs, verifiers, and LLVM lowering. Keeping
    // normalization here prevents each stage from inventing subtly different zero/sign rules.
    [[nodiscard]] auto
    normalize_integer(IntegerLiteral value) -> IntegerLiteral;
    [[nodiscard]] auto
    integer_is_canonical(const IntegerLiteral &value) noexcept -> bool;
    [[nodiscard]] auto
    integer_is_zero(const IntegerLiteral &value) noexcept -> bool;
    [[nodiscard]] auto
    integer_fits(const IntegerLiteral &value, const Type &type) noexcept -> bool;
    [[nodiscard]] auto
    integer_from_signed(std::int64_t value) -> IntegerLiteral;
    [[nodiscard]] auto
    integer_from_unsigned(std::uint64_t value) -> IntegerLiteral;
    [[nodiscard]] auto
    integer_hex_magnitude(const IntegerLiteral &value) -> std::string;

    // Floating spelling is intentionally locale-independent. Accepted forms are decimal
    // significands with an optional exponent plus inf/nan spellings used by LLVM APFloat.
    [[nodiscard]] auto
    floating_spelling_is_valid(std::string_view spelling) noexcept -> bool;

    // Validates both the variant alternative and scalar range. A diagnostic-ready reason is
    // returned so artifact readers can reject malformed payloads before any backend sees them.
    [[nodiscard]] auto
    validate_literal(const Literal &literal, const Type &type) -> std::optional<std::string>;
} // namespace visual_xsharp::core
