// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

#ifndef XS_MONO_PLAN_HPP
#define XS_MONO_PLAN_HPP

#include "xs/mono/plan.h"

#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>

namespace xs::mono
{
class Error final : public std::runtime_error
{
public:
    Error(XsMonoStatus status, std::string_view message);

    [[nodiscard]] XsMonoStatus status() const noexcept;

private:
    XsMonoStatus status_;
};

struct Entry final
{
    std::string_view unit_name;
    std::string_view source_name;
    std::string_view symbol_name;
};

class Plan final
{
public:
    explicit Plan(const XsMirModule &module);
    ~Plan();

    Plan(const Plan &) = delete;
    Plan &operator=(const Plan &) = delete;
    Plan(Plan &&other) noexcept;
    Plan &operator=(Plan &&other) noexcept;

    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] Entry at(std::size_t index) const;
    [[nodiscard]] Entry operator[](std::size_t index) const;

    [[nodiscard]] XsMonoPlan *native_handle() noexcept;
    [[nodiscard]] const XsMonoPlan *native_handle() const noexcept;

private:
    XsMonoPlan *value_{};
};
} // namespace xs::mono

#endif
