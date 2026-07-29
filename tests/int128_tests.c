/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "xs/int128.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(condition)                                                                                               \
  do                                                                                                                   \
  {                                                                                                                    \
    if(!(condition))                                                                                                   \
    {                                                                                                                  \
      fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition);                                    \
      ++failures;                                                                                                      \
    }                                                                                                                  \
  } while(0)

int main(void)
{
  XsUInt128 maximum = {0};
  CHECK(xs_uint128_parse_hex("ffffffffffffffffffffffffffffffff", 32, &maximum));
  CHECK(maximum.high == UINT64_MAX);
  CHECK(maximum.low == UINT64_MAX);
  char text[33];
  xs_uint128_format_hex(maximum, text);
  CHECK(strcmp(text, "ffffffffffffffffffffffffffffffff") == 0);
  CHECK(xs_uint128_compare(xs_uint128_from_u64(7), xs_uint128_from_words(1, 0)) < 0);
  CHECK(!xs_uint128_parse_hex("g", 1, &maximum));
  CHECK(!xs_uint128_parse_hex("000000000000000000000000000000000", 33, &maximum));
  XsUInt128 negative_one = xs_int128_as_bits(xs_int128_from_i64(-1));
  CHECK(negative_one.high == UINT64_MAX);
  CHECK(negative_one.low == UINT64_MAX);
  return failures == 0 ? 0 : 1;
}
