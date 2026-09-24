// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1
#pragma once

#include <atomic>
#include <cstdint>

#include "Visual/XSharp/Runtime/AARC.hpp"

namespace Visual::XSharp::Runtime::Aarc
{
    enum class ObjectState : std::uint32_t
    {
        Alive = 0U,
        Destroying = 1U,
        Destroyed = 2U
    };

    // The allocation/control-block layout is implementation-private. Only the
    // C++ runtime owns these atomics; C callers receive opaque weak/unowned
    // handles.
    struct ObjectHeader final
    {
        std::uint32_t abiVersion{ kAbiVersion };
        std::atomic<ObjectState> state{ ObjectState::Alive };
        std::atomic<std::uint64_t> strongCount{ 1U };
        std::atomic<std::uint64_t> weakCount{ 1U };
        const TypeMetadata *metadata{};
        std::atomic<void *> object{};
        void *allocation{};
    };
} // namespace Visual::XSharp::Runtime::Aarc
