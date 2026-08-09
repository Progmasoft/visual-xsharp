// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

#include "xs/codegen/Plan.hpp"

#include <fmt/format.h>

#include <cstddef>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
struct CodegenUnit final
{
    std::string name;
    std::vector<std::string> functions;
};

[[nodiscard]] XsCodegenUnitsStatus set_error(XsCodegenUnitsError *error, XsCodegenUnitsStatus status,
                                             std::string_view message) noexcept
{
    if(error != nullptr)
    {
        error->status = status;
        const auto output = fmt::format_to_n(error->message, sizeof(error->message) - 1U, "{}", message);
        *output.out = '\0';
    }
    return status;
}

void clear_error(XsCodegenUnitsError *error) noexcept
{
    if(error != nullptr)
    {
        *error = XsCodegenUnitsError{};
        error->status = XS_CODEGEN_UNITS_OK;
    }
}

[[nodiscard]] std::string_view checked_text(const char *text)
{
    return text == nullptr ? std::string_view{} : std::string_view{text};
}

[[nodiscard]] std::string unit_name_for_function(std::string_view module_name, std::string_view function_name)
{
    const auto separator = function_name.rfind('.');
    if(separator != std::string_view::npos && separator != 0U)
        return std::string{function_name.substr(0U, separator)};
    return std::string{module_name};
}
} // namespace

struct XsCodegenPlan
{
    std::vector<CodegenUnit> units;
};

namespace
{
[[nodiscard]] CodegenUnit *find_unit(XsCodegenPlan &plan, std::string_view name) noexcept
{
    for(auto &unit : plan.units)
    {
        if(unit.name == name)
            return &unit;
    }
    return nullptr;
}

[[nodiscard]] CodegenUnit &find_or_append_unit(XsCodegenPlan &plan, std::string name)
{
    if(auto *unit = find_unit(plan, name); unit != nullptr)
        return *unit;
    plan.units.push_back(CodegenUnit{.name = std::move(name), .functions = {}});
    return plan.units.back();
}

template <typename AppendEntries>
[[nodiscard]] XsCodegenUnitsStatus create_plan(XsCodegenPlan **plan, XsCodegenUnitsError *error,
                                               AppendEntries &&append_entries)
{
    try
    {
        auto created = std::make_unique<XsCodegenPlan>();
        append_entries(*created);
        *plan = created.release();
        return XS_CODEGEN_UNITS_OK;
    }
    catch(const std::bad_alloc &)
    {
        return set_error(error, XS_CODEGEN_UNITS_ALLOCATION_FAILED, "out of memory while creating a codegen plan");
    }
}
} // namespace

extern "C" XsCodegenUnitsStatus xs_codegen_plan_create_from_mir(const XsMirModule *module, XsCodegenPlan **plan,
                                                                XsCodegenUnitsError *error)
{
    clear_error(error);
    if(plan != nullptr)
        *plan = nullptr;
    if(module == nullptr || plan == nullptr)
        return set_error(error, XS_CODEGEN_UNITS_INVALID_ARGUMENT, "valid MIR module and output plan are required");

    return create_plan(plan, error,
                       [module](XsCodegenPlan &created)
                       {
                           const std::string_view module_name = checked_text(xs_mir_module_name(module));
                           for(std::size_t index = 0; index < xs_mir_module_function_count(module); ++index)
                           {
                               const XsMirFunction *function = xs_mir_module_function_at(module, index);
                               const std::string_view function_name = checked_text(xs_mir_function_name(function));
                               auto &unit =
                                   find_or_append_unit(created, unit_name_for_function(module_name, function_name));
                               unit.functions.emplace_back(function_name);
                           }
                       });
}

extern "C" XsCodegenUnitsStatus xs_codegen_plan_create_from_mono(const XsMonoPlan *mono, XsCodegenPlan **plan,
                                                                 XsCodegenUnitsError *error)
{
    clear_error(error);
    if(plan != nullptr)
        *plan = nullptr;
    if(mono == nullptr || plan == nullptr)
        return set_error(error, XS_CODEGEN_UNITS_INVALID_ARGUMENT, "valid mono plan and output plan are required");

    try
    {
        return create_plan(plan, error,
                           [mono](XsCodegenPlan &created)
                           {
                               for(std::size_t index = 0; index < xs_mono_plan_entry_count(mono); ++index)
                               {
                                   const std::string_view unit_name =
                                       checked_text(xs_mono_plan_entry_unit_name(mono, index));
                                   const std::string_view symbol_name =
                                       checked_text(xs_mono_plan_entry_symbol_name(mono, index));
                                   if(unit_name.empty() || symbol_name.empty())
                                       throw std::invalid_argument{"mono plan contains an invalid codegen entry"};
                                   auto &unit = find_or_append_unit(created, std::string{unit_name});
                                   unit.functions.emplace_back(symbol_name);
                               }
                           });
    }
    catch(const std::invalid_argument &error_value)
    {
        return set_error(error, XS_CODEGEN_UNITS_INVALID_ARGUMENT, error_value.what());
    }
}

extern "C" void xs_codegen_plan_destroy(XsCodegenPlan *plan)
{
    delete plan;
}

extern "C" std::size_t xs_codegen_plan_unit_count(const XsCodegenPlan *plan)
{
    return plan == nullptr ? 0U : plan->units.size();
}

extern "C" const char *xs_codegen_plan_unit_name(const XsCodegenPlan *plan, std::size_t unit_index)
{
    if(plan == nullptr || unit_index >= plan->units.size())
        return nullptr;
    return plan->units[unit_index].name.c_str();
}

extern "C" std::size_t xs_codegen_plan_unit_function_count(const XsCodegenPlan *plan, std::size_t unit_index)
{
    if(plan == nullptr || unit_index >= plan->units.size())
        return 0U;
    return plan->units[unit_index].functions.size();
}

extern "C" const char *xs_codegen_plan_unit_function_name(const XsCodegenPlan *plan, std::size_t unit_index,
                                                          std::size_t function_index)
{
    if(plan == nullptr || unit_index >= plan->units.size() ||
       function_index >= plan->units[unit_index].functions.size())
        return nullptr;
    return plan->units[unit_index].functions[function_index].c_str();
}

namespace xs::codegen
{
namespace
{
[[noreturn]] void throw_error(const XsCodegenUnitsError &error, std::string_view operation)
{
    const std::string_view detail = error.message[0] == '\0' ? "unknown planning error" : error.message;
    throw Error{error.status, fmt::format("{}: {}", operation, detail)};
}
} // namespace

Error::Error(XsCodegenUnitsStatus status, std::string_view message)
    : std::runtime_error{std::string{message}}, status_{status}
{
}

XsCodegenUnitsStatus Error::status() const noexcept
{
    return status_;
}

Unit::Unit(const XsCodegenPlan &plan, std::size_t index) noexcept : plan_{&plan}, index_{index} {}

std::string_view Unit::name() const noexcept
{
    const char *value = xs_codegen_plan_unit_name(plan_, index_);
    return value == nullptr ? std::string_view{} : std::string_view{value};
}

std::size_t Unit::size() const noexcept
{
    return xs_codegen_plan_unit_function_count(plan_, index_);
}

bool Unit::empty() const noexcept
{
    return size() == 0U;
}

std::string_view Unit::function(std::size_t index) const
{
    const char *value = xs_codegen_plan_unit_function_name(plan_, index_, index);
    if(value == nullptr)
        throw std::out_of_range{"codegen unit function index is out of range"};
    return value;
}

Plan::Plan(const XsMirModule &module)
{
    XsCodegenUnitsError error{};
    const auto status = xs_codegen_plan_create_from_mir(&module, &value_, &error);
    if(status != XS_CODEGEN_UNITS_OK)
        throw_error(error, "create codegen plan from MIR");
}

Plan::Plan(const XsMonoPlan &mono)
{
    XsCodegenUnitsError error{};
    const auto status = xs_codegen_plan_create_from_mono(&mono, &value_, &error);
    if(status != XS_CODEGEN_UNITS_OK)
        throw_error(error, "create codegen plan from monomorphization");
}

Plan::Plan(XsCodegenPlan *value) noexcept : value_{value} {}

Plan::~Plan()
{
    xs_codegen_plan_destroy(value_);
}

Plan::Plan(Plan &&other) noexcept : value_{std::exchange(other.value_, nullptr)} {}

Plan &Plan::operator=(Plan &&other) noexcept
{
    if(this != &other)
    {
        xs_codegen_plan_destroy(value_);
        value_ = std::exchange(other.value_, nullptr);
    }
    return *this;
}

std::size_t Plan::size() const noexcept
{
    return xs_codegen_plan_unit_count(value_);
}

bool Plan::empty() const noexcept
{
    return size() == 0U;
}

Unit Plan::at(std::size_t index) const
{
    if(index >= size())
        throw std::out_of_range{"codegen unit index is out of range"};
    return Unit{*value_, index};
}

Unit Plan::operator[](std::size_t index) const
{
    return at(index);
}

XsCodegenPlan *Plan::native_handle() noexcept
{
    return value_;
}

const XsCodegenPlan *Plan::native_handle() const noexcept
{
    return value_;
}
} // namespace xs::codegen
