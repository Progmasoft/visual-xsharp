// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

#include "Visual/XSharp/lil/Error.hpp"

#include <utility>

namespace xs::lil
{
Error::Error(const XsLilStatus status, std::string message) : std::runtime_error(std::move(message)), status_(status) {}

XsLilStatus Error::status() const noexcept
{
    return status_;
}

void throw_if_failed(const XsLilStatus status, const XsLilError &error, const std::string_view operation)
{
    if(status == XS_LIL_OK)
        return;

    const auto *status_name = xs_lil_status_name(status);
    const auto *message = xs_lil_error_message(&error);
    std::string detail{operation};
    detail += " failed: ";
    detail += message == nullptr ? "XLIL error" : message;
    detail += " (";
    detail += status_name == nullptr ? "unknown" : status_name;
    detail += ')';
    throw Error{status, std::move(detail)};
}
} // namespace xs::lil
