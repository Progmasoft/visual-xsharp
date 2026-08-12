// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

#include "Visual/XSharp/Core/CorePrep.hpp"
#include "Visual/XSharp/Core/CorePrep/Verifier.hpp"
#include "Visual/XSharp/Core/CorePrep/Wire.hpp"
#include "Visual/XSharp/Xmm/IR.hpp"
#include "Visual/XSharp/Xpp/IR.hpp"

#include <catch2/catch_test_macros.hpp>

namespace
{
auto prepared_module() -> visual_xsharp::core::CorePrepModule
{
    using namespace visual_xsharp::core;
    const auto int_constant = [](std::int64_t value) { return Atom::constant(value, Type::int64()); };
    const auto variable = [](SymbolId symbol, Type type) { return Atom::variable(symbol, std::move(type)); };

    Function sum{{10, U"Sum"}, {{{11, U"left"}, Type::int64()}, {{12, U"right"}, Type::int64()}}, Type::int64(), 0,
                 {{0,
                   {{Instruction::Kind::Bind, {13, U"result"}, Type::int64(), false, Operation::Add,
                     {variable(11, Type::int64()), variable(12, Type::int64())}}},
                   {Terminator::Kind::Return, variable(13, Type::int64()), 0, 0}}}};

    Function main{{20, U"Main"}, {}, Type::unit(), 0,
                  {{0,
                    {{Instruction::Kind::Bind, {21, U"value"}, Type::int64(), true, Operation::Call,
                      {variable(10, Type::function({Type::int64(), Type::int64()}, Type::int64())), int_constant(20), int_constant(22)}},
                     {Instruction::Kind::Bind, {22, U"condition"}, Type::boolean(), false, Operation::GreaterEqual,
                      {variable(21, Type::int64()), int_constant(40)}}},
                    {Terminator::Kind::Branch, variable(22, Type::boolean()), 1, 2}},
                   {1,
                    {{Instruction::Kind::Bind, {23, U"$coreprep23"}, Type::int64(), false, Operation::Add,
                      {variable(21, Type::int64()), int_constant(1)}},
                     {Instruction::Kind::Assign, {21, U"value"}, Type::int64(), false, Operation::Copy,
                      {variable(23, Type::int64())}}},
                    {Terminator::Kind::Jump, {}, 3, 0}},
                   {2,
                    {{Instruction::Kind::Assign, {21, U"value"}, Type::int64(), false, Operation::Copy, {int_constant(0)}}},
                    {Terminator::Kind::Jump, {}, 3, 0}},
                   {3, {}, {Terminator::Kind::Return, Atom::constant({}, Type::unit()), 0, 0}},
                   {99, {}, {Terminator::Kind::Unreachable, {}, 0, 0}}}};
    return CorePrepModule{{U"Name"}, {sum, main}};
}
} // namespace

TEST_CASE("C++20 pipeline consumes Haskell-owned CorePrep")
{
    const auto prepared = prepared_module();
    REQUIRE(visual_xsharp::core::verify(prepared).empty());
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

TEST_CASE("CorePrep verifier rejects malformed control flow before Xpp lowering")
{
    auto prepared = prepared_module();
    prepared.functions.at(1).blocks.front().terminator.true_target = 404;
    prepared.functions.at(1).blocks.front().instructions.front().operands.clear();

    const auto issues = visual_xsharp::core::verify(prepared);
    REQUIRE(issues.size() == 2);
    REQUIRE(issues.at(0).code == "VXC1008");
    REQUIRE(issues.at(1).code == "VXC1012");
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

TEST_CASE("C++20 CorePrep wire codec preserves the complete typed CFG")
{
    const auto prepared = prepared_module();
    const auto encoded = visual_xsharp::core::wire::encode(prepared);
    REQUIRE(encoded);
    REQUIRE(encoded.bytes.size() > 8);
    REQUIRE(encoded.bytes.at(0) == 'V');
    REQUIRE(encoded.bytes.at(1) == 'X');
    REQUIRE(encoded.bytes.at(2) == 'C');
    REQUIRE(encoded.bytes.at(3) == 'P');

    const auto decoded = visual_xsharp::core::wire::decode(encoded.bytes);
    REQUIRE(decoded);
    REQUIRE(decoded.module.value() == prepared);
    REQUIRE(visual_xsharp::core::verify(decoded.module.value()).empty());
}

TEST_CASE("CorePrep wire decoder rejects malformed document boundaries")
{
    const auto encoded = visual_xsharp::core::wire::encode(prepared_module());
    REQUIRE(encoded);

    SECTION("invalid magic")
    {
        auto bytes = encoded.bytes;
        bytes.front() = 'N';
        const auto decoded = visual_xsharp::core::wire::decode(bytes);
        REQUIRE_FALSE(decoded);
        REQUIRE(decoded.error->kind == visual_xsharp::core::wire::ErrorKind::InvalidMagic);
    }
    SECTION("truncation")
    {
        auto bytes = encoded.bytes;
        bytes.pop_back();
        const auto decoded = visual_xsharp::core::wire::decode(bytes);
        REQUIRE_FALSE(decoded);
        REQUIRE(decoded.error->kind == visual_xsharp::core::wire::ErrorKind::TruncatedInput);
    }
    SECTION("trailing data")
    {
        auto bytes = encoded.bytes;
        bytes.push_back(0xff);
        const auto decoded = visual_xsharp::core::wire::decode(bytes);
        REQUIRE_FALSE(decoded);
        REQUIRE(decoded.error->kind == visual_xsharp::core::wire::ErrorKind::TrailingInput);
    }
    SECTION("unsupported version")
    {
        auto bytes = encoded.bytes;
        bytes.at(4) = 2;
        const auto decoded = visual_xsharp::core::wire::decode(bytes);
        REQUIRE_FALSE(decoded);
        REQUIRE(decoded.error->kind == visual_xsharp::core::wire::ErrorKind::UnsupportedVersion);
    }
}

TEST_CASE("CorePrep wire codec enforces resource and Unicode limits")
{
    using namespace visual_xsharp::core;
    auto prepared = prepared_module();

    SECTION("function count")
    {
        visual_xsharp::core::wire::Limits limits;
        limits.maximum_functions = 1;
        const auto encoded = visual_xsharp::core::wire::encode(prepared, limits);
        REQUIRE_FALSE(encoded);
        REQUIRE(encoded.error->kind == visual_xsharp::core::wire::ErrorKind::LimitExceeded);
    }
    SECTION("invalid Unicode scalar")
    {
        prepared.name = {std::u32string(1, static_cast<char32_t>(0xd800))};
        const auto encoded = visual_xsharp::core::wire::encode(prepared);
        REQUIRE_FALSE(encoded);
        REQUIRE(encoded.error->kind == visual_xsharp::core::wire::ErrorKind::InvalidCodePoint);
    }
    SECTION("zero symbol")
    {
        prepared.functions.front().symbol.id = 0;
        const auto encoded = visual_xsharp::core::wire::encode(prepared);
        REQUIRE_FALSE(encoded);
        REQUIRE(encoded.error->kind == visual_xsharp::core::wire::ErrorKind::InvalidSymbol);
    }
}
