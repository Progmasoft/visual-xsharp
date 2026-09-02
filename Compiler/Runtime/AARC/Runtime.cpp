// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

#include <algorithm>
#include <bit>
#include <limits>
#include <new>

#include "Visual/XSharp/Runtime/AARC.hpp"

namespace Visual::XSharp::Runtime::Aarc
{
    namespace
    {
        struct StringObject final
        {
            std::uint64_t count{};
            char32_t *scalars{};
        };

        void
        DestroyString(void *object) noexcept
        {
            auto *string = static_cast<StringObject *>(object);
            delete[] string->scalars;
            string->scalars = nullptr;
            string->count = 0U;
        }

        const TypeMetadata kStringMetadata{
            kAbiVersion,
            0U,
            sizeof(StringObject),
            alignof(StringObject),
            DestroyString,
            "System.String"
        };

        [[nodiscard]] auto
        ValidAlignment(std::size_t alignment) noexcept -> bool
        {
            return alignment != 0U && std::has_single_bit(alignment);
        }

        [[nodiscard]] auto
        RetainControl(ObjectHeader *header) noexcept -> bool
        {
            if (header == nullptr)
                return false;
            auto count = header->weakCount.load(std::memory_order_relaxed);
            while (count != 0U && count != std::numeric_limits<std::uint64_t>::max())
                if (header->weakCount.compare_exchange_weak(
                        count,
                        count + 1U,
                        std::memory_order_relaxed,
                        std::memory_order_relaxed))
                    return true;
            return false;
        }

        void
        ReleaseControl(ObjectHeader *header) noexcept
        {
            if (header == nullptr || header->weakCount.fetch_sub(1U, std::memory_order_acq_rel) != 1U)
                return;

            // The acquire side of the final decrement observes the destructor and all
            // preceding handle releases before reclaiming the combined allocation.
            auto *allocation = header->allocation;
            header->~ObjectHeader();
            ::operator delete(allocation);
        }

        [[nodiscard]] auto
        TryRetain(ObjectHeader *header) noexcept -> void *
        {
            if (header == nullptr)
                return nullptr;
            auto count = header->strongCount.load(std::memory_order_acquire);
            while (count != 0U)
            {
                if (count == std::numeric_limits<std::uint64_t>::max())
                    return nullptr;
                if (header->strongCount.compare_exchange_weak(
                        count,
                        count + 1U,
                        std::memory_order_acq_rel,
                        std::memory_order_acquire))
                    return header->object.load(std::memory_order_acquire);
            }
            return nullptr;
        }
    } // namespace

    auto
    Allocate(const TypeMetadata &metadata) noexcept -> void *
    {
        if (metadata.abiVersion != kAbiVersion || metadata.instanceSize == 0U || !ValidAlignment(metadata.instanceAlignment))
            return nullptr;

        const auto alignment = std::max(metadata.instanceAlignment, alignof(ObjectHeader *));
        constexpr auto kPrefix = sizeof(ObjectHeader) + sizeof(ObjectHeader *);
        if (metadata.instanceSize > std::numeric_limits<std::size_t>::max() - kPrefix - alignment)
            return nullptr;

        const auto allocationSize = kPrefix + alignment - 1U + metadata.instanceSize;
        auto *allocation = static_cast<std::byte *>(::operator new(allocationSize, std::nothrow));
        if (allocation == nullptr)
            return nullptr;

        auto *header = new (allocation) ObjectHeader{};
        header->metadata = &metadata;
        header->allocation = allocation;

        auto address = reinterpret_cast<std::uintptr_t>(allocation + kPrefix);
        address = (address + alignment - 1U) & ~(static_cast<std::uintptr_t>(alignment) - 1U);
        auto *object = reinterpret_cast<void *>(address);
        *(reinterpret_cast<ObjectHeader **>(object) - 1) = header;
        header->object.store(object, std::memory_order_release);
        return object;
    }

    auto
    Header(const void *object) noexcept -> ObjectHeader *
    {
        return object == nullptr ? nullptr : *(reinterpret_cast<ObjectHeader *const *>(object) - 1);
    }

    auto
    RetainStrong(void *object) noexcept -> void *
    {
        return TryRetain(Header(object));
    }

    void
    ReleaseStrong(void *object) noexcept
    {
        auto *header = Header(object);
        if (header == nullptr)
            return;
        if (header->strongCount.load(std::memory_order_relaxed) == std::numeric_limits<std::uint64_t>::max())
            return;
        if (header->strongCount.fetch_sub(1U, std::memory_order_acq_rel) != 1U)
            return;

        auto expected = ObjectState::Alive;
        if (header->state.compare_exchange_strong(expected, ObjectState::Destroying, std::memory_order_acq_rel))
        {
            if (header->metadata->destructor != nullptr)
                header->metadata->destructor(object);
            header->object.store(nullptr, std::memory_order_release);
            header->state.store(ObjectState::Destroyed, std::memory_order_release);
        }
        ReleaseControl(header); // Drop the implicit weak reference.
    }

    auto
    MakeWeak(void *object) noexcept -> Weak
    {
        auto *header = Header(object);
        return RetainControl(header) ? Weak{ header } : Weak{};
    }

    auto
    CopyWeak(Weak value) noexcept -> Weak
    {
        return RetainControl(value.header) ? value : Weak{};
    }

    auto
    LockWeak(Weak value) noexcept -> void *
    {
        return TryRetain(value.header);
    }

    void
    ReleaseWeak(Weak value) noexcept
    {
        ReleaseControl(value.header);
    }

    auto
    MakeUnowned(void *object) noexcept -> Unowned
    {
        auto *header = Header(object);
        return RetainControl(header) ? Unowned{ header } : Unowned{};
    }

    auto
    CopyUnowned(Unowned value) noexcept -> Unowned
    {
        return RetainControl(value.header) ? value : Unowned{};
    }

    auto
    LoadUnowned(Unowned value) noexcept -> void *
    {
        // Loading upgrades to a temporary strong result. Merely reading object after an
        // Alive check would race the last release on another thread. The generated owner
        // pass balances this result like Weak::Lock.
        return TryRetain(value.header);
    }

    void
    ReleaseUnowned(Unowned value) noexcept
    {
        ReleaseControl(value.header);
    }
} // namespace Visual::XSharp::Runtime::Aarc

extern "C"
{
    auto
    vxs_aarc_allocate(const Visual::XSharp::Runtime::Aarc::TypeMetadata *metadata) noexcept -> void *
    {
        return metadata == nullptr ? nullptr : Visual::XSharp::Runtime::Aarc::Allocate(*metadata);
    }

    auto
    vxs_aarc_retain_strong(void *object) noexcept -> void *
    {
        return Visual::XSharp::Runtime::Aarc::RetainStrong(object);
    }

    void
    vxs_aarc_release_strong(void *object) noexcept
    {
        Visual::XSharp::Runtime::Aarc::ReleaseStrong(object);
    }

    auto
    vxs_aarc_make_weak(void *object) noexcept -> void *
    {
        return Visual::XSharp::Runtime::Aarc::MakeWeak(object).header;
    }

    auto
    vxs_aarc_lock_weak(void *header) noexcept -> void *
    {
        return Visual::XSharp::Runtime::Aarc::LockWeak({ static_cast<Visual::XSharp::Runtime::Aarc::ObjectHeader *>(header) });
    }

    void
    vxs_aarc_release_weak(void *header) noexcept
    {
        Visual::XSharp::Runtime::Aarc::ReleaseWeak({ static_cast<Visual::XSharp::Runtime::Aarc::ObjectHeader *>(header) });
    }

    auto
    vxs_aarc_make_unowned(void *object) noexcept -> void *
    {
        return Visual::XSharp::Runtime::Aarc::MakeUnowned(object).header;
    }

    auto
    vxs_aarc_load_unowned(void *header) noexcept -> void *
    {
        return Visual::XSharp::Runtime::Aarc::LoadUnowned({ static_cast<Visual::XSharp::Runtime::Aarc::ObjectHeader *>(header) });
    }

    void
    vxs_aarc_release_unowned(void *header) noexcept
    {
        Visual::XSharp::Runtime::Aarc::ReleaseUnowned({ static_cast<Visual::XSharp::Runtime::Aarc::ObjectHeader *>(header) });
    }

    auto
    vxs_aarc_string_literal(const std::uint32_t *scalars, std::size_t count) noexcept -> void *
    {
        using namespace Visual::XSharp::Runtime::Aarc;
        if ((scalars == nullptr && count != 0U) || count == std::numeric_limits<std::size_t>::max())
            return nullptr;
        for (std::size_t index = 0; index < count; ++index)
            if (scalars[index] > 0x10ffffU || (scalars[index] >= 0xd800U && scalars[index] <= 0xdfffU))
                return nullptr;
        auto *object = static_cast<StringObject *>(Allocate(kStringMetadata));
        if (object == nullptr)
            return nullptr;
        object->scalars = new (std::nothrow) char32_t[count + 1U];
        if (object->scalars == nullptr)
        {
            ReleaseStrong(object);
            return nullptr;
        }
        object->count = count;
        for (std::size_t index = 0; index < count; ++index)
            object->scalars[index] = static_cast<char32_t>(scalars[index]);
        object->scalars[count] = U'\0';
        return object;
    }
}
