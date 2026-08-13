// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "Visual/XSharp/Core/CorePrep.hpp"
#include "Visual/XSharp/Core/IR.hpp"

namespace Visual::XSharp::Core::CorePrep
{
// Prepare is the adapting layer between verified, nested Core and the flat typed
// CorePrep CFG consumed by Xpp. It deliberately performs no optimization and invents
// no types; it only atomizes expressions and makes source control flow explicit.
[[nodiscard]] auto Prepare(const Module &module) -> ::visual_xsharp::core::CorePrepModule;
} // namespace Visual::XSharp::Core::CorePrep
