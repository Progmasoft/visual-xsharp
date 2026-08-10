// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

#include "Visual/XSharp/codegen/Plan.hpp"
#include "Visual/XSharp/mono/Plan.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace
{
struct MirModuleDeleter final
{
    void operator()(XsMirModule *module) const noexcept
    {
        xs_mir_module_destroy(module);
    }
};

using MirModule = std::unique_ptr<XsMirModule, MirModuleDeleter>;

[[nodiscard]] MirModule create_module(std::string_view name)
{
    XsMirModule *module{};
    XsMirError error{};
    const std::string owned_name{name};
    REQUIRE(xs_mir_module_create(owned_name.c_str(), &module, &error) == XS_MIR_OK);
    REQUIRE(module != nullptr);
    return MirModule{module};
}

void add_declaration(XsMirModule &module, std::string_view name)
{
    XsMirError error{};
    const std::string owned_name{name};
    constexpr XsMirType return_type{.kind = XS_LIL_TYPE_VOID};
    REQUIRE(xs_mir_module_add_function_declaration(&module, owned_name.c_str(), return_type, nullptr, 0U, &error) ==
            XS_MIR_OK);
}

[[nodiscard]] MirModule create_grouped_module()
{
    auto module = create_module("root");
    add_declaration(*module, "app.main");
    add_declaration(*module, "app.helper");
    add_declaration(*module, "tools.log");
    add_declaration(*module, "standalone");
    return module;
}

TEST_CASE("GNU++ monomorphization plan preserves stable registry entries", "[planning][mono][cpp]")
{
    auto module = create_grouped_module();
    xs::mono::Plan plan{*module};

    REQUIRE(plan.size() == 4U);
    CHECK_FALSE(plan.empty());
    CHECK(plan[0].unit_name == "app");
    CHECK(plan[0].source_name == "app.main");
    CHECK(plan[0].symbol_name == "_XS_FN_app_main_G0");
    CHECK(plan[1].unit_name == "app");
    CHECK(plan[1].symbol_name == "_XS_FN_app_helper_G0");
    CHECK(plan[2].unit_name == "tools");
    CHECK(plan[3].unit_name == "root");
    CHECK(plan.native_handle() != nullptr);
    CHECK_THROWS_AS(plan.at(4U), std::out_of_range);
}

TEST_CASE("GNU++ monomorphization plan owns entries across moves", "[planning][mono][cpp]")
{
    auto module = create_grouped_module();
    xs::mono::Plan original{*module};
    xs::mono::Plan moved{std::move(original)};

    REQUIRE(moved.size() == 4U);
    CHECK(moved[2].source_name == "tools.log");

    auto replacement_module = create_module("replacement");
    add_declaration(*replacement_module, "replacement.run");
    xs::mono::Plan replacement{*replacement_module};
    replacement = std::move(moved);
    REQUIRE(replacement.size() == 4U);
    CHECK(replacement[3].source_name == "standalone");
}

TEST_CASE("GNU++ codegen plan groups MIR functions by qualified owner", "[planning][codegen][cpp]")
{
    auto module = create_grouped_module();
    xs::codegen::Plan plan{*module};

    REQUIRE(plan.size() == 3U);
    CHECK_FALSE(plan.empty());
    const auto app = plan[0];
    CHECK(app.name() == "app");
    REQUIRE(app.size() == 2U);
    CHECK(app.function(0U) == "app.main");
    CHECK(app.function(1U) == "app.helper");
    CHECK(plan[1].name() == "tools");
    CHECK(plan[1].function(0U) == "tools.log");
    CHECK(plan[2].name() == "root");
    CHECK(plan[2].function(0U) == "standalone");
    CHECK_THROWS_AS(plan.at(3U), std::out_of_range);
    CHECK_THROWS_AS(app.function(2U), std::out_of_range);
}

TEST_CASE("GNU++ codegen plan consumes monomorphized symbol names", "[planning][codegen][cpp]")
{
    auto module = create_grouped_module();
    xs::mono::Plan mono{*module};
    xs::codegen::Plan codegen{*mono.native_handle()};

    REQUIRE(codegen.size() == 3U);
    const auto app = codegen[0];
    REQUIRE(app.size() == 2U);
    CHECK(app.function(0U) == "_XS_FN_app_main_G0");
    CHECK(app.function(1U) == "_XS_FN_app_helper_G0");
    CHECK(codegen[2].function(0U) == "_XS_FN_standalone_G0");
}

TEST_CASE("planning C ABI reports invalid inputs without throwing", "[planning][abi][cpp]")
{
    XsMonoError mono_error{};
    XsMonoPlan *mono = reinterpret_cast<XsMonoPlan *>(0x1);
    CHECK(xs_mono_plan_create_for_concrete_mir(nullptr, &mono, &mono_error) == XS_MONO_INVALID_ARGUMENT);
    CHECK(mono == nullptr);
    CHECK(std::string_view{mono_error.message}.find("valid MIR module") != std::string_view::npos);
    CHECK(xs_mono_plan_entry_count(nullptr) == 0U);
    CHECK(xs_mono_plan_entry_unit_name(nullptr, 0U) == nullptr);

    XsCodegenUnitsError codegen_error{};
    XsCodegenPlan *codegen = reinterpret_cast<XsCodegenPlan *>(0x1);
    CHECK(xs_codegen_plan_create_from_mir(nullptr, &codegen, &codegen_error) == XS_CODEGEN_UNITS_INVALID_ARGUMENT);
    CHECK(codegen == nullptr);
    CHECK(std::string_view{codegen_error.message}.find("valid MIR module") != std::string_view::npos);
    CHECK(xs_codegen_plan_unit_count(nullptr) == 0U);
    CHECK(xs_codegen_plan_unit_name(nullptr, 0U) == nullptr);
    CHECK(xs_codegen_plan_unit_function_count(nullptr, 0U) == 0U);
    CHECK(xs_codegen_plan_unit_function_name(nullptr, 0U, 0U) == nullptr);
}

TEST_CASE("empty MIR produces empty movable planning views", "[planning][cpp]")
{
    auto module = create_module("empty");
    xs::mono::Plan mono{*module};
    xs::codegen::Plan codegen{*module};
    CHECK(mono.empty());
    CHECK(codegen.empty());

    xs::codegen::Plan moved{std::move(codegen)};
    CHECK(moved.empty());
}
} // namespace
