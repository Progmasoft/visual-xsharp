// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace visual_xsharp::core::wire
{
inline constexpr std::uint8_t magic[] = {'V', 'X', 'C', 'P'};
inline constexpr std::uint16_t current_version = 1;

struct Limits final
{
    std::size_t maximum_wire_bytes{64U * 1024U * 1024U};
    std::size_t maximum_string_code_points{1024U * 1024U};
    std::size_t maximum_functions{65535U};
    std::size_t maximum_parameters_per_function{65535U};
    std::size_t maximum_blocks_per_function{1048576U};
    std::size_t maximum_instructions_per_block{1048576U};
    std::size_t maximum_operands_per_instruction{65535U};
    std::size_t maximum_type_depth{128U};
};

enum class ErrorKind : std::uint8_t
{
    InvalidMagic, UnsupportedVersion, TruncatedInput, TrailingInput,
    InvalidTag, InvalidBoolean, InvalidCodePoint, InvalidCount,
    InvalidSymbol, InvalidInteger, UnsupportedType, LimitExceeded
};

struct Error final
{
    ErrorKind kind{ErrorKind::InvalidTag};
    std::size_t offset{};
    std::string context;
    std::string message;
};
} // namespace visual_xsharp::core::wire
