// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

#include "Visual/XSharp/Core.hpp"
#include "Visual/XSharp/Core/CorePrep.hpp"
#include "Visual/XSharp/Xmm/IR.hpp"
#include "Visual/XSharp/Xpp/IR.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("renewed native pipeline starts at CorePrep")
{
    const visual_xsharp::CoreModule core{"Example", {{7, "Int", std::int64_t{42}}}};
    const auto prepared = visual_xsharp::core::prepare(core);
    const auto xpp = visual_xsharp::xpp::optimize(visual_xsharp::xpp::lower(prepared));
    const auto xmm = visual_xsharp::xmm::optimize(visual_xsharp::xmm::lower(xpp));

    REQUIRE(prepared.bindings.size() == 1);
    REQUIRE(xpp.blocks.size() == 1);
    REQUIRE(xpp.blocks.front().instructions.size() == 2);
    REQUIRE(xmm.blocks.size() == 1);
    REQUIRE(xmm.blocks.front().instructions.front().registers == std::vector<visual_xsharp::xmm::VirtualRegister>{7});
}
