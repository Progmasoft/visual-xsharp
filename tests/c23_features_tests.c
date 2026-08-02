/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "xs/c23_features.h"

typedef struct
{
    int value;
} Counter;

XS_C23_TRAIT(Readable, int (*read)(void *self););

static int counter_read(void *self)
{
    return ((Counter *)self)->value;
}

XS_C23_IMPL_FOR(Readable, Counter, .read = counter_read);

int main(void)
{
    Counter counter = {.value = 42};
    Readable readable = XS_C23_AS_TRAIT(Readable, Counter, &counter);
    return XS_C23_TRAIT_CALL(readable, read) == 42 ? 0 : 1;
}
