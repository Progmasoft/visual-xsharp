// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace visual_xsharp
{

using SymbolId = std::uint64_t;
using CoreLiteral = std::variant<std::int64_t, double, bool, std::string>;

struct CoreBinding final
{
    SymbolId symbol{};
    std::string type;
    CoreLiteral value;
};

struct CoreModule final
{
    std::string name;
    std::vector<CoreBinding> bindings;
};

} // namespace visual_xsharp
