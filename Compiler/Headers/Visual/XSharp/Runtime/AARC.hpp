// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "Visual/XSharp/Runtime/AARC.h"

namespace Visual::XSharp::Runtime::Aarc
{
    inline constexpr std::uint32_t kAbiVersion = VXS_AARC_ABI_VERSION;

    /** Hash a canonical, case-sensitive runtime type name into its stable
     * identity. */
    [[nodiscard]] consteval auto
    TypeIdentity(std::string_view canonicalName) noexcept -> std::uint64_t
    {
        std::uint64_t value = 14695981039346656037ULL;
        for (const auto byte : canonicalName)
            value
                = (value ^ static_cast<unsigned char>(byte)) * 1099511628211ULL;
        return value;
    }

    using Destructor = VxsAarcDestructor;
    using TypeMetadata = VxsAarcTypeMetadata;

    // The control header stays incomplete at the API boundary: C++ callers can
    // own handles without depending on atomic layout or runtime-private
    // offsets.
    struct ObjectHeader;

    struct Weak final
    {
        // Copying this value bitwise does not retain the control block; use
        // CopyWeak.
        ObjectHeader *header{};
    };

    struct Unowned final
    {
        // Copying this value bitwise does not retain the control block; use
        // CopyUnowned.
        ObjectHeader *header{};
    };

    /** Allocate aligned payload storage and return its initial strong owner. */
    [[nodiscard]] auto
    Allocate(const TypeMetadata &metadata) noexcept -> void *;

    /** Retain the live object, returning null if its strong lifetime has ended.
     */
    auto
    RetainStrong(void *object) noexcept -> void *;

    /** Release one strong owner and run the payload destructor exactly once at
     * zero. */
    void
    ReleaseStrong(void *object) noexcept;

    /** Create one weak control reference without retaining the payload. */
    [[nodiscard]] auto
    MakeWeak(void *object) noexcept -> Weak;

    /** Retain the control reference represented by an existing weak value. */
    [[nodiscard]] auto
    CopyWeak(Weak value) noexcept -> Weak;

    /** Upgrade a live weak target to one strong owner, or return null after
     * destruction. */
    [[nodiscard]] auto
    LockWeak(Weak value) noexcept -> void *;

    /** Release one weak control reference. */
    void
    ReleaseWeak(Weak value) noexcept;

    /** Create one unowned control reference without retaining the payload. */
    [[nodiscard]] auto
    MakeUnowned(void *object) noexcept -> Unowned;

    /** Retain the control reference represented by an existing unowned value.
     */
    [[nodiscard]] auto
    CopyUnowned(Unowned value) noexcept -> Unowned;

    /** Load an unowned target as a temporary strong owner, or return null when
     * dead. */
    [[nodiscard]] auto
    LoadUnowned(Unowned value) noexcept -> void *;

    /** Release one unowned control reference. */
    void
    ReleaseUnowned(Unowned value) noexcept;

    /** Check exact runtime identity for a live payload; null never matches. */
    [[nodiscard]] auto
    IsExactType(const void *object, std::uint64_t typeIdentity) noexcept
        -> bool;
} // namespace Visual::XSharp::Runtime::Aarc
