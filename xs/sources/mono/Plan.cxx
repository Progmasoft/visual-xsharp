// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

#include "xs/mono/Plan.hxx"

#include <fmt/format.h>

#include <algorithm>
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
struct MonoEntry final
{
    std::string unit_name;
    std::string source_name;
    std::string symbol_name;
};

[[nodiscard]] XsMonoStatus set_error(XsMonoError *error, XsMonoStatus status, std::string_view message) noexcept
{
    if(error != nullptr)
    {
        error->status = status;
        const auto output = fmt::format_to_n(error->message, sizeof(error->message) - 1U, "{}", message);
        *output.out = '\0';
    }
    return status;
}

void clear_error(XsMonoError *error) noexcept
{
    if(error != nullptr)
    {
        *error = XsMonoError{};
        error->status = XS_MONO_OK;
    }
}

[[nodiscard]] std::string unit_name_for_function(std::string_view module_name, std::string_view function_name)
{
    const auto separator = function_name.rfind('.');
    if(separator != std::string_view::npos && separator != 0U)
        return std::string{function_name.substr(0U, separator)};
    return std::string{module_name};
}

[[nodiscard]] bool is_ascii_alphanumeric(char character) noexcept
{
    return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
           (character >= '0' && character <= '9');
}

[[nodiscard]] std::string mangle_concrete_function(std::string_view name)
{
    std::string symbol{"_XS_FN_"};
    symbol.reserve(symbol.size() + name.size() + 3U);
    std::ranges::transform(name, std::back_inserter(symbol),
                           [](char character) { return is_ascii_alphanumeric(character) ? character : '_'; });
    symbol.append("_G0");
    return symbol;
}

[[nodiscard]] std::string_view checked_text(const char *text)
{
    return text == nullptr ? std::string_view{} : std::string_view{text};
}
} // namespace

struct XsMonoPlan
{
    std::vector<MonoEntry> entries;
};

extern "C" XsMonoStatus xs_mono_plan_create_for_concrete_mir(const XsMirModule *module, XsMonoPlan **plan,
                                                             XsMonoError *error)
{
    clear_error(error);
    if(plan != nullptr)
        *plan = nullptr;
    if(module == nullptr || plan == nullptr)
        return set_error(error, XS_MONO_INVALID_ARGUMENT, "valid MIR module and output plan are required");

    try
    {
        auto created = std::make_unique<XsMonoPlan>();
        const auto function_count = xs_mir_module_function_count(module);
        created->entries.reserve(function_count);
        const std::string_view module_name = checked_text(xs_mir_module_name(module));
        for(std::size_t index = 0; index < function_count; ++index)
        {
            const XsMirFunction *function = xs_mir_module_function_at(module, index);
            const std::string_view name = checked_text(xs_mir_function_name(function));
            if(name.empty())
                return set_error(error, XS_MONO_INVALID_ARGUMENT, "MIR module contains an unnamed function");
            created->entries.push_back(MonoEntry{.unit_name = unit_name_for_function(module_name, name),
                                                 .source_name = std::string{name},
                                                 .symbol_name = mangle_concrete_function(name)});
        }
        *plan = created.release();
        return XS_MONO_OK;
    }
    catch(const std::bad_alloc &)
    {
        return set_error(error, XS_MONO_ALLOCATION_FAILED, "out of memory while creating a monomorphization plan");
    }
}

extern "C" void xs_mono_plan_destroy(XsMonoPlan *plan)
{
    delete plan;
}

extern "C" std::size_t xs_mono_plan_entry_count(const XsMonoPlan *plan)
{
    return plan == nullptr ? 0U : plan->entries.size();
}

extern "C" const char *xs_mono_plan_entry_unit_name(const XsMonoPlan *plan, std::size_t index)
{
    if(plan == nullptr || index >= plan->entries.size())
        return nullptr;
    return plan->entries[index].unit_name.c_str();
}

extern "C" const char *xs_mono_plan_entry_source_name(const XsMonoPlan *plan, std::size_t index)
{
    if(plan == nullptr || index >= plan->entries.size())
        return nullptr;
    return plan->entries[index].source_name.c_str();
}

extern "C" const char *xs_mono_plan_entry_symbol_name(const XsMonoPlan *plan, std::size_t index)
{
    if(plan == nullptr || index >= plan->entries.size())
        return nullptr;
    return plan->entries[index].symbol_name.c_str();
}

namespace xs::mono
{
namespace
{
[[noreturn]] void throw_error(const XsMonoError &error, std::string_view operation)
{
    const std::string_view detail = error.message[0] == '\0' ? "unknown planning error" : error.message;
    throw Error{error.status, fmt::format("{}: {}", operation, detail)};
}
} // namespace

Error::Error(XsMonoStatus status, std::string_view message) : std::runtime_error{std::string{message}}, status_{status}
{
}

XsMonoStatus Error::status() const noexcept
{
    return status_;
}

Plan::Plan(const XsMirModule &module)
{
    XsMonoError error{};
    const auto status = xs_mono_plan_create_for_concrete_mir(&module, &value_, &error);
    if(status != XS_MONO_OK)
        throw_error(error, "create monomorphization plan");
}

Plan::~Plan()
{
    xs_mono_plan_destroy(value_);
}

Plan::Plan(Plan &&other) noexcept : value_{std::exchange(other.value_, nullptr)} {}

Plan &Plan::operator=(Plan &&other) noexcept
{
    if(this != &other)
    {
        xs_mono_plan_destroy(value_);
        value_ = std::exchange(other.value_, nullptr);
    }
    return *this;
}

std::size_t Plan::size() const noexcept
{
    return xs_mono_plan_entry_count(value_);
}

bool Plan::empty() const noexcept
{
    return size() == 0U;
}

Entry Plan::at(std::size_t index) const
{
    if(index >= size())
        throw std::out_of_range{"monomorphization plan entry index is out of range"};
    return Entry{.unit_name = xs_mono_plan_entry_unit_name(value_, index),
                 .source_name = xs_mono_plan_entry_source_name(value_, index),
                 .symbol_name = xs_mono_plan_entry_symbol_name(value_, index)};
}

Entry Plan::operator[](std::size_t index) const
{
    return at(index);
}

XsMonoPlan *Plan::native_handle() noexcept
{
    return value_;
}

const XsMonoPlan *Plan::native_handle() const noexcept
{
    return value_;
}
} // namespace xs::mono
