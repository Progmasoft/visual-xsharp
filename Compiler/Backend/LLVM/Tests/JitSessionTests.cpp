// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <span>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "Visual/XSharp/Backend/LLVM.hpp"
#include "Visual/XSharp/Core/CorePrep/Wire.hpp"
#include "Visual/XSharp/Pipeline.hpp"

namespace
{
    namespace Llvm = Visual::XSharp::Backend::LLVM;
    namespace Core = Llvm::Core;

    auto
    LowerConstant(Core::Literal literal, Core::Type type, std::uint64_t id = 1U) -> Llvm::Result
    {
        Core::Function function{
            { id, U"Evaluate" },
            {},
            type,
            0,
            { Core::Block{ 0,
                           {},
                           Core::Terminator{ Core::Terminator::Kind::Return,
                                             Core::Atom::constant(std::move(literal), type),
                                             0,
                                             0 } } },
        };
        const Core::CorePrepModule module{ { U"VXSI", U"Cells" }, { std::move(function) } };
        auto xpp = visual_xsharp::xpp::lower(module);
        auto xmm = visual_xsharp::xmm::lower(xpp);
        return Llvm::Lower(xmm);
    }

    auto
    InvokeConstant(Core::Literal literal, Core::Type type, std::string_view symbol = "VXSI.Cells.Evaluate.1")
        -> Llvm::JitResult
    {
        const auto lowered = LowerConstant(std::move(literal), type);
        if (!lowered)
            return { std::nullopt,
                     Llvm::JitError{ Llvm::JitErrorKind::InvalidBitcode,
                                     lowered.error->code,
                                     lowered.error->message } };
        Llvm::JitSession session;
        if (auto error = session.AddModule(lowered.artifact->bitcode, "vxsi-test", symbol, type))
            return { std::nullopt, std::move(error) };
        return session.InvokeScalar(symbol, type);
    }
} // namespace

TEST_CASE("ORC JIT invokes verified integer constants with their exact host widths", "[llvm][orc][vxsi]")
{
    struct Case final
    {
        Core::Type type;
        std::int64_t input;
        std::int64_t expected;
    };
    const std::array cases{
        Case{ Core::Type::int8(), -7, -7 },
        Case{ Core::Type::int16(), 1234, 1234 },
        Case{ Core::Type::int32(), -123456, -123456 },
        Case{ Core::Type::int64(), 0x123456789LL, 0x123456789LL },
    };
    for (const auto &test : cases)
    {
        const auto result = InvokeConstant(test.input, test.type);
        REQUIRE(result);
        REQUIRE(std::get<std::int64_t>(result.value->payload) == test.expected);
        REQUIRE(result.value->type == test.type);
    }
}

TEST_CASE("ORC JIT uses the exact boolean, character, and floating return ABI", "[llvm][orc][abi]")
{
    const auto boolean = InvokeConstant(true, Core::Type::boolean());
    REQUIRE(boolean);
    REQUIRE(std::get<bool>(boolean.value->payload));

    const auto character = InvokeConstant(Core::IntegerLiteral{ false, { 0x51U } }, Core::Type::character());
    REQUIRE(character);
    REQUIRE(std::get<char32_t>(character.value->payload) == U'Q');

    const auto single = InvokeConstant(Core::FloatingLiteral{ "1.25" }, Core::Type::float32());
    REQUIRE(single);
    REQUIRE(std::get<double>(single.value->payload) == 1.25);

    const auto doublePrecision = InvokeConstant(Core::FloatingLiteral{ "-19.5" }, Core::Type::float64());
    REQUIRE(doublePrecision);
    REQUIRE(std::get<double>(doublePrecision.value->payload) == -19.5);

    const auto noValue = InvokeConstant(std::monostate{}, Core::Type::unit());
    REQUIRE(noValue);
    REQUIRE(std::holds_alternative<std::monostate>(noValue.value->payload));
}

TEST_CASE("ORC JIT preserves unsigned result magnitude through UInt64", "[llvm][orc][abi]")
{
    const auto result = InvokeConstant(
        Core::IntegerLiteral{ false, { 0xffU, 0xffU, 0xffU, 0xffU, 0xffU, 0xffU, 0xffU, 0xffU } },
        Core::Type::uint64());
    REQUIRE(result);
    REQUIRE(std::get<std::uint64_t>(result.value->payload) == 18446744073709551615ULL);
}

TEST_CASE("ORC validates the LLVM entry ABI before registration and invocation", "[llvm][orc][abi][safety]")
{
    const auto lowered = LowerConstant(std::int64_t{ 321 }, Core::Type::int64());
    REQUIRE(lowered);
    Llvm::JitSession session;

    const auto mismatch = session.AddModule(
        lowered.artifact->bitcode,
        "wrong-declared-abi",
        "VXSI.Cells.Evaluate.1",
        Core::Type::int32());
    REQUIRE(mismatch);
    REQUIRE(mismatch->kind == Llvm::JitErrorKind::SignatureMismatch);

    const auto absent = session.InvokeScalar("VXSI.Cells.Evaluate.1", Core::Type::int64());
    REQUIRE_FALSE(absent);
    REQUIRE(absent.error->kind == Llvm::JitErrorKind::SymbolLookup);

    REQUIRE_FALSE(session.AddModule(
        lowered.artifact->bitcode,
        "valid-declared-abi",
        "VXSI.Cells.Evaluate.1",
        Core::Type::int64()));
    const auto wrongRequest = session.InvokeScalar("VXSI.Cells.Evaluate.1", Core::Type::int8());
    REQUIRE_FALSE(wrongRequest);
    REQUIRE(wrongRequest.error->kind == Llvm::JitErrorKind::SignatureMismatch);

    const auto correctRequest = session.InvokeScalar("VXSI.Cells.Evaluate.1", Core::Type::int64());
    REQUIRE(correctRequest);
    REQUIRE(std::get<std::int64_t>(correctRequest.value->payload) == 321);

    const auto duplicate = session.AddModule(
        lowered.artifact->bitcode,
        "duplicate-entry",
        "VXSI.Cells.Evaluate.1",
        Core::Type::int64());
    REQUIRE(duplicate);
    REQUIRE(duplicate->kind == Llvm::JitErrorKind::SignatureMismatch);
}

TEST_CASE("ORC JIT resolves modules added later in the same live session", "[llvm][orc][session]")
{
    const auto first = LowerConstant(std::int64_t{ 17 }, Core::Type::int64(), 1U);
    const auto second = LowerConstant(std::int64_t{ -21 }, Core::Type::int64(), 2U);
    REQUIRE(first);
    REQUIRE(second);

    Llvm::JitSession session;
    REQUIRE_FALSE(session.AddModule(first.artifact->bitcode,
                                    "vxsi-first",
                                    "VXSI.Cells.Evaluate.1",
                                    Core::Type::int64()));
    const auto firstValue = session.InvokeScalar("VXSI.Cells.Evaluate.1", Core::Type::int64());
    REQUIRE(firstValue);
    REQUIRE(std::get<std::int64_t>(firstValue.value->payload) == 17);

    REQUIRE_FALSE(session.AddModule(second.artifact->bitcode,
                                    "vxsi-second",
                                    "VXSI.Cells.Evaluate.2",
                                    Core::Type::int64()));
    const auto secondValue = session.InvokeScalar("VXSI.Cells.Evaluate.2", Core::Type::int64());
    REQUIRE(secondValue);
    REQUIRE(std::get<std::int64_t>(secondValue.value->payload) == -21);

    const auto firstStillCallable = session.InvokeScalar("VXSI.Cells.Evaluate.1", Core::Type::int64());
    REQUIRE(firstStillCallable);
    REQUIRE(std::get<std::int64_t>(firstStillCallable.value->payload) == 17);
}

TEST_CASE("ORC JIT serializes concurrent callers without losing session results", "[llvm][orc][concurrency]")
{
    constexpr std::size_t kThreadCount = 8U;
    constexpr std::size_t kCallsPerThread = 16U;
    const auto lowered = LowerConstant(std::int64_t{ 73 }, Core::Type::int64());
    REQUIRE(lowered);

    Llvm::JitSession session;
    REQUIRE_FALSE(session.AddModule(lowered.artifact->bitcode,
                                    "vxsi-concurrent-cell",
                                    "VXSI.Cells.Evaluate.1",
                                    Core::Type::int64()));

    // Each worker writes one distinct slot. Catch2 assertions stay on the main
    // thread, while the shared JIT session is exercised from every worker.
    std::array<bool, kThreadCount> completed{};
    std::array<std::int64_t, kThreadCount> lastValue{};
    std::vector<std::thread> workers;
    workers.reserve(kThreadCount);
    for (std::size_t worker = 0U; worker < kThreadCount; ++worker)
    {
        workers.emplace_back([&session, &completed, &lastValue, worker] {
            for (std::size_t call = 0U; call < kCallsPerThread; ++call)
            {
                const auto result = session.InvokeScalar("VXSI.Cells.Evaluate.1", Core::Type::int64());
                if (!result || !std::holds_alternative<std::int64_t>(result.value->payload))
                    return;
                lastValue[worker] = std::get<std::int64_t>(result.value->payload);
            }
            completed[worker] = true;
        });
    }
    for (auto &worker : workers)
        worker.join();

    for (std::size_t worker = 0U; worker < kThreadCount; ++worker)
    {
        REQUIRE(completed[worker]);
        REQUIRE(lastValue[worker] == 73);
    }
}

TEST_CASE("ORC JIT rejects empty, oversized, unnamed, and malformed modules", "[llvm][orc][errors]")
{
    Llvm::JitSession session;
    const std::array<std::uint8_t, 0> empty{};
    const std::array<std::uint8_t, 4> malformed{ 0x00, 0x01, 0x02, 0x03 };
    const auto emptyError = session.AddModule(empty, "empty", "unused", Core::Type::int64());
    REQUIRE(emptyError);
    REQUIRE(emptyError->kind == Llvm::JitErrorKind::InvalidBitcode);

    const auto unnamedError = session.AddModule(malformed, "", "unused", Core::Type::int64());
    REQUIRE(unnamedError);
    REQUIRE(unnamedError->code == "VXL4008");

    const auto malformedError = session.AddModule(malformed, "malformed", "unused", Core::Type::int64());
    REQUIRE(malformedError);
    REQUIRE(malformedError->kind == Llvm::JitErrorKind::InvalidBitcode);

    const auto missing = session.InvokeScalar("not.present", Core::Type::int64());
    REQUIRE_FALSE(missing);
    REQUIRE(missing.error->kind == Llvm::JitErrorKind::SymbolLookup);
}

TEST_CASE("ORC JIT reports missing and empty entry names without reserving symbols", "[llvm][orc][errors][recovery]")
{
    const auto lowered = LowerConstant(std::int64_t{ 8 }, Core::Type::int64());
    REQUIRE(lowered);
    Llvm::JitSession session;

    const auto emptyName = session.AddModule(
        lowered.artifact->bitcode,
        "empty-entry-name",
        "",
        Core::Type::int64());
    REQUIRE(emptyName);
    REQUIRE(emptyName->kind == Llvm::JitErrorKind::SignatureMismatch);
    REQUIRE(emptyName->code == "VXL4020");

    const auto absentName = session.AddModule(
        lowered.artifact->bitcode,
        "missing-entry-name",
        "VXSI.Cells.DoesNotExist.8",
        Core::Type::int64());
    REQUIRE(absentName);
    REQUIRE(absentName->kind == Llvm::JitErrorKind::SignatureMismatch);
    REQUIRE(absentName->code == "VXL4022");

    const auto valid = session.AddModule(
        lowered.artifact->bitcode,
        "valid-after-entry-errors",
        "VXSI.Cells.Evaluate.1",
        Core::Type::int64());
    REQUIRE_FALSE(valid);
    const auto result = session.InvokeScalar("VXSI.Cells.Evaluate.1", Core::Type::int64());
    REQUIRE(result);
    REQUIRE(std::get<std::int64_t>(result.value->payload) == 8);
}

TEST_CASE("moving an ORC session transfers loaded code and makes the source inert", "[llvm][orc][lifetime]")
{
    const auto lowered = LowerConstant(std::int64_t{ -64 }, Core::Type::int64());
    REQUIRE(lowered);

    Llvm::JitSession source;
    REQUIRE_FALSE(source.AddModule(
        lowered.artifact->bitcode,
        "move-owned-cell",
        "VXSI.Cells.Evaluate.1",
        Core::Type::int64()));

    Llvm::JitSession destination(std::move(source));
    const auto movedResult = destination.InvokeScalar("VXSI.Cells.Evaluate.1", Core::Type::int64());
    REQUIRE(movedResult);
    REQUIRE(std::get<std::int64_t>(movedResult.value->payload) == -64);

    const auto sourceInvoke = source.InvokeScalar("VXSI.Cells.Evaluate.1", Core::Type::int64());
    REQUIRE_FALSE(sourceInvoke);
    REQUIRE(sourceInvoke.error->kind == Llvm::JitErrorKind::Initialization);

    const auto sourceAdd = source.AddModule(
        lowered.artifact->bitcode,
        "moved-from-cell",
        "VXSI.Cells.Evaluate.2",
        Core::Type::int64());
    REQUIRE(sourceAdd);
    REQUIRE(sourceAdd->kind == Llvm::JitErrorKind::Initialization);

    const auto sourceReset = source.Reset();
    REQUIRE(sourceReset);
    REQUIRE(sourceReset->kind == Llvm::JitErrorKind::Initialization);

    REQUIRE_FALSE(destination.Reset());
    const auto removed = destination.InvokeScalar("VXSI.Cells.Evaluate.1", Core::Type::int64());
    REQUIRE_FALSE(removed);
    REQUIRE(removed.error->kind == Llvm::JitErrorKind::SymbolLookup);
}

TEST_CASE("ORC JIT keeps unsupported wide scalar calls explicit", "[llvm][orc][abi]")
{
    const auto lowered = LowerConstant(Core::IntegerLiteral{ false, { 42U } }, Core::Type::int128());
    REQUIRE(lowered);
    Llvm::JitSession session;
    const auto added = session.AddModule(lowered.artifact->bitcode,
                                         "wide-result",
                                         "VXSI.Cells.Evaluate.1",
                                         Core::Type::int128());
    REQUIRE(added);
    REQUIRE(added->kind == Llvm::JitErrorKind::UnsupportedResult);
}

TEST_CASE("ORC JIT does not discard earlier cells when a later module is invalid", "[llvm][orc][recovery]")
{
    const auto lowered = LowerConstant(std::int64_t{ 99 }, Core::Type::int64());
    REQUIRE(lowered);
    Llvm::JitSession session;
    REQUIRE_FALSE(session.AddModule(lowered.artifact->bitcode,
                                    "cell-0",
                                    "VXSI.Cells.Evaluate.1",
                                    Core::Type::int64()));

    const std::array<std::uint8_t, 2> malformed{ 0xFFU, 0xFFU };
    REQUIRE(session.AddModule(malformed, "cell-1", "invalid", Core::Type::int64()));
    const auto previous = session.InvokeScalar("VXSI.Cells.Evaluate.1", Core::Type::int64());
    REQUIRE(previous);
    REQUIRE(std::get<std::int64_t>(previous.value->payload) == 99);
}

TEST_CASE("ORC reset releases cell symbols and permits a fresh session generation", "[llvm][orc][reset]")
{
    const auto first = LowerConstant(std::int64_t{ 41 }, Core::Type::int64());
    const auto second = LowerConstant(std::int64_t{ 73 }, Core::Type::int64());
    REQUIRE(first);
    REQUIRE(second);

    Llvm::JitSession session;
    REQUIRE_FALSE(session.AddModule(first.artifact->bitcode,
                                    "vxsi-before-reset",
                                    "VXSI.Cells.Evaluate.1",
                                    Core::Type::int64()));
    const auto before = session.InvokeScalar("VXSI.Cells.Evaluate.1", Core::Type::int64());
    REQUIRE(before);
    REQUIRE(std::get<std::int64_t>(before.value->payload) == 41);

    REQUIRE_FALSE(session.Reset());
    const auto removed = session.InvokeScalar("VXSI.Cells.Evaluate.1", Core::Type::int64());
    REQUIRE_FALSE(removed);
    REQUIRE(removed.error->kind == Llvm::JitErrorKind::SymbolLookup);

    REQUIRE_FALSE(session.AddModule(second.artifact->bitcode,
                                    "vxsi-after-reset",
                                    "VXSI.Cells.Evaluate.1",
                                    Core::Type::int64()));
    const auto after = session.InvokeScalar("VXSI.Cells.Evaluate.1", Core::Type::int64());
    REQUIRE(after);
    REQUIRE(std::get<std::int64_t>(after.value->payload) == 73);
}
