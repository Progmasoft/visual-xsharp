// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1
#pragma once

#include <cstdint>

namespace Visual::XSharp::Core::Wire
{
    /// The four bytes every Core wire document starts with. The reader and
    /// the writer are separate translation units and share this one
    /// definition.
    inline constexpr std::uint8_t kMagic[] = { 'V', 'X', 'C', 'R' };
} // namespace Visual::XSharp::Core::Wire
