// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
//

#ifndef XS_LIL_BUILDER_HPP
#define XS_LIL_BUILDER_HPP

#include <cstdint>
#include <span>
#include <string_view>

#include "Visual/XSharp/lil-c/builder.hh"
#include "Visual/XSharp/lil/Handles.hpp"
#include "Visual/XSharp/lil/Module.hpp"

namespace xs::lil
{
    class Builder final
    {
    public:
        explicit Builder(Module &module);
        ~Builder();

        Builder(const Builder &) = delete;
        Builder &
        operator=(const Builder &) = delete;
        Builder(Builder &&other) noexcept;
        Builder &
        operator=(Builder &&other) noexcept;

        [[nodiscard]] Block
        append_block(Function function, std::string_view label);
        void
        position_at_end(Block block);

        [[nodiscard]] ValueId
        constant_i32(std::int32_t value);
        [[nodiscard]] ValueId
        constant_i64(std::int64_t value);
        [[nodiscard]] ValueId
        constant_bool(bool value);
        [[nodiscard]] ValueId
        call(std::string_view callee, std::span<const ValueId> arguments = {});

        void
        return_void();
        void
        return_value(ValueId value);
        void
        branch(Block target);
        void
        branch_if(ValueId condition, Block then_block, Block else_block);

        [[nodiscard]] XsLilBuilder *
        native_handle() noexcept;

    private:
        XsLilBuilder *value_{};
    };
} // namespace xs::lil

#endif
