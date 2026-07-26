/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <xs-lang.chess031@slmails.com>
 * SPDX-License-Identifier: MPL-2.0
 */

#include <xs/runtime.h>

#include <stdlib.h>
#include <string.h>

struct XsRuntimeStringBox
{
  size_t length;
  uint16_t units[];
};

XsRuntimeOptionalStr xs_runtime_optional_str_none(void)
{
  return (XsRuntimeOptionalStr){.box = nullptr};
}

static XsRuntimeStatus allocate_box(XsRuntimeStrView value, XsRuntimeStringBox **result)
{
  if(result == nullptr || (value.units == nullptr && value.length != 0U))
    return XS_RUNTIME_INVALID_ARGUMENT;
  if(value.length > (SIZE_MAX - sizeof(XsRuntimeStringBox)) / sizeof(uint16_t))
    return XS_RUNTIME_SIZE_OVERFLOW;

  const size_t allocation_size = sizeof(XsRuntimeStringBox) + value.length * sizeof(uint16_t);
  XsRuntimeStringBox *box = malloc(allocation_size);
  if(box == nullptr)
    return XS_RUNTIME_ALLOCATION_FAILED;

  box->length = value.length;
  if(value.length != 0U)
    memcpy(box->units, value.units, value.length * sizeof(uint16_t));
  *result = box;
  return XS_RUNTIME_OK;
}

XsRuntimeStatus xs_runtime_optional_str_some(XsRuntimeStrView value, XsRuntimeOptionalStr *result)
{
  if(result == nullptr)
    return XS_RUNTIME_INVALID_ARGUMENT;

  XsRuntimeStringBox *box = nullptr;
  const XsRuntimeStatus status = allocate_box(value, &box);
  if(status != XS_RUNTIME_OK)
    return status;

  result->box = box;
  return XS_RUNTIME_OK;
}

XsRuntimeStatus xs_runtime_optional_str_clone(const XsRuntimeOptionalStr *value, XsRuntimeOptionalStr *result)
{
  if(value == nullptr || result == nullptr || value == result)
    return XS_RUNTIME_INVALID_ARGUMENT;
  if(value->box == nullptr)
  {
    *result = xs_runtime_optional_str_none();
    return XS_RUNTIME_OK;
  }

  const XsRuntimeStrView view = {
      .units = value->box->units,
      .length = value->box->length,
  };
  return xs_runtime_optional_str_some(view, result);
}

void xs_runtime_optional_str_drop(XsRuntimeOptionalStr *value)
{
  if(value == nullptr)
    return;
  free(value->box);
  value->box = nullptr;
}

bool xs_runtime_optional_str_is_some(const XsRuntimeOptionalStr *value)
{
  return value != nullptr && value->box != nullptr;
}

XsRuntimeStatus xs_runtime_optional_str_borrow(const XsRuntimeOptionalStr *value, XsRuntimeStrView *result)
{
  if(value == nullptr || result == nullptr)
    return XS_RUNTIME_INVALID_ARGUMENT;
  if(value->box == nullptr)
    return XS_RUNTIME_VALUE_IS_NONE;

  *result = (XsRuntimeStrView){
      .units = value->box->units,
      .length = value->box->length,
  };
  return XS_RUNTIME_OK;
}
