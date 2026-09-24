// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1
#ifndef VISUAL_XSHARP_RUNTIME_AARC_H
#define VISUAL_XSHARP_RUNTIME_AARC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define VXS_AARC_ABI_VERSION UINT32_C(2)

#ifdef __cplusplus
#    define VXS_AARC_NOEXCEPT noexcept
extern "C"
{
#else
#    define VXS_AARC_NOEXCEPT
#endif

    /** Destructor callbacks must not throw and must release only payload-owned resources. */
    typedef void (*VxsAarcDestructor)(void *object);

    /** Opaque control-block handles; callers must never inspect or free them directly. */
    typedef struct VxsAarcWeakHandle VxsAarcWeakHandle;
    typedef struct VxsAarcUnownedHandle VxsAarcUnownedHandle;

    /** Stable metadata layout shared by C11 callers and the C++20 runtime implementation. */
    typedef struct VxsAarcTypeMetadata
    {
        uint32_t abiVersion;
        uint32_t flags;
        uint64_t typeIdentity;
        size_t instanceSize;
        size_t instanceAlignment;
        VxsAarcDestructor destructor;
        const char *typeName;
    } VxsAarcTypeMetadata;

    /** Return the ABI version implemented by this runtime binary. */
    uint32_t
    vxs_aarc_abi_version(void) VXS_AARC_NOEXCEPT;

    /** Allocate a payload with one strong owner; metadata must outlive every allocated value. */
    void *
    vxs_aarc_allocate(const VxsAarcTypeMetadata *metadata) VXS_AARC_NOEXCEPT;

    /** Retain a live object, returning null if the object is no longer retainable. */
    void *
    vxs_aarc_retain_strong(void *object) VXS_AARC_NOEXCEPT;

    /** Release one strong owner and destroy the payload after the final release. */
    void
    vxs_aarc_release_strong(void *object) VXS_AARC_NOEXCEPT;

    /** Create or copy an opaque weak handle; release each successful handle exactly once. */
    VxsAarcWeakHandle *
    vxs_aarc_make_weak(void *object) VXS_AARC_NOEXCEPT;
    VxsAarcWeakHandle *
    vxs_aarc_copy_weak(VxsAarcWeakHandle *weakHandle) VXS_AARC_NOEXCEPT;
    void *
    vxs_aarc_lock_weak(VxsAarcWeakHandle *weakHandle) VXS_AARC_NOEXCEPT;
    void
    vxs_aarc_release_weak(VxsAarcWeakHandle *weakHandle) VXS_AARC_NOEXCEPT;

    /** Create or copy an opaque unowned handle; load upgrades it to a temporary strong owner. */
    VxsAarcUnownedHandle *
    vxs_aarc_make_unowned(void *object) VXS_AARC_NOEXCEPT;
    VxsAarcUnownedHandle *
    vxs_aarc_copy_unowned(VxsAarcUnownedHandle *unownedHandle) VXS_AARC_NOEXCEPT;
    void *
    vxs_aarc_load_unowned(VxsAarcUnownedHandle *unownedHandle) VXS_AARC_NOEXCEPT;
    void
    vxs_aarc_release_unowned(VxsAarcUnownedHandle *unownedHandle) VXS_AARC_NOEXCEPT;

    /** Create an immutable UTF-32 scalar string; reject null input, surrogates, and out-of-range values. */
    void *
    vxs_aarc_string_literal(const uint32_t *scalars, size_t count) VXS_AARC_NOEXCEPT;

    /** Compare the exact registered runtime identity without accepting null/destroyed objects. */
    bool
    vxs_aarc_is_exact_type(const void *object, uint64_t typeIdentity) VXS_AARC_NOEXCEPT;

#ifdef __cplusplus
}
#endif

#undef VXS_AARC_NOEXCEPT
#endif
