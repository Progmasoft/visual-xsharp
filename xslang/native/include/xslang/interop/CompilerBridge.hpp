// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

#ifndef XSLANG_INTEROP_COMPILER_BRIDGE_HPP
#define XSLANG_INTEROP_COMPILER_BRIDGE_HPP

#include <cstdint>

namespace xslang::interop
{
[[nodiscard]] std::uint32_t BridgeAbiVersion() noexcept;
[[nodiscard]] bool SupportsXlilVersion(std::uint32_t version) noexcept;
} // namespace xslang::interop

#endif
