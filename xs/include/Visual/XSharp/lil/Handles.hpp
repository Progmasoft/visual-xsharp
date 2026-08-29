// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
//

#ifndef XS_LIL_HANDLES_HPP
#define XS_LIL_HANDLES_HPP

#include "Visual/XSharp/lil-c/model.hh"

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
