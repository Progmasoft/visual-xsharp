/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "xs/int128.h"

XsUInt128 xs_uint128_from_words(uint64_t high, uint64_t low)
{
  return (XsUInt128){.low = low, .high = high};
}

XsUInt128 xs_uint128_from_u64(uint64_t value)
{
  return xs_uint128_from_words(0, value);
}

XsInt128 xs_int128_from_words(int64_t high, uint64_t low)
{
  return (XsInt128){.low = low, .high = high};
}

XsInt128 xs_int128_from_i64(int64_t value)
{
  return xs_int128_from_words(value < 0 ? -1 : 0, (uint64_t)value);
}

XsUInt128 xs_int128_as_bits(XsInt128 value)
{
  return xs_uint128_from_words((uint64_t)value.high, value.low);
}

int xs_uint128_compare(XsUInt128 left, XsUInt128 right)
{
  if(left.high != right.high)
    return left.high < right.high ? -1 : 1;
  if(left.low != right.low)
    return left.low < right.low ? -1 : 1;
  return 0;
}

static int hexadecimal_digit(char character)
{
  if(character >= '0' && character <= '9')
    return character - '0';
  if(character >= 'a' && character <= 'f')
    return character - 'a' + 10;
  if(character >= 'A' && character <= 'F')
    return character - 'A' + 10;
  return -1;
}

bool xs_uint128_parse_hex(const char *text, size_t length, XsUInt128 *value)
{
  if(text == nullptr || value == nullptr || length == 0 || length > 32)
    return false;
  XsUInt128 parsed = {0};
  for(size_t index = 0; index < length; ++index)
  {
    int digit = hexadecimal_digit(text[index]);
    if(digit < 0)
      return false;
    parsed.high = (parsed.high << 4U) | (parsed.low >> 60U);
    parsed.low = (parsed.low << 4U) | (uint64_t)digit;
  }
  *value = parsed;
  return true;
}

void xs_uint128_format_hex(XsUInt128 value, char output[33])
{
  static const char digits[] = "0123456789abcdef";
  for(size_t index = 0; index < 16; ++index)
  {
    unsigned shift = (unsigned)(15U - index) * 4U;
    output[index] = digits[(value.high >> shift) & UINT64_C(0xf)];
    output[index + 16U] = digits[(value.low >> shift) & UINT64_C(0xf)];
  }
  output[32] = '\0';
}
