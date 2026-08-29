// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

#include "xslang/interop/CompilerBridge.hpp"

namespace xslang::interop
{
std::uint32_t BridgeAbiVersion() noexcept
{
    return 1U;
}

bool SupportsXlilVersion(const std::uint32_t version) noexcept
{
    return version <= 1U;
}
} // namespace xslang::interop
