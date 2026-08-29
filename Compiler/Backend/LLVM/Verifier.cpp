// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

#include "Visual/XSharp/Backend/LLVM.hpp"

namespace Visual::XSharp::Backend::LLVM
{
auto Verify(const Xmm::Module &module) -> std::vector<Issue>
{
    // Preserve the backend API while delegating ownership to the Xmm stage. Lower
    // calls this adapter as a defense-in-depth check for direct embedding clients.
    return ::Visual::XSharp::Xmm::Verify(module);
}
} // namespace Visual::XSharp::Backend::LLVM
