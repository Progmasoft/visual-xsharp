// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

#ifndef XS_LIL_BUILDER_HXX
#define XS_LIL_BUILDER_HXX

#include "xs/lil-c/builder.h"
#include "xs/lil/Handles.hxx"
#include "xs/lil/Module.hxx"

#include <cstdint>
#include <span>
#include <string_view>

namespace xs::lil
{
class Builder final
{
public:
  explicit Builder(Module &module);
  ~Builder();

  Builder(const Builder &) = delete;
  Builder &operator=(const Builder &) = delete;
  Builder(Builder &&other) noexcept;
  Builder &operator=(Builder &&other) noexcept;

  [[nodiscard]] Block append_block(Function function, std::string_view label);
  void position_at_end(Block block);

  [[nodiscard]] ValueId constant_i32(std::int32_t value);
  [[nodiscard]] ValueId constant_i64(std::int64_t value);
  [[nodiscard]] ValueId constant_bool(bool value);
  [[nodiscard]] ValueId call(std::string_view callee, std::span<const ValueId> arguments = {});

  void return_void();
  void return_value(ValueId value);
  void branch(Block target);
  void branch_if(ValueId condition, Block then_block, Block else_block);

  [[nodiscard]] XsLilBuilder *native_handle() noexcept;

private:
  XsLilBuilder *value_{};
};
} // namespace xs::lil

#endif
