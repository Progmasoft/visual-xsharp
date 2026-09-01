// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

#include <algorithm>
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "Visual/XSharp/Core/CorePrep.hpp"
#include "Visual/XSharp/Core/CorePrep/Verifier.hpp"
#include "Visual/XSharp/Core/Scalar.hpp"
#include "Visual/XSharp/Pipeline.hpp"
#include "Visual/XSharp/Xmm/IR.hpp"
#include "Visual/XSharp/Xmm/Verifier.hpp"
#include "Visual/XSharp/Xmm/Wire.hpp"
#include "Visual/XSharp/Xpp/IR.hpp"
#include "Visual/XSharp/Xpp/Verifier.hpp"
#include "Visual/XSharp/Xpp/Wire.hpp"

namespace
{
    namespace Core = ::visual_xsharp::core;
    namespace Pipeline = ::Visual::XSharp::Pipeline;
    namespace Xmm = ::visual_xsharp::xmm;
    namespace XmmWire = ::Visual::XSharp::Xmm::Wire;
    namespace Xpp = ::visual_xsharp::xpp;
    namespace XppWire = ::Visual::XSharp::Xpp::Wire;

    [[nodiscard]] auto
    Name(Core::SymbolId id, std::u32string spelling) -> Core::SymbolName
    {
        return { id, std::move(spelling) };
    }

    [[nodiscard]] auto
    Immediate(Core::Literal literal, Core::Type type) -> Core::Atom
    {
        return Core::Atom::constant(std::move(literal), std::move(type));
    }

    [[nodiscard]] auto
    Variable(Core::SymbolId id, Core::Type type) -> Core::Atom
    {
        return Core::Atom::variable(Name(id, {}), std::move(type));
    }

    [[nodiscard]] auto
    Integer(std::int64_t value, Core::Type type = Core::Type::int64()) -> Core::Atom
    {
        return Immediate(Core::integer_from_signed(value), std::move(type));
    }

    [[nodiscard]] auto
    Unit() -> Core::Atom
    {
        return Immediate(std::monostate{}, Core::Type::unit());
    }

    [[nodiscard]] auto
    Bind(Core::SymbolId id, std::u32string spelling, Core::Type type, Core::Operation operation, std::vector<Core::Atom> operands) -> Core::Instruction
    {
        Core::Instruction instruction;
        instruction.kind = Core::Instruction::Kind::Bind;
        instruction.destination = Name(id, std::move(spelling));
        instruction.type = std::move(type);
        instruction.mutable_binding = true;
        instruction.operation = operation;
        instruction.operands = std::move(operands);
        return instruction;
    }

    [[nodiscard]] auto
    ScalarModule() -> Core::CorePrepModule
    {
        // The fixture deliberately crosses several scalar payload families and
        // two blocks. Codec equality therefore covers more than header framing.
        auto byte = Bind(10U, U"byteValue", Core::Type::int8(), Core::Operation::Copy, { Integer(42, Core::Type::int8()) });
        auto wide = Bind(11U, U"wideValue", Core::Type::uint128(), Core::Operation::Copy, { Immediate(Core::IntegerLiteral{ false, { 0x80U, 0x00U, 0x00U, 0x01U } }, Core::Type::uint128()) });
        auto floating = Bind(12U, U"floatValue", Core::Type::float128(), Core::Operation::Copy, { Immediate(Core::FloatingLiteral{ "-12345.625e-7" }, Core::Type::float128()) });
        auto text = Bind(13U, U"textValue", Core::Type::string(), Core::Operation::Copy, { Immediate(std::u32string(U"ViGet · Visual X# 🐺"), Core::Type::string()) });
        auto sum = Bind(14U, U"sum", Core::Type::int8(), Core::Operation::Add, { Variable(10U, Core::Type::int8()), Integer(1, Core::Type::int8()) });
        auto condition = Bind(15U, U"continue", Core::Type::boolean(), Core::Operation::LessThan, { Variable(14U, Core::Type::int8()), Integer(100, Core::Type::int8()) });

        Core::Function main;
        main.symbol = Name(1U, U"Main");
        main.return_type = Core::Type::unit();
        main.entry = 1U;
        main.blocks = {
            Core::Block{
                1U,
                { std::move(byte), std::move(wide), std::move(floating), std::move(text), std::move(sum), std::move(condition) },
                Core::Terminator{ Core::Terminator::Kind::Branch, Variable(15U, Core::Type::boolean()), 2U, 3U },
            },
            Core::Block{
                2U,
                {},
                Core::Terminator{ Core::Terminator::Kind::Jump, Unit(), 3U, 0U },
            },
            Core::Block{
                3U,
                {},
                Core::Terminator{ Core::Terminator::Kind::Return, Unit(), 0U, 0U },
            },
        };
        return Core::CorePrepModule{ { U"Artifacts", U"Scalar" }, { std::move(main) } };
    }

    [[nodiscard]] auto
    CallModule() -> Core::CorePrepModule
    {
        Core::Function helper;
        helper.symbol = Name(20U, U"Increment");
        helper.parameters = { Core::Parameter{ Name(21U, U"value"), Core::Type::int64() } };
        helper.return_type = Core::Type::int64();
        helper.entry = 1U;
        helper.blocks = {
            Core::Block{
                1U,
                { Bind(22U, U"result", Core::Type::int64(), Core::Operation::Add, { Variable(21U, Core::Type::int64()), Integer(1) }) },
                Core::Terminator{ Core::Terminator::Kind::Return, Variable(22U, Core::Type::int64()), 0U, 0U },
            },
        };

        const auto signature = Core::Type::function({ Core::Type::int64() }, Core::Type::int64());
        auto call = Bind(31U, U"answer", Core::Type::int64(), Core::Operation::Call, { Variable(20U, signature), Integer(41) });
        Core::Function main;
        main.symbol = Name(30U, U"Main");
        main.return_type = Core::Type::unit();
        main.entry = 1U;
        main.blocks = {
            Core::Block{
                1U,
                { std::move(call) },
                Core::Terminator{ Core::Terminator::Kind::Return, Unit(), 0U, 0U },
            },
        };
        return Core::CorePrepModule{ { U"Artifacts", U"Calls" }, { std::move(helper), std::move(main) } };
    }

    [[nodiscard]] auto
    XppModule(const Core::CorePrepModule &module) -> Xpp::Module
    {
        REQUIRE(Core::verify(module).empty());
        auto lowered = Xpp::optimize(Xpp::lower(module));
        REQUIRE(::Visual::XSharp::Xpp::Verify(lowered).empty());
        return lowered;
    }

    [[nodiscard]] auto
    XmmModule(const Xpp::Module &module) -> Xmm::Module
    {
        auto lowered = Xmm::optimize(Xmm::lower(module));
        REQUIRE(::Visual::XSharp::Xmm::Verify(lowered).empty());
        return lowered;
    }

    template<typename Decode>
    void
    CheckFramingFailures(std::vector<std::uint8_t> bytes, Decode decode)
    {
        SECTION("invalid magic")
        {
            auto malformed = bytes;
            malformed[0] ^= 0xffU;
            const auto result = decode(malformed);
            REQUIRE_FALSE(result);
            REQUIRE(result.error->kind == ::Visual::XSharp::Artifact::Wire::ErrorKind::InvalidMagic);
        }
        SECTION("unsupported version")
        {
            auto malformed = bytes;
            malformed[4] = 0xffU;
            malformed[5] = 0xffU;
            const auto result = decode(malformed);
            REQUIRE_FALSE(result);
            REQUIRE(result.error->kind == ::Visual::XSharp::Artifact::Wire::ErrorKind::UnsupportedVersion);
        }
        SECTION("reserved flags")
        {
            auto malformed = bytes;
            malformed[6] = 1U;
            const auto result = decode(malformed);
            REQUIRE_FALSE(result);
            REQUIRE(result.error->kind == ::Visual::XSharp::Artifact::Wire::ErrorKind::InvalidTag);
        }
        SECTION("truncated document")
        {
            bytes.pop_back();
            const auto result = decode(bytes);
            REQUIRE_FALSE(result);
            REQUIRE(result.error->kind == ::Visual::XSharp::Artifact::Wire::ErrorKind::TruncatedInput);
        }
        SECTION("trailing input")
        {
            bytes.push_back(0U);
            const auto result = decode(bytes);
            REQUIRE_FALSE(result);
            REQUIRE(result.error->kind == ::Visual::XSharp::Artifact::Wire::ErrorKind::TrailingInput);
        }
    }
} // namespace

TEST_CASE("Xpp wire preserves scalar payloads, CFG, calls, and symbols")
{
    for (const auto &module : { ScalarModule(), CallModule() })
    {
        const auto original = XppModule(module);
        const auto encoded = XppWire::Encode(original);
        REQUIRE(encoded);
        REQUIRE(encoded.bytes.size() > 64U);
        const auto decoded = XppWire::Decode(encoded.bytes);
        REQUIRE(decoded);
        REQUIRE(*decoded.module == original);
        REQUIRE(::Visual::XSharp::Xpp::Verify(*decoded.module).empty());
    }
}

TEST_CASE("Xmm wire preserves virtual-register ABI and typed immediates")
{
    for (const auto &module : { ScalarModule(), CallModule() })
    {
        const auto original = XmmModule(XppModule(module));
        const auto encoded = XmmWire::Encode(original);
        REQUIRE(encoded);
        REQUIRE(encoded.bytes.size() > 64U);
        const auto decoded = XmmWire::Decode(encoded.bytes);
        REQUIRE(decoded);
        REQUIRE(*decoded.module == original);
        REQUIRE(::Visual::XSharp::Xmm::Verify(*decoded.module).empty());
    }
}

TEST_CASE("Xpp wire rejects malformed framing without constructing a module")
{
    const auto encoded = XppWire::Encode(XppModule(ScalarModule()));
    REQUIRE(encoded);
    CheckFramingFailures(encoded.bytes, [](const auto &bytes) {
        return XppWire::Decode(bytes);
    });
}

TEST_CASE("Xmm wire rejects malformed framing without constructing a module")
{
    const auto encoded = XmmWire::Encode(XmmModule(XppModule(ScalarModule())));
    REQUIRE(encoded);
    CheckFramingFailures(encoded.bytes, [](const auto &bytes) {
        return XmmWire::Decode(bytes);
    });
}

TEST_CASE("artifact wire applies configured collection and byte limits")
{
    const auto xpp = XppModule(ScalarModule());
    XppWire::Limits tiny;
    tiny.maximumWireBytes = 16U;
    const auto encode = XppWire::Encode(xpp, tiny);
    REQUIRE_FALSE(encode);
    REQUIRE(encode.error->kind == ::Visual::XSharp::Artifact::Wire::ErrorKind::LimitExceeded);

    const auto normal = XppWire::Encode(xpp);
    REQUIRE(normal);
    const auto decode = XppWire::Decode(normal.bytes, tiny);
    REQUIRE_FALSE(decode);
    REQUIRE(decode.error->kind == ::Visual::XSharp::Artifact::Wire::ErrorKind::LimitExceeded);
}

TEST_CASE("artifact encoders reject unverified stage models")
{
    auto xpp = XppModule(ScalarModule());
    xpp.functions.front().symbol.id = 0U;
    const auto xppResult = XppWire::Encode(xpp);
    REQUIRE_FALSE(xppResult);
    REQUIRE(xppResult.error->kind == ::Visual::XSharp::Artifact::Wire::ErrorKind::InvalidModel);

    auto xmm = XmmModule(XppModule(ScalarModule()));
    xmm.functions.front().entry = 999U;
    const auto xmmResult = XmmWire::Encode(xmm);
    REQUIRE_FALSE(xmmResult);
    REQUIRE(xmmResult.error->kind == ::Visual::XSharp::Artifact::Wire::ErrorKind::InvalidModel);
}

TEST_CASE("pipeline resumes from verified Xpp and Xmm documents")
{
    const auto xpp = XppModule(CallModule());
    const auto xppBytes = XppWire::Encode(xpp);
    REQUIRE(xppBytes);
    Pipeline::Options stopAtXmm;
    stopAtXmm.stop_after = Pipeline::Stop::Xmm;
    const auto fromXpp = Pipeline::ConsumeXpp(xppBytes.bytes, stopAtXmm);
    REQUIRE(fromXpp);
    REQUIRE(fromXpp.xpp.has_value());
    REQUIRE(fromXpp.xmm.has_value());
    REQUIRE_FALSE(fromXpp.llvm.has_value());

    const auto xmmBytes = XmmWire::Encode(*fromXpp.xmm);
    REQUIRE(xmmBytes);
    const auto fromXmm = Pipeline::ConsumeXmm(xmmBytes.bytes, stopAtXmm);
    REQUIRE(fromXmm);
    REQUIRE_FALSE(fromXmm.xpp.has_value());
    REQUIRE(fromXmm.xmm.has_value());
    REQUIRE_FALSE(fromXmm.llvm.has_value());
}

TEST_CASE("pipeline reports stage-specific wire failures")
{
    const std::array<std::uint8_t, 8U> badXpp{ 'V', 'X', 'P', 'P', 0xffU, 0xffU, 0U, 0U };
    const auto xpp = Pipeline::ConsumeXpp(badXpp);
    REQUIRE_FALSE(xpp);
    REQUIRE(xpp.xppWireError.has_value());
    REQUIRE_FALSE(xpp.xpp.has_value());

    const std::array<std::uint8_t, 8U> badXmm{ 'V', 'X', 'M', 'M', 0xffU, 0xffU, 0U, 0U };
    const auto xmm = Pipeline::ConsumeXmm(badXmm);
    REQUIRE_FALSE(xmm);
    REQUIRE(xmm.xmmWireError.has_value());
    REQUIRE_FALSE(xmm.xmm.has_value());
}

TEST_CASE("artifact encoders are deterministic for identical verified models")
{
    const auto xpp = XppModule(CallModule());
    const auto firstXpp = XppWire::Encode(xpp);
    const auto secondXpp = XppWire::Encode(xpp);
    REQUIRE(firstXpp);
    REQUIRE(secondXpp);
    REQUIRE(firstXpp.bytes == secondXpp.bytes);

    const auto xmm = XmmModule(xpp);
    const auto firstXmm = XmmWire::Encode(xmm);
    const auto secondXmm = XmmWire::Encode(xmm);
    REQUIRE(firstXmm);
    REQUIRE(secondXmm);
    REQUIRE(firstXmm.bytes == secondXmm.bytes);
}

TEST_CASE("pipeline applies the configured artifact limits before stage lowering")
{
    const auto encoded = XppWire::Encode(XppModule(ScalarModule()));
    REQUIRE(encoded);

    Pipeline::Options options;
    options.stop_after = Pipeline::Stop::Xpp;
    options.artifactWireLimits.maximumWireBytes = encoded.bytes.size() - 1U;
    const auto result = Pipeline::ConsumeXpp(encoded.bytes, options);
    REQUIRE_FALSE(result);
    REQUIRE(result.xppWireError.has_value());
    REQUIRE(result.xppWireError->kind == ::Visual::XSharp::Artifact::Wire::ErrorKind::LimitExceeded);
    REQUIRE_FALSE(result.xpp.has_value());
}

TEST_CASE("pipeline optimizes a loaded Xpp document before the requested boundary")
{
    auto module = XppModule(ScalarModule());
    const auto &existing = module.functions.front().blocks.front().instructions.front();
    auto selfCopy = existing;
    selfCopy.effect = Xpp::Instruction::Effect::Store;
    selfCopy.opcode = Xpp::Opcode::Copy;
    selfCopy.operands = { Xpp::Operand{
        Xpp::Operand::Kind::Symbol,
        selfCopy.result_type,
        selfCopy.destination,
        {},
    } };
    module.functions.front().blocks.front().instructions.push_back(std::move(selfCopy));
    REQUIRE(::Visual::XSharp::Xpp::Verify(module).empty());
    const auto instructionCount = module.functions.front().blocks.front().instructions.size();
    const auto encoded = XppWire::Encode(module);
    REQUIRE(encoded);

    Pipeline::Options options;
    options.stop_after = Pipeline::Stop::Xpp;
    options.optimize_xpp = true;
    const auto optimized = Pipeline::ConsumeXpp(encoded.bytes, options);
    REQUIRE(optimized);
    REQUIRE(optimized.xpp->functions.front().blocks.front().instructions.size() == instructionCount - 1U);

    options.optimize_xpp = false;
    const auto retained = Pipeline::ConsumeXpp(encoded.bytes, options);
    REQUIRE(retained);
    REQUIRE(retained.xpp->functions.front().blocks.front().instructions.size() == instructionCount);
}

TEST_CASE("pipeline optimizes a loaded Xmm document before the requested boundary")
{
    auto module = XmmModule(XppModule(ScalarModule()));
    const auto &existing = module.functions.front().blocks.front().instructions.front();
    auto selfMove = existing;
    selfMove.opcode = Xmm::Opcode::Move;
    selfMove.has_result = true;
    selfMove.operands = { Xmm::Value{
        Xmm::Value::Kind::Register,
        selfMove.result_type,
        selfMove.destination,
        0U,
        {},
    } };
    module.functions.front().blocks.front().instructions.push_back(std::move(selfMove));
    REQUIRE(::Visual::XSharp::Xmm::Verify(module).empty());
    const auto instructionCount = module.functions.front().blocks.front().instructions.size();
    const auto encoded = XmmWire::Encode(module);
    REQUIRE(encoded);

    Pipeline::Options options;
    options.stop_after = Pipeline::Stop::Xmm;
    options.optimize_xmm = true;
    const auto optimized = Pipeline::ConsumeXmm(encoded.bytes, options);
    REQUIRE(optimized);
    REQUIRE(optimized.xmm->functions.front().blocks.front().instructions.size() == instructionCount - 1U);

    options.optimize_xmm = false;
    const auto retained = Pipeline::ConsumeXmm(encoded.bytes, options);
    REQUIRE(retained);
    REQUIRE(retained.xmm->functions.front().blocks.front().instructions.size() == instructionCount);
}

TEST_CASE("pipeline rejects a backward Xmm to Xpp boundary request")
{
    const auto encoded = XmmWire::Encode(XmmModule(XppModule(CallModule())));
    REQUIRE(encoded);
    Pipeline::Options options;
    options.stop_after = Pipeline::Stop::Xpp;
    const auto result = Pipeline::ConsumeXmm(encoded.bytes, options);
    REQUIRE_FALSE(result);
    REQUIRE(result.xmmWireError.has_value());
    REQUIRE(result.xmmWireError->kind == ::Visual::XSharp::Artifact::Wire::ErrorKind::InvalidModel);
    REQUIRE(result.xmmWireError->context == "pipeline boundary");
}
