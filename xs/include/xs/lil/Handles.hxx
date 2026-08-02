// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

#ifndef XS_LIL_HANDLES_HXX
#define XS_LIL_HANDLES_HXX

#include "xs/lil-c/model.h"

namespace xs::lil
{
class Function final
{
public:
  [[nodiscard]] XsLilFunction *native_handle() const noexcept
  {
    return value_;
  }

private:
  friend class Module;
  friend class Builder;

  explicit Function(XsLilFunction *value) noexcept : value_(value) {}

  XsLilFunction *value_{};
};

class Block final
{
public:
  [[nodiscard]] XsLilBlock *native_handle() const noexcept
  {
    return value_;
  }

private:
  friend class Builder;

  explicit Block(XsLilBlock *value) noexcept : value_(value) {}

  XsLilBlock *value_{};
};

using ValueId = XsLilValueId;
using SlotId = XsLilSlotId;
} // namespace xs::lil

#endif
