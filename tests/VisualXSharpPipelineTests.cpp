// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

#include "Visual/XSharp/Core/CorePrep.hpp"
#include "Visual/XSharp/Xmm/IR.hpp"
#include "Visual/XSharp/Xpp/IR.hpp"

#include <catch2/catch_test_macros.hpp>

namespace
{
auto prepared_module() -> visual_xsharp::core::CorePrepModule
{
    using namespace visual_xsharp::core;
    const auto int_constant = [](std::int64_t value) { return Atom::constant(value, ValueType::Int64); };
    const auto variable = [](SymbolId symbol, ValueType type) { return Atom::variable(symbol, type); };

    Function sum{10, {{11, ValueType::Int64}, {12, ValueType::Int64}}, ValueType::Int64, 0,
                 {{0,
                   {{Instruction::Kind::Bind, 13, ValueType::Int64, false, Operation::Add,
                     {variable(11, ValueType::Int64), variable(12, ValueType::Int64)}}},
                   {Terminator::Kind::Return, variable(13, ValueType::Int64), 0, 0}}}};

    Function main{20, {}, ValueType::Unit, 0,
                  {{0,
                    {{Instruction::Kind::Bind, 21, ValueType::Int64, true, Operation::Call,
                      {variable(10, ValueType::Int64), int_constant(20), int_constant(22)}},
                     {Instruction::Kind::Bind, 22, ValueType::Bool, false, Operation::GreaterEqual,
                      {variable(21, ValueType::Int64), int_constant(40)}}},
                    {Terminator::Kind::Branch, variable(22, ValueType::Bool), 1, 2}},
                   {1,
                    {{Instruction::Kind::Assign, 21, ValueType::Int64, true, Operation::Add,
                      {variable(21, ValueType::Int64), int_constant(1)}}},
                    {Terminator::Kind::Jump, {}, 3, 0}},
                   {2,
                    {{Instruction::Kind::Assign, 21, ValueType::Int64, true, Operation::Copy, {int_constant(0)}}},
                    {Terminator::Kind::Jump, {}, 3, 0}},
                   {3, {}, {Terminator::Kind::Return, Atom::constant({}, ValueType::Unit), 0, 0}},
                   {99, {}, {Terminator::Kind::Unreachable, {}, 0, 0}}}};
    return CorePrepModule{"Name", {sum, main}};
}
} // namespace

TEST_CASE("C++20 pipeline consumes Haskell-owned CorePrep")
{
    const auto prepared = prepared_module();
    const auto lowered_xpp = visual_xsharp::xpp::lower(prepared);
    const auto xpp = visual_xsharp::xpp::optimize(lowered_xpp);
    const auto xmm = visual_xsharp::xmm::optimize(visual_xsharp::xmm::lower(xpp));

    REQUIRE(prepared.functions.size() == 2);
    REQUIRE(lowered_xpp.functions.at(1).blocks.size() == 5);
    REQUIRE(xpp.functions.at(1).blocks.size() == 4);
    REQUIRE(xpp.functions.at(1).blocks.front().terminator.kind == visual_xsharp::xpp::Terminator::Kind::Branch);
    REQUIRE(xmm.functions.size() == 2);
    REQUIRE(xmm.functions.at(1).blocks.size() == 4);
    REQUIRE(xmm.functions.at(1).blocks.front().instructions.at(0).opcode == visual_xsharp::xmm::Opcode::Call);
    REQUIRE(xmm.functions.at(1).blocks.front().instructions.at(1).opcode == visual_xsharp::xmm::Opcode::CompareGreaterEqualI64);
}

TEST_CASE("typed native lowering allocates stable virtual registers")
{
    const auto xmm = visual_xsharp::xmm::lower(visual_xsharp::xpp::lower(prepared_module()));
    const auto &sum = xmm.functions.front();
    REQUIRE(sum.parameter_registers.size() == 2);
    REQUIRE(sum.parameter_registers[0] != sum.parameter_registers[1]);
    REQUIRE(sum.blocks.front().instructions.front().destination != 0);
    REQUIRE(sum.blocks.front().terminator.value.kind == visual_xsharp::xmm::Value::Kind::Register);
}
