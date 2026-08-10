// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0
//

#ifndef XS_CODEGEN_PLAN_HPP
#define XS_CODEGEN_PLAN_HPP

#include "Visual/XSharp/codegen/units.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>

namespace xs::codegen
{
class Error final : public std::runtime_error
{
public:
    Error(XsCodegenUnitsStatus status, std::string_view message);

    [[nodiscard]] XsCodegenUnitsStatus status() const noexcept;

private:
    XsCodegenUnitsStatus status_;
};

class Unit final
{
public:
    [[nodiscard]] std::string_view name() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] std::string_view function(std::size_t index) const;

private:
    friend class Plan;
    Unit(const XsCodegenPlan &plan, std::size_t index) noexcept;

    const XsCodegenPlan *plan_{};
    std::size_t index_{};
};

class Plan final
{
public:
    explicit Plan(const XsMirModule &module);
    explicit Plan(const XsMonoPlan &mono);
    ~Plan();

    Plan(const Plan &) = delete;
    Plan &operator=(const Plan &) = delete;
    Plan(Plan &&other) noexcept;
    Plan &operator=(Plan &&other) noexcept;

    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] Unit at(std::size_t index) const;
    [[nodiscard]] Unit operator[](std::size_t index) const;

    [[nodiscard]] XsCodegenPlan *native_handle() noexcept;
    [[nodiscard]] const XsCodegenPlan *native_handle() const noexcept;

private:
    explicit Plan(XsCodegenPlan *value) noexcept;

    XsCodegenPlan *value_{};
};
} // namespace xs::codegen

#endif
