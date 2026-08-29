// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

#include "Visual/XSharp/lil/Builder.hpp"

#include "Visual/XSharp/lil-c/builder.hh"
#include "Visual/XSharp/lil/Error.hpp"

#include <string>
#include <utility>

namespace xs::lil
{
Builder::Builder(Module &module)
{
    XsLilError error{};
    throw_if_failed(xs_lil_builder_create(module.native_handle(), &value_, &error), error, "create XLIL builder");
}

Builder::~Builder()
{
    xs_lil_builder_destroy(value_);
}

Builder::Builder(Builder &&other) noexcept : value_(std::exchange(other.value_, nullptr)) {}

Builder &Builder::operator=(Builder &&other) noexcept
{
    if(this == &other)
        return *this;
    xs_lil_builder_destroy(value_);
    value_ = std::exchange(other.value_, nullptr);
    return *this;
}

Block Builder::append_block(const Function function, const std::string_view label)
{
    const std::string owned_label{label};
    XsLilBlock *block{};
    XsLilError error{};
    throw_if_failed(xs_lil_builder_append_block(value_, function.native_handle(), owned_label.c_str(), &block, &error),
                    error, "append XLIL block");
    return Block{block};
}

void Builder::position_at_end(const Block block)
{
    XsLilError error{};
    throw_if_failed(xs_lil_builder_position_at_end(value_, block.native_handle(), &error), error,
                    "position XLIL builder");
}

ValueId Builder::constant_i32(const std::int32_t value)
{
    ValueId result{XS_LIL_INVALID_VALUE_ID};
    XsLilError error{};
    throw_if_failed(xs_lil_builder_const_i32(value_, value, &result, &error), error, "build XLIL const.i32");
    return result;
}

ValueId Builder::constant_i64(const std::int64_t value)
{
    ValueId result{XS_LIL_INVALID_VALUE_ID};
    XsLilError error{};
    throw_if_failed(xs_lil_builder_const_i64(value_, value, &result, &error), error, "build XLIL const.i64");
    return result;
}

ValueId Builder::constant_bool(const bool value)
{
    ValueId result{XS_LIL_INVALID_VALUE_ID};
    XsLilError error{};
    throw_if_failed(xs_lil_builder_const_bool(value_, value, &result, &error), error, "build XLIL const.bool");
    return result;
}

ValueId Builder::call(const std::string_view callee, const std::span<const ValueId> arguments)
{
    const std::string owned_callee{callee};
    ValueId result{XS_LIL_INVALID_VALUE_ID};
    XsLilError error{};
    throw_if_failed(
        xs_lil_builder_call(value_, owned_callee.c_str(), arguments.data(), arguments.size(), &result, &error), error,
        "build XLIL call");
    return result;
}

void Builder::return_void()
{
    XsLilError error{};
    throw_if_failed(xs_lil_builder_return(value_, &error), error, "build XLIL return");
}

void Builder::return_value(const ValueId value)
{
    XsLilError error{};
    throw_if_failed(xs_lil_builder_return_value(value_, value, &error), error, "build XLIL return value");
}

void Builder::branch(const Block target)
{
    XsLilError error{};
    throw_if_failed(xs_lil_builder_branch(value_, target.native_handle(), &error), error, "build XLIL branch");
}

void Builder::branch_if(const ValueId condition, const Block then_block, const Block else_block)
{
    XsLilError error{};
    throw_if_failed(
        xs_lil_builder_branch_if(value_, condition, then_block.native_handle(), else_block.native_handle(), &error),
        error, "build XLIL conditional branch");
}

XsLilBuilder *Builder::native_handle() noexcept
{
    return value_;
}
} // namespace xs::lil
