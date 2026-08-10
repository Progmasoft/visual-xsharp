// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0
//

#ifndef XS_LIL_TYPE_HPP
#define XS_LIL_TYPE_HPP

#include "Visual/XSharp/lil-c/model.hh"

#include <cstdint>

namespace xs::lil
{
enum class TypeKind : std::uint8_t
{
    Void = XS_LIL_TYPE_VOID,
    Bool = XS_LIL_TYPE_BOOL,
    U8 = XS_LIL_TYPE_U8,
    I8 = XS_LIL_TYPE_I8,
    U16 = XS_LIL_TYPE_U16,
    I16 = XS_LIL_TYPE_I16,
    U32 = XS_LIL_TYPE_U32,
    I32 = XS_LIL_TYPE_I32,
    U64 = XS_LIL_TYPE_U64,
    I64 = XS_LIL_TYPE_I64,
    U128 = XS_LIL_TYPE_U128,
    I128 = XS_LIL_TYPE_I128,
    F16 = XS_LIL_TYPE_F16,
    F32 = XS_LIL_TYPE_F32,
    F64 = XS_LIL_TYPE_F64,
    F128 = XS_LIL_TYPE_F128,
    Str = XS_LIL_TYPE_STR,
    String = XS_LIL_TYPE_STRING,
    Aggregate = XS_LIL_TYPE_AGGREGATE,
    Array = XS_LIL_TYPE_ARRAY,
};

class Type final
{
public:
    [[nodiscard]] static constexpr Type scalar(const TypeKind kind) noexcept
    {
        return Type{{static_cast<XsLilTypeKind>(kind), 0U}};
    }

    [[nodiscard]] static constexpr Type aggregate(const std::uint32_t registry_id) noexcept
    {
        return Type{{XS_LIL_TYPE_AGGREGATE, registry_id}};
    }

    [[nodiscard]] static constexpr Type array(const std::uint32_t registry_id) noexcept
    {
        return Type{{XS_LIL_TYPE_ARRAY, registry_id}};
    }

    [[nodiscard]] constexpr TypeKind kind() const noexcept
    {
        return static_cast<TypeKind>(value_.kind);
    }

    [[nodiscard]] constexpr std::uint32_t registry_id() const noexcept
    {
        return value_.registry_id;
    }

    [[nodiscard]] constexpr XsLilType native_handle() const noexcept
    {
        return value_;
    }

    [[nodiscard]] friend constexpr bool operator==(const Type &left, const Type &right) noexcept
    {
        return left.value_.kind == right.value_.kind && left.value_.registry_id == right.value_.registry_id;
    }

private:
    explicit constexpr Type(const XsLilType value) noexcept : value_(value) {}

    XsLilType value_{};
};
} // namespace xs::lil

#endif
