// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

#include "xslang/interop/CompilerBridge.hxx"

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
