// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "Visual/XSharp/Core.hpp"

#include <vector>

namespace visual_xsharp::core
{

struct CorePrepBinding final
{
    SymbolId symbol{};
    std::string type;
    CoreLiteral atom;
};

struct CorePrepModule final
{
    std::string name;
    std::vector<CorePrepBinding> bindings;
};

// CorePrep makes Core bindings atomic before control-flow lowering.
[[nodiscard]] auto prepare(const CoreModule &module) -> CorePrepModule;

} // namespace visual_xsharp::core
