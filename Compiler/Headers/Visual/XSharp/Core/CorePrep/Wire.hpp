// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
#pragma once

#include "Visual/XSharp/Core/CorePrep.hpp"
#include "Visual/XSharp/Core/CorePrep/Wire/Format.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace visual_xsharp::core::wire
{
struct EncodeResult final
{
    std::vector<std::uint8_t> bytes;
    std::optional<Error> error;
    [[nodiscard]] explicit operator bool() const noexcept { return !error.has_value(); }
};

struct DecodeResult final
{
    std::optional<CorePrepModule> module;
    std::optional<Error> error;
    [[nodiscard]] explicit operator bool() const noexcept { return module.has_value(); }
};

[[nodiscard]] auto encode(const CorePrepModule &module, const Limits &limits = {}) -> EncodeResult;
[[nodiscard]] auto decode(std::span<const std::uint8_t> bytes, const Limits &limits = {}) -> DecodeResult;
} // namespace visual_xsharp::core::wire
