// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <string>
#include <utility>

#include "Visual/XSharp/Legacy/lil.hpp"

namespace
{
    using xs::lil::Builder;
    using xs::lil::Error;
    using xs::lil::Module;
    using xs::lil::Type;
    using xs::lil::TypeKind;

    TEST_CASE("C++ XLIL API owns, verifies, and emits a module", "[lil][cpp]")
    {
        Module module{ "CppProducer" };
        const auto function = module.define_function("main", Type::scalar(TypeKind::I32));
        Builder builder{ module };
        const auto entry = builder.append_block(function, "entry");
        const auto value = builder.constant_i32(7);
        builder.return_value(value);

        module.verify();
        const auto text = module.emit_text();
        CHECK(module.name() == "CppProducer");
        CHECK(module.text_version() == XS_LIL_TEXT_VERSION);
        CHECK(text.starts_with(".xlil version 1\n.xlil module CppProducer\n"));
        CHECK(text.find("%r0:i32 = const.i32 7") != std::string::npos);
        CHECK(text.find("ret %r0") != std::string::npos);
        CHECK(entry.native_handle() != nullptr);
    }

    TEST_CASE("C++ XLIL API supports declarations and direct calls", "[lil][cpp]")
    {
        Module module{ "Calls" };
        const std::array parameters{ Type::scalar(TypeKind::I64) };
        module.declare_function("identity", Type::scalar(TypeKind::I64), parameters);
        const auto main = module.define_function("main", Type::scalar(TypeKind::I64), parameters);
        Builder builder{ module };
        const auto entry = builder.append_block(main, "entry");
        const std::array arguments{ XsLilValueId{ 0U } };
        const auto result = builder.call("identity", arguments);
        builder.return_value(result);

        CHECK(entry.native_handle() != nullptr);
        CHECK(module.emit_text().find("%r1:i64 = call identity(%r0)") != std::string::npos);
    }

    TEST_CASE("C++ XLIL parser preserves ownership across moves", "[lil][cpp]")
    {
        constexpr auto source = R"(.xlil version 1
.xlil module Parsed
.func main : () -> i32
bb0.entry:
  %r0:i32 = const.i32 0
  ret %r0
.end
)";
        auto parsed = Module::parse("Parsed.xlil", source);
        Module moved{ std::move(parsed) };
        CHECK(moved.name() == "Parsed");
        CHECK(moved.emit_text() == source);
    }

    TEST_CASE("C++ XLIL failures retain C ABI status", "[lil][cpp]")
    {
        Module module{ "Invalid" };
        const auto function = module.define_function("main", Type::scalar(TypeKind::I32));
        Builder builder{ module };
        const auto entry = builder.append_block(function, "entry");

        try
        {
            module.verify();
            FAIL("verification unexpectedly succeeded");
        }
        catch (const Error &error)
        {
            CHECK(error.status() == XS_LIL_INVALID_ARGUMENT);
            CHECK(std::string{ error.what() }.find("missing a terminator") != std::string::npos);
        }
        CHECK(entry.native_handle() != nullptr);
    }
} // namespace
