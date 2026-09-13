// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <initializer_list>
#include <ranges>
#include <utility>
#include <vector>

#include "Visual/XSharp/Analysis/Liveness.hpp"

namespace
{
    namespace Live = Visual::XSharp::Analysis::Liveness;

    [[nodiscard]] auto
    Read(const std::size_t instruction, std::initializer_list<Live::StorageId> reads) -> Live::Access
    {
        return { instruction, false, { reads }, std::nullopt, false };
    }

    [[nodiscard]] auto
    Write(
        const std::size_t instruction,
        const Live::StorageId destination,
        std::initializer_list<Live::StorageId> reads = {}) -> Live::Access
    {
        return { instruction, false, { reads }, destination, true };
    }

    [[nodiscard]] auto
    Effect(const std::size_t instruction, std::initializer_list<Live::StorageId> reads) -> Live::Access
    {
        return { instruction, false, { reads }, std::nullopt, false };
    }

    [[nodiscard]] auto
    Terminator(std::initializer_list<Live::StorageId> reads) -> Live::Access
    {
        return { 99U, true, { reads }, std::nullopt, false };
    }

    [[nodiscard]] auto
    Block(
        const Live::BlockId id,
        std::vector<Live::BlockId> successors,
        std::vector<Live::Access> accesses) -> Live::Block
    {
        return { id, std::move(successors), std::move(accesses) };
    }

    [[nodiscard]] auto
    Facts(const Live::Result &result, const Live::BlockId block) -> const Live::BlockFacts &
    {
        const auto found = std::ranges::find(result.facts, block, &Live::BlockFacts::block);
        REQUIRE(found != result.facts.end());
        return *found;
    }

    [[nodiscard]] auto
    Retained(const Live::BlockFacts &facts) -> std::vector<bool>
    {
        std::vector<bool> retained;
        for (const auto &access : facts.accesses)
            retained.push_back(access.retained);
        return retained;
    }
} // namespace

TEST_CASE("liveness removes a complete dead producer chain in one result")
{
    const auto result = Live::Analyze({
        0U,
        { Block(0U, {}, { Write(0U, 1U), Write(1U, 2U, { 1U }), Write(2U, 3U, { 2U }), Terminator({}) }) },
    });

    REQUIRE(result.valid());
    const auto &facts = Facts(result, 0U);
    CHECK(Retained(facts) == std::vector<bool>{ false, false, false, true });
    CHECK(facts.liveOnEntry.empty());
    CHECK(facts.liveOnExit.empty());
}

TEST_CASE("an observable use retains its transitive producer chain")
{
    const auto result = Live::Analyze({
        0U,
        { Block(0U, {}, { Write(0U, 1U), Write(1U, 2U, { 1U }), Effect(2U, { 2U }), Terminator({}) }) },
    });

    REQUIRE(result.valid());
    const auto &facts = Facts(result, 0U);
    CHECK(Retained(facts) == std::vector<bool>{ true, true, true, true });
    CHECK(facts.accesses[0].liveAfter == std::vector<Live::StorageId>{ 1U });
    CHECK(facts.accesses[1].liveAfter == std::vector<Live::StorageId>{ 2U });
}

TEST_CASE("a terminator keeps its condition and the condition producer live")
{
    const auto result = Live::Analyze({
        0U,
        { Block(0U, { 1U, 2U }, { Write(0U, 5U), Terminator({ 5U }) }),
          Block(1U, {}, { Terminator({}) }),
          Block(2U, {}, { Terminator({}) }) },
    });

    REQUIRE(result.valid());
    CHECK(Retained(Facts(result, 0U)) == std::vector<bool>{ true, true });
    CHECK(Facts(result, 0U).liveOnEntry.empty());
}

TEST_CASE("successor uses are united across branch edges")
{
    const auto result = Live::Analyze({
        0U,
        { Block(0U, { 1U, 2U }, { Write(0U, 10U), Write(1U, 20U), Terminator({}) }),
          Block(1U, {}, { Read(0U, { 10U }), Terminator({}) }),
          Block(2U, {}, { Read(0U, { 20U }), Terminator({}) }) },
    });

    REQUIRE(result.valid());
    CHECK(Retained(Facts(result, 0U)) == std::vector<bool>{ true, true, true });
    CHECK(Facts(result, 0U).liveOnExit == std::vector<Live::StorageId>{ 10U, 20U });
}

TEST_CASE("loop-carried liveness reaches a fixed point")
{
    const auto result = Live::Analyze({
        0U,
        { Block(0U, { 1U }, { Write(0U, 1U), Terminator({}) }),
          Block(1U, { 1U, 2U }, { Write(0U, 1U, { 1U }), Terminator({ 1U }) }),
          Block(2U, {}, { Read(0U, { 1U }), Terminator({}) }) },
    });

    REQUIRE(result.valid());
    CHECK(Retained(Facts(result, 0U)).front());
    CHECK(Retained(Facts(result, 1U)).front());
    CHECK(Facts(result, 1U).liveOnEntry == std::vector<Live::StorageId>{ 1U });
    CHECK(Facts(result, 1U).liveOnExit == std::vector<Live::StorageId>{ 1U });
}

TEST_CASE("unreachable blocks receive conservative non-removal facts")
{
    const auto result = Live::Analyze({
        0U,
        { Block(0U, {}, { Terminator({}) }), Block(9U, {}, { Write(0U, 7U), Terminator({}) }) },
    });

    REQUIRE(result.valid());
    const auto &unreachable = Facts(result, 9U);
    CHECK_FALSE(unreachable.reachable);
    CHECK(Retained(unreachable) == std::vector<bool>{ true, true });
}

TEST_CASE("facts are independent of block presentation order")
{
    const Live::Function ordered{
        0U,
        { Block(0U, { 1U }, { Write(0U, 8U), Terminator({}) }),
          Block(1U, {}, { Read(0U, { 8U }), Terminator({}) }) },
    };
    const Live::Function shuffled{
        0U,
        { Block(1U, {}, { Read(0U, { 8U }), Terminator({}) }),
          Block(0U, { 1U }, { Write(0U, 8U), Terminator({}) }) },
    };

    CHECK(Live::Analyze(ordered).facts == Live::Analyze(shuffled).facts);
}

TEST_CASE("malformed control flow remains diagnostic and finite")
{
    const auto missingEntry = Live::Analyze({ 9U, { Block(0U, { 4U }, { Terminator({}) }) } });
    REQUIRE(missingEntry.issues.size() == 2U);
    CHECK(missingEntry.issues[0].kind == Visual::XSharp::Analysis::ControlFlowIssueKind::MissingEntry);
    CHECK(missingEntry.issues[1].kind == Visual::XSharp::Analysis::ControlFlowIssueKind::MissingTarget);
}
