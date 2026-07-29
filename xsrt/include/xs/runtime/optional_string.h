/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef XS_RUNTIME_OPTIONAL_STRING_H
#define XS_RUNTIME_OPTIONAL_STRING_H

#include <stddef.h>
#include <stdint.h>

typedef enum XsRuntimeStatus
{
  XS_RUNTIME_OK = 0,
  XS_RUNTIME_INVALID_ARGUMENT = 1,
  XS_RUNTIME_ALLOCATION_FAILED = 2,
  XS_RUNTIME_SIZE_OVERFLOW = 3,
  XS_RUNTIME_VALUE_IS_NONE = 4
} XsRuntimeStatus;

typedef struct XsRuntimeStrView
{
  const uint16_t *units;
  size_t length;
} XsRuntimeStrView;

typedef struct XsRuntimeStringBox XsRuntimeStringBox;

typedef struct XsRuntimeOptionalStr
{
  XsRuntimeStringBox *box;
} XsRuntimeOptionalStr;

XsRuntimeOptionalStr xs_runtime_optional_str_none(void);
XsRuntimeStatus xs_runtime_optional_str_some(XsRuntimeStrView value, XsRuntimeOptionalStr *result);
XsRuntimeStatus xs_runtime_optional_str_clone(const XsRuntimeOptionalStr *value, XsRuntimeOptionalStr *result);
void xs_runtime_optional_str_drop(XsRuntimeOptionalStr *value);
bool xs_runtime_optional_str_is_some(const XsRuntimeOptionalStr *value);
XsRuntimeStatus xs_runtime_optional_str_borrow(const XsRuntimeOptionalStr *value, XsRuntimeStrView *result);

#endif
