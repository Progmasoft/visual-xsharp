// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

#include <stddef.h>
#include <stdint.h>

#include "Visual/XSharp/Runtime/AARC.h"

_Static_assert(sizeof(((VxsAarcTypeMetadata *)0)->abiVersion) == sizeof(uint32_t), "ABI version must remain 32-bit");
_Static_assert(sizeof(((VxsAarcTypeMetadata *)0)->typeIdentity) == sizeof(uint64_t), "type identity must remain 64-bit");

typedef struct TestPayload
{
    uint64_t marker;
} TestPayload;

static uint32_t gDestructionCount;

static void
DestroyPayload(void *object)
{
    TestPayload *payload = (TestPayload *)object;
    payload->marker = 0U;
    ++gDestructionCount;
}

static const VxsAarcTypeMetadata kPayloadMetadata = {
    VXS_AARC_ABI_VERSION,
    0U,
    UINT64_C(0x51A7E001),
    sizeof(TestPayload),
    _Alignof(TestPayload),
    DestroyPayload,
    "C11.TestPayload",
};

static uint64_t
StringTypeIdentity(void)
{
    static const char kName[] = "String";
    uint64_t identity = UINT64_C(14695981039346656037);
    size_t index;
    for (index = 0U; index < sizeof(kName) - 1U; ++index)
        identity = (identity ^ (uint8_t)kName[index]) * UINT64_C(1099511628211);
    return identity;
}

static int
CheckOpaqueStrongWeakAndUnownedHandles(void)
{
    TestPayload *payload;
    VxsAarcWeakHandle *weak;
    VxsAarcWeakHandle *weakCopy;
    VxsAarcUnownedHandle *unowned;
    VxsAarcUnownedHandle *unownedCopy;
    void *loaded;

    gDestructionCount = 0U;
    payload = (TestPayload *)vxs_aarc_allocate(&kPayloadMetadata);
    if (payload == NULL)
        return 1;
    payload->marker = UINT64_C(0xAACC55);
    if (vxs_aarc_retain_strong(payload) != payload)
        return 2;
    vxs_aarc_release_strong(payload);
    if (gDestructionCount != 0U)
        return 3;

    weak = vxs_aarc_make_weak(payload);
    weakCopy = vxs_aarc_copy_weak(weak);
    unowned = vxs_aarc_make_unowned(payload);
    unownedCopy = vxs_aarc_copy_unowned(unowned);
    if (weak == NULL || weakCopy == NULL || unowned == NULL || unownedCopy == NULL)
        return 4;
    loaded = vxs_aarc_lock_weak(weak);
    if (loaded != payload)
        return 5;
    vxs_aarc_release_strong(loaded);
    loaded = vxs_aarc_load_unowned(unowned);
    if (loaded != payload)
        return 6;

    vxs_aarc_release_strong(payload);
    if (gDestructionCount != 0U)
        return 7;
    vxs_aarc_release_strong(loaded);
    if (gDestructionCount != 1U)
        return 8;
    if (vxs_aarc_lock_weak(weak) != NULL || vxs_aarc_load_unowned(unowned) != NULL)
        return 9;
    vxs_aarc_release_weak(weakCopy);
    vxs_aarc_release_weak(weak);
    vxs_aarc_release_unowned(unownedCopy);
    vxs_aarc_release_unowned(unowned);
    return 0;
}

static int
CheckStringFactoryRejectsInvalidScalars(void)
{
    static const uint32_t kValidScalars[] = { UINT32_C(0x56), UINT32_C(0x1F642) };
    static const uint32_t kSurrogate[] = { UINT32_C(0xD800) };
    static const uint32_t kOutOfRange[] = { UINT32_C(0x110000) };
    void *empty = vxs_aarc_string_literal(NULL, 0U);
    void *text = vxs_aarc_string_literal(kValidScalars, sizeof(kValidScalars) / sizeof(kValidScalars[0]));

    if (empty == NULL || text == NULL)
        return 10;
    if (!vxs_aarc_is_exact_type(text, StringTypeIdentity()))
        return 11;
    vxs_aarc_release_strong(text);
    vxs_aarc_release_strong(empty);
    if (vxs_aarc_string_literal(NULL, 1U) != NULL)
        return 12;
    if (vxs_aarc_string_literal(kSurrogate, 1U) != NULL)
        return 13;
    if (vxs_aarc_string_literal(kOutOfRange, 1U) != NULL)
        return 14;
    return 0;
}

static int
CheckMetadataValidation(void)
{
    VxsAarcTypeMetadata metadata = kPayloadMetadata;
    void *payload;

    metadata.abiVersion += 1U;
    if (vxs_aarc_allocate(&metadata) != NULL)
        return 20;
    metadata = kPayloadMetadata;
    metadata.instanceSize = 0U;
    if (vxs_aarc_allocate(&metadata) != NULL)
        return 21;
    metadata = kPayloadMetadata;
    metadata.instanceAlignment = 3U;
    if (vxs_aarc_allocate(&metadata) != NULL)
        return 22;
    metadata = kPayloadMetadata;
    metadata.instanceAlignment = 64U;
    payload = vxs_aarc_allocate(&metadata);
    if (payload == NULL || (uintptr_t)payload % 64U != 0U)
        return 23;
    vxs_aarc_release_strong(payload);
    return 0;
}

int
main(void)
{
    int result;
    if (vxs_aarc_abi_version() != VXS_AARC_ABI_VERSION)
        return 100;
    if (vxs_aarc_allocate(NULL) != NULL || vxs_aarc_retain_strong(NULL) != NULL)
        return 101;
    result = CheckOpaqueStrongWeakAndUnownedHandles();
    if (result != 0)
        return result;
    result = CheckMetadataValidation();
    if (result != 0)
        return result;
    return CheckStringFactoryRejectsInvalidScalars();
}
