// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
//

#ifndef XS_LIL_ERROR_HPP
#define XS_LIL_ERROR_HPP

#include "Visual/XSharp/lil-c/model.hh"

#include <stdexcept>
#include <string>
#include <string_view>

namespace xs::lil
{
class Error final : public std::runtime_error
{
public:
    Error(XsLilStatus status, std::string message);

    [[nodiscard]] XsLilStatus status() const noexcept;

private:
    XsLilStatus status_;
};

void throw_if_failed(XsLilStatus status, const XsLilError &error, std::string_view operation);
} // namespace xs::lil

#endif
