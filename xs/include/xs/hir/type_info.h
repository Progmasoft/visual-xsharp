/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef XS_HIR_TYPE_INFO_H
#define XS_HIR_TYPE_INFO_H

#include "xs/lil.h"

#include <stddef.h>

typedef enum
{
  XS_HIR_PRIMITIVE_STR,
  XS_HIR_PRIMITIVE_BOOL,
  XS_HIR_PRIMITIVE_BYTE,
  XS_HIR_PRIMITIVE_SBYTE,
  XS_HIR_PRIMITIVE_CHAR,
  XS_HIR_PRIMITIVE_SHORT,
  XS_HIR_PRIMITIVE_LONG,
  XS_HIR_PRIMITIVE_INT,
  XS_HIR_PRIMITIVE_INTEGER,
  XS_HIR_PRIMITIVE_USHORT,
  XS_HIR_PRIMITIVE_ULONG,
  XS_HIR_PRIMITIVE_UINT,
  XS_HIR_PRIMITIVE_UINTEGER,
  XS_HIR_PRIMITIVE_SFLOAT,
  XS_HIR_PRIMITIVE_LFLOAT,
  XS_HIR_PRIMITIVE_FLOAT,
  XS_HIR_PRIMITIVE_DOUBLE,
  XS_HIR_PRIMITIVE_STRING,
} XsHirPrimitiveKind;

typedef struct
{
  const char *name;
  size_t bit_width;
  size_t code_unit_bit_width;
  XsHirPrimitiveKind kind;
  bool is_signed;
  bool is_integer;
  bool is_float;
  bool is_text;
  bool has_xlil_type;
  XsLilType xlil_type;
} XsHirPrimitiveInfo;

const XsHirPrimitiveInfo *xs_hir_primitive_find(const char *name, size_t length);

#endif
