// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

#include "xs/lil/Error.hxx"

#include <fmt/format.h>

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
  throw Error{status, fmt::format("{} failed: {} ({})", operation, message == nullptr ? "XLIL error" : message,
                                  status_name == nullptr ? "unknown" : status_name)};
}
} // namespace xs::lil
