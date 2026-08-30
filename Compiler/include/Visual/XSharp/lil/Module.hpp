// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
//

#ifndef XS_LIL_MODULE_HPP
#define XS_LIL_MODULE_HPP

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include "Visual/XSharp/lil/Handles.hpp"
#include "Visual/XSharp/lil/Type.hpp"

namespace xs::lil
{
    class Module final
    {
    public:
        explicit Module(std::string_view name);
        ~Module();

        Module(const Module &) = delete;
        Module &
        operator=(const Module &) = delete;
        Module(Module &&other) noexcept;
        Module &
        operator=(Module &&other) noexcept;

        [[nodiscard]] static Module
        parse(std::string_view path, std::string_view text);

        [[nodiscard]] std::string_view
        name() const noexcept;
        [[nodiscard]] std::uint32_t
        text_version() const noexcept;
        [[nodiscard]] std::string
        emit_text() const;
        void
        verify() const;

        void
        declare_function(std::string_view name, Type return_type, std::span<const Type> parameters = {});
        [[nodiscard]] Function
        define_function(std::string_view name, Type return_type, std::span<const Type> parameters = {});

        [[nodiscard]] XsLilModule *
        native_handle() noexcept;
        [[nodiscard]] const XsLilModule *
        native_handle() const noexcept;

    private:
        explicit Module(XsLilModule *value) noexcept;

        XsLilModule *value_{};
    };
} // namespace xs::lil

#endif
