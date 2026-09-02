// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "Visual/XSharp/Artifact/WireSupport.hpp"
#include "Visual/XSharp/Xmm/IR.hpp"

namespace Visual::XSharp::Xmm::Wire
{
    inline constexpr std::uint16_t kCurrentVersion = 2U;
    using Limits = Artifact::Wire::Limits;
    using Error = Artifact::Wire::Error;
    using ErrorKind = Artifact::Wire::ErrorKind;

    struct EncodeResult final
    {
        std::vector<std::uint8_t> bytes;
        std::optional<Error> error;
        [[nodiscard]] explicit
        operator bool() const noexcept
        {
            return !error;
        }
    };

    struct DecodeResult final
    {
        std::optional<::visual_xsharp::xmm::Module> module;
        std::optional<Error> error;
        [[nodiscard]] explicit
        operator bool() const noexcept
        {
            return module.has_value();
        }
    };

    [[nodiscard]] auto
    Encode(const ::visual_xsharp::xmm::Module &module, const Limits &limits = {}) -> EncodeResult;
    [[nodiscard]] auto
    Decode(std::span<const std::uint8_t> bytes, const Limits &limits = {}) -> DecodeResult;
} // namespace Visual::XSharp::Xmm::Wire
