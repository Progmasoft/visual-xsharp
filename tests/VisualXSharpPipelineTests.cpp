// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <fstream>
#include <ranges>
#include <sstream>
#include <string_view>

#include "Visual/XSharp/Core/CorePrep.hpp"
#include "Visual/XSharp/Core/CorePrep/Verifier.hpp"
#include "Visual/XSharp/Core/CorePrep/Wire.hpp"
#include "Visual/XSharp/Pipeline.hpp"
#include "Visual/XSharp/Xmm/IR.hpp"
#include "Visual/XSharp/Xpp/IR.hpp"
#include "Visual/XSharp/Xpp/Verifier.hpp"

namespace
{
    using visual_xsharp::core::Atom;
    using visual_xsharp::core::CorePrepModule;
    using visual_xsharp::core::Function;
    using visual_xsharp::core::Instruction;
    using visual_xsharp::core::Operation;
    using visual_xsharp::core::SymbolId;
    using visual_xsharp::core::Terminator;
    using visual_xsharp::core::Type;

    auto
    prepared_module() -> visual_xsharp::core::CorePrepModule
    {
        const auto int_constant = [](std::int64_t value) {
            return Atom::constant(value, Type::int64());
        };
        const auto variable = [](SymbolId symbol, Type type) {
            return Atom::variable(symbol, std::move(type));
        };

        Function sum{ { 10, U"Sum" },
                      { { { 11, U"left" }, Type::int64() }, { { 12, U"right" }, Type::int64() } },
                      Type::int64(),
                      0,
                      { { 0,
                          { { Instruction::Kind::Bind,
                              { 13, U"result" },
                              Type::int64(),
                              false,
                              Operation::Add,
                              { variable(11, Type::int64()), variable(12, Type::int64()) } } },
                          { Terminator::Kind::Return, variable(13, Type::int64()), 0, 0 } } } };

        Function main{
            { 20, U"Main" },
            {},
            Type::unit(),
            0,
            { { 0,
                { { Instruction::Kind::Bind,
                    { 21, U"value" },
                    Type::int64(),
                    true,
                    Operation::Call,
                    { variable(10, Type::function({ Type::int64(), Type::int64() }, Type::int64())), int_constant(20), int_constant(22) } },
                  { Instruction::Kind::Bind,
                    { 22, U"condition" },
                    Type::boolean(),
                    false,
                    Operation::GreaterEqual,
                    { variable(21, Type::int64()), int_constant(40) } } },
                { Terminator::Kind::Branch, variable(22, Type::boolean()), 1, 2 } },
              { 1,
                { { Instruction::Kind::Bind,
                    { 23, U"$coreprep23" },
                    Type::int64(),
                    false,
                    Operation::Add,
                    { variable(21, Type::int64()), int_constant(1) } },
                  { Instruction::Kind::Assign,
                    { 21, U"value" },
                    Type::int64(),
                    false,
                    Operation::Copy,
                    { variable(23, Type::int64()) } } },
                { Terminator::Kind::Jump, {}, 3, 0 } },
              { 2,
                { { Instruction::Kind::Assign, { 21, U"value" }, Type::int64(), false, Operation::Copy, { int_constant(0) } } },
                { Terminator::Kind::Jump, {}, 3, 0 } },
              { 3, {}, { Terminator::Kind::Return, Atom::constant({}, Type::unit()), 0, 0 } },
              { 99, {}, { Terminator::Kind::Unreachable, {}, 0, 0 } } }
        };
        return CorePrepModule{ { U"Name" }, { sum, main } };
    }

    auto
    golden_module() -> visual_xsharp::core::CorePrepModule
    {
        Function main{ { 1, U"Main" },
                       {},
                       Type::unit(),
                       0,
                       { { 0, {}, { Terminator::Kind::Return, Atom::constant({}, Type::unit()), 0, 0 } } } };
        return CorePrepModule{ { U"Demo" }, { std::move(main) } };
    }

    auto
    read_golden_hex() -> std::vector<std::uint8_t>
    {
        std::ifstream stream(XS_COREPREP_GOLDEN_PATH);
        REQUIRE(stream);
        std::vector<std::uint8_t> bytes;
        std::string line;
        while (std::getline(stream, line))
        {
            if (const auto comment = line.find('#'); comment != std::string::npos)
                line.erase(comment);
            std::istringstream tokens(line);
            std::string token;
            while (tokens >> token)
                bytes.push_back(static_cast<std::uint8_t>(std::stoul(token, nullptr, 16)));
        }
        return bytes;
    }

    auto
    has_issue(const std::vector<visual_xsharp::core::VerificationIssue> &issues, std::string_view code) -> bool
    {
        return std::ranges::any_of(issues, [code](const auto &issue) {
            return issue.code == code;
        });
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
    REQUIRE(has_issue(issues, "VXC1008"));
    REQUIRE(has_issue(issues, "VXC1012"));
    REQUIRE(has_issue(issues, "VXC1024"));
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

TEST_CASE("Xpp verifier rejects optimizer output that breaks storage and control flow")
{
    auto xpp = visual_xsharp::xpp::lower(prepared_module());
    auto &main = xpp.functions.at(1);
    main.blocks.front().instructions.front().effect = visual_xsharp::xpp::Instruction::Effect::Store;
    main.blocks.front().instructions.front().destination = 404U;
    main.blocks.front().terminator.true_target = 405U;

    const auto issues = Visual::XSharp::Xpp::Verify(xpp);
    REQUIRE(std::ranges::any_of(issues, [](const auto &issue) {
        return issue.code == "VXP1018";
    }));
    REQUIRE(std::ranges::any_of(issues, [](const auto &issue) {
        return issue.code == "VXP1022";
    }));
}

TEST_CASE("Xpp verifier accepts the complete lowered and optimized contract")
{
    const auto lowered = visual_xsharp::xpp::lower(prepared_module());
    REQUIRE(Visual::XSharp::Xpp::Verify(lowered).empty());
    REQUIRE(Visual::XSharp::Xpp::Verify(visual_xsharp::xpp::optimize(lowered)).empty());
}

TEST_CASE("Xpp verifier reports declaration and operand corruption independently")
{
    auto xpp = visual_xsharp::xpp::lower(prepared_module());
    auto &sum = xpp.functions.front();
    sum.parameters.back().symbol.id = sum.parameters.front().symbol.id;
    sum.blocks.front().instructions.front().operands.front().symbol = 900U;

    const auto issues = Visual::XSharp::Xpp::Verify(xpp);
    REQUIRE(std::ranges::any_of(issues, [](const auto &issue) {
        return issue.code == "VXP1006";
    }));
    REQUIRE(std::ranges::any_of(issues, [](const auto &issue) {
        return issue.code == "VXP1013";
    }));
    REQUIRE(std::ranges::all_of(issues, [](const auto &issue) {
        return issue.function == 10U;
    }));
}

TEST_CASE("Xpp verifier validates direct-call signatures before register lowering")
{
    auto xpp = visual_xsharp::xpp::lower(prepared_module());
    xpp.functions.at(1).blocks.front().instructions.front().operands.pop_back();

    const auto issues = Visual::XSharp::Xpp::Verify(xpp);
    REQUIRE(std::ranges::any_of(issues, [](const auto &issue) {
        return issue.code == "VXP1025";
    }));
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
        prepared.name = { std::u32string(1, static_cast<char32_t>(0xd800)) };
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

TEST_CASE("RAM pipeline decodes verifies and lowers CorePrep to optimized Xmm")
{
    const auto encoded = visual_xsharp::core::wire::encode(prepared_module());
    REQUIRE(encoded);

    const auto result = visual_xsharp::consume_coreprep(encoded.bytes);
    REQUIRE(result);
    REQUIRE(result.core_prep.has_value());
    REQUIRE(result.xpp.has_value());
    REQUIRE(result.xmm.has_value());
    REQUIRE(result.xmmVerificationIssues.empty());
    REQUIRE(result.llvm.has_value());
    REQUIRE_FALSE(result.wire_error.has_value());
    REQUIRE(result.verification_issues.empty());
    REQUIRE(result.xppVerificationIssues.empty());
    REQUIRE(result.core_prep->functions.at(1).blocks.size() == 5);
    REQUIRE(result.xpp->functions.at(1).blocks.size() == 4);
    REQUIRE(result.xmm->functions.at(1).blocks.size() == 4);
    REQUIRE_FALSE(result.llvm->bitcode.empty());
}

TEST_CASE("RAM pipeline keeps optimization choices explicit")
{
    const auto encoded = visual_xsharp::core::wire::encode(prepared_module());
    REQUIRE(encoded);
    visual_xsharp::PipelineOptions options;
    options.optimize_xpp = false;
    options.optimize_xmm = false;

    const auto result = visual_xsharp::consume_coreprep(encoded.bytes, options);
    REQUIRE(result);
    REQUIRE(result.xpp->functions.at(1).blocks.size() == 5);
    REQUIRE(result.xmm->functions.at(1).blocks.size() == 5);
}

TEST_CASE("RAM pipeline never lowers malformed wire input")
{
    const std::vector<std::uint8_t> malformed{ 'N', 'O', 'P', 'E' };
    const auto result = visual_xsharp::consume_coreprep(malformed);
    REQUIRE_FALSE(result);
    REQUIRE(result.wire_error.has_value());
    REQUIRE(result.wire_error->kind == visual_xsharp::core::wire::ErrorKind::InvalidMagic);
    REQUIRE_FALSE(result.core_prep.has_value());
    REQUIRE_FALSE(result.xpp.has_value());
    REQUIRE_FALSE(result.xmm.has_value());
    REQUIRE_FALSE(result.llvm.has_value());
}

TEST_CASE("RAM pipeline never lowers semantically invalid CorePrep")
{
    auto prepared = prepared_module();
    prepared.functions.at(1).blocks.at(1).instructions.back().destination = { 22, U"condition" };
    const auto encoded = visual_xsharp::core::wire::encode(prepared);
    REQUIRE(encoded);

    const auto result = visual_xsharp::consume_coreprep(encoded.bytes);
    REQUIRE_FALSE(result);
    REQUIRE(result.core_prep.has_value());
    REQUIRE(has_issue(result.verification_issues, "VXC1036"));
    REQUIRE_FALSE(result.xpp.has_value());
    REQUIRE_FALSE(result.xmm.has_value());
    REQUIRE_FALSE(result.llvm.has_value());
}

TEST_CASE("semantic verifier rejects mismatched calls assignments and returns")
{
    auto prepared = prepared_module();
    auto &main = prepared.functions.at(1);
    main.blocks.at(0).instructions.at(0).operands.at(1).type = visual_xsharp::core::Type::boolean();
    main.blocks.at(2).instructions.at(0).operands.front().type = visual_xsharp::core::Type::boolean();
    main.blocks.at(3).terminator.value = visual_xsharp::core::Atom::constant(std::int64_t{ 1 }, visual_xsharp::core::Type::int64());

    const auto issues = visual_xsharp::core::verify(prepared);
    REQUIRE(has_issue(issues, "VXC1027"));
    REQUIRE(has_issue(issues, "VXC1037"));
    REQUIRE(has_issue(issues, "VXC1038"));
}

TEST_CASE("semantic verifier rejects inconsistent symbol spelling")
{
    auto prepared = prepared_module();
    prepared.functions.at(0).blocks.at(0).instructions.at(0).operands.at(0).symbol.spelling = U"different";
    const auto issues = visual_xsharp::core::verify(prepared);
    REQUIRE(has_issue(issues, "VXC1014"));
}

TEST_CASE("shared wire-v2 golden document decodes to the canonical CorePrep module")
{
    const auto golden = read_golden_hex();
    const auto decoded = visual_xsharp::core::wire::decode(golden);
    REQUIRE(decoded);
    REQUIRE(decoded.module.value() == golden_module());

    const auto encoded = visual_xsharp::core::wire::encode(golden_module());
    REQUIRE(encoded);
    REQUIRE(encoded.bytes == golden);
}
