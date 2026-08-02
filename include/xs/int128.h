/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef XS_INT128_H
#define XS_INT128_H

#include <stddef.h>
#include <stdint.h>

typedef struct
{
    uint64_t low;
    uint64_t high;
} XsUInt128;

typedef struct
{
    uint64_t low;
    int64_t high;
} XsInt128;

XsUInt128 xs_uint128_from_words(uint64_t high, uint64_t low);
XsUInt128 xs_uint128_from_u64(uint64_t value);
XsInt128 xs_int128_from_words(int64_t high, uint64_t low);
XsInt128 xs_int128_from_i64(int64_t value);
XsUInt128 xs_int128_as_bits(XsInt128 value);
int xs_uint128_compare(XsUInt128 left, XsUInt128 right);
bool xs_uint128_parse_hex(const char *text, size_t length, XsUInt128 *value);
void xs_uint128_format_hex(XsUInt128 value, char output[33]);

#endif
