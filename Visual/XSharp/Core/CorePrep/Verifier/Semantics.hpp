// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "Visual/XSharp/Core/CorePrep/Verifier.hpp"

namespace visual_xsharp::core
{
[[nodiscard]] auto verify_semantics(const CorePrepModule &module) -> std::vector<VerificationIssue>;
}
