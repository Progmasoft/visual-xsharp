// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

#include "xs/lil/Module.hxx"

#include "xs/lil-c/function.h"
#include "xs/lil-c/module.h"
#include "xs/lil-c/text.h"
#include "xs/lil/Error.hxx"

#include <utility>
#include <vector>

namespace xs::lil
{
namespace
{
[[nodiscard]] std::vector<XsLilType> native_types(const std::span<const Type> types)
{
  std::vector<XsLilType> result;
  result.reserve(types.size());
  for(const auto type : types)
    result.push_back(type.native_handle());
  return result;
}
} // namespace

Module::Module(const std::string_view name)
{
  const std::string owned_name{name};
  XsLilError error{};
  throw_if_failed(xs_lil_module_create(owned_name.c_str(), &value_, &error), error, "create XLIL module");
}

Module::Module(XsLilModule *const value) noexcept : value_(value) {}

Module::~Module()
{
  xs_lil_module_destroy(value_);
}

Module::Module(Module &&other) noexcept : value_(std::exchange(other.value_, nullptr)) {}

Module &Module::operator=(Module &&other) noexcept
{
  if(this == &other)
    return *this;
  xs_lil_module_destroy(value_);
  value_ = std::exchange(other.value_, nullptr);
  return *this;
}

Module Module::parse(const std::string_view path, const std::string_view text)
{
  const std::string owned_path{path};
  XsLilModule *module{};
  XsLilError error{};
  throw_if_failed(xs_lil_module_parse_text(owned_path.c_str(), text.data(), text.size(), &module, &error), error,
                  "parse XLIL module");
  return Module{module};
}

std::string_view Module::name() const noexcept
{
  const auto *value = xs_lil_module_name(value_);
  return value == nullptr ? std::string_view{} : std::string_view{value};
}

std::uint32_t Module::text_version() const noexcept
{
  return xs_lil_module_text_version(value_);
}

std::string Module::emit_text() const
{
  XsLilText text{};
  XsLilError error{};
  throw_if_failed(xs_lil_module_emit_text(value_, &text, &error), error, "emit XLIL text");
  std::string result{xs_lil_text_data(&text), xs_lil_text_length(&text)};
  xs_lil_text_destroy(&text);
  return result;
}

void Module::verify() const
{
  XsLilError error{};
  throw_if_failed(xs_lil_module_verify(value_, &error), error, "verify XLIL module");
}

void Module::declare_function(const std::string_view name, const Type return_type,
                              const std::span<const Type> parameters)
{
  const std::string owned_name{name};
  const auto native_parameters = native_types(parameters);
  XsLilError error{};
  throw_if_failed(xs_lil_module_add_function(value_, owned_name.c_str(), return_type.native_handle(),
                                             native_parameters.data(), native_parameters.size(), &error),
                  error, "declare XLIL function");
}

Function Module::define_function(const std::string_view name, const Type return_type,
                                 const std::span<const Type> parameters)
{
  const std::string owned_name{name};
  const auto native_parameters = native_types(parameters);
  XsLilFunction *function{};
  XsLilError error{};
  throw_if_failed(xs_lil_module_add_function_definition(value_, owned_name.c_str(), return_type.native_handle(),
                                                        native_parameters.data(), native_parameters.size(), &function,
                                                        &error),
                  error, "define XLIL function");
  return Function{function};
}

XsLilModule *Module::native_handle() noexcept
{
  return value_;
}

const XsLilModule *Module::native_handle() const noexcept
{
  return value_;
}
} // namespace xs::lil
