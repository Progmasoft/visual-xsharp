// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

#include "Visual/XSharp/lil-c/model.hh"

#include <array>
#include <cstddef>

extern "C" const char *xs_lil_type_name(XsLilType type)
{
    static constexpr std::array names{
        "void", "bool", "u8",  "i8",  "u16", "i16",  "u32", "i32",    "u64",       "i64",
        "u128", "i128", "f16", "f32", "f64", "f128", "str", "string", "aggregate", "array",
    };
    const auto index = static_cast<std::size_t>(type.kind);
    return index < names.size() ? names[index] : "unknown";
}
