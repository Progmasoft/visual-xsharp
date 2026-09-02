// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace Visual::XSharp::Runtime::Aarc
{
    inline constexpr std::uint32_t kAbiVersion = 1U;

    enum class ObjectState : std::uint32_t
    {
        Alive = 0U,
        Destroying = 1U,
        Destroyed = 2U
    };

    using Destructor = void (*)(void *) noexcept;

    struct TypeMetadata final
    {
        // Metadata is immutable and must outlive every object allocated with it.
        std::uint32_t abiVersion{ kAbiVersion };
        std::uint32_t flags{};
        std::size_t instanceSize{};
        std::size_t instanceAlignment{ alignof(std::max_align_t) };
        Destructor destructor{};
        const char *typeName{};
    };

    // The control header outlives the payload destructor while weak or unowned
    // handles exist. weakCount includes one implicit reference while strongCount
    // is non-zero, exactly preventing the header from disappearing during teardown.
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

    struct Weak final
    {
        ObjectHeader *header{};
    };

    struct Unowned final
    {
        ObjectHeader *header{};
    };

    [[nodiscard]] auto
    Allocate(const TypeMetadata &metadata) noexcept -> void *;
    [[nodiscard]] auto
    Header(const void *object) noexcept -> ObjectHeader *;
    auto
    RetainStrong(void *object) noexcept -> void *;
    void
    ReleaseStrong(void *object) noexcept;
    [[nodiscard]] auto
    MakeWeak(void *object) noexcept -> Weak;
    [[nodiscard]] auto
    CopyWeak(Weak value) noexcept -> Weak;
    [[nodiscard]] auto
    LockWeak(Weak value) noexcept -> void *;
    void
    ReleaseWeak(Weak value) noexcept;
    [[nodiscard]] auto
    MakeUnowned(void *object) noexcept -> Unowned;
    [[nodiscard]] auto
    CopyUnowned(Unowned value) noexcept -> Unowned;
    [[nodiscard]] auto
    LoadUnowned(Unowned value) noexcept -> void *;
    void
    ReleaseUnowned(Unowned value) noexcept;
} // namespace Visual::XSharp::Runtime::Aarc

// Xmm lowering targets a stable C ABI. The C++ API above remains convenient for
// the runtime and native tests without exposing C++ name mangling to LLVM IR.
extern "C"
{
    [[nodiscard]] auto
    vxs_aarc_allocate(const Visual::XSharp::Runtime::Aarc::TypeMetadata *metadata) noexcept -> void *;
    [[nodiscard]] auto
    vxs_aarc_retain_strong(void *object) noexcept -> void *;
    void
    vxs_aarc_release_strong(void *object) noexcept;
    [[nodiscard]] auto
    vxs_aarc_make_weak(void *object) noexcept -> void *;
    [[nodiscard]] auto
    vxs_aarc_lock_weak(void *header) noexcept -> void *;
    void
    vxs_aarc_release_weak(void *header) noexcept;
    [[nodiscard]] auto
    vxs_aarc_make_unowned(void *object) noexcept -> void *;
    [[nodiscard]] auto
    vxs_aarc_load_unowned(void *header) noexcept -> void *;
    void
    vxs_aarc_release_unowned(void *header) noexcept;
    [[nodiscard]] auto
    vxs_aarc_string_literal(const std::uint32_t *scalars, std::size_t count) noexcept -> void *;
}
