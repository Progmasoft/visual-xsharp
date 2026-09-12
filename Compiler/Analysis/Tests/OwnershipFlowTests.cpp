// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <ranges>
#include <utility>
#include <vector>

#include "Visual/XSharp/Analysis/OwnershipFlow.hpp"

namespace
{
    namespace Flow = Visual::XSharp::Analysis::OwnershipFlow;

    [[nodiscard]] auto
    Observe(
        Flow::HandleId handle,
        Flow::HandleKind kind,
        std::size_t instruction = 0U,
        bool terminator = false) -> Flow::Action
    {
        return { Flow::ActionKind::Observe, handle, kind, instruction, terminator };
    }

    [[nodiscard]] auto
    Consume(
        Flow::HandleId handle,
        Flow::HandleKind kind,
        std::size_t instruction = 0U) -> Flow::Action
    {
        return { Flow::ActionKind::Consume, handle, kind, instruction, false };
    }

    [[nodiscard]] auto
    Define(
        Flow::HandleId handle,
        Flow::HandleKind kind,
        std::size_t instruction = 0U) -> Flow::Action
    {
        return { Flow::ActionKind::Define, handle, kind, instruction, false };
    }

    [[nodiscard]] auto
    Forget(Flow::HandleId handle, std::size_t instruction = 0U) -> Flow::Action
    {
        return {
            Flow::ActionKind::Forget,
            handle,
            Flow::HandleKind::Strong,
            instruction,
            false,
        };
    }

    [[nodiscard]] auto
    Block(
        Flow::BlockId id,
        std::vector<Flow::BlockId> successors,
        std::vector<Flow::Action> actions) -> Flow::Block
    {
        return { id, std::move(successors), std::move(actions) };
    }

    [[nodiscard]] auto
    Function(
        std::vector<Flow::Block> blocks,
        std::vector<Flow::InitialHandle> initial = {}) -> Flow::Function
    {
        return { 0U, std::move(initial), std::move(blocks) };
    }

    [[nodiscard]] auto
    IssuesOf(
        const Flow::Result &result,
        Flow::IssueKind kind) -> std::vector<Flow::Issue>
    {
        std::vector<Flow::Issue> issues;
        std::ranges::copy_if(
            result.issues,
            std::back_inserter(issues),
            [kind](const auto &issue) {
                return issue.kind == kind;
            });
        return issues;
    }

    [[nodiscard]] auto
    FactsFor(const Flow::Result &result, Flow::BlockId block) -> const Flow::BlockFacts &
    {
        const auto found = std::ranges::find(result.facts, block, &Flow::BlockFacts::block);
        REQUIRE(found != result.facts.end());
        return *found;
    }

    [[nodiscard]] auto
    StateOf(const std::vector<Flow::HandleFact> &facts, Flow::HandleId handle) -> Flow::StateMask
    {
        const auto found = std::ranges::find(facts, handle, &Flow::HandleFact::handle);
        REQUIRE(found != facts.end());
        return found->states;
    }
} // namespace

TEST_CASE("ownership flow accepts a live strong parameter observation")
{
    const auto result = Flow::Analyze(Function(
        { Block(0U, {}, { Observe(1U, Flow::HandleKind::Strong) }) },
        { { 1U, Flow::HandleKind::Strong } }));
    CHECK(result.issues.empty());
    CHECK(Flow::IsExactly(
        StateOf(FactsFor(result, 0U).outgoing, 1U),
        Flow::HandleKind::Strong));
}

TEST_CASE("ownership flow preserves weak observations")
{
    const auto result = Flow::Analyze(Function(
        { Block(0U, {}, { Observe(2U, Flow::HandleKind::Weak) }) },
        { { 2U, Flow::HandleKind::Weak } }));
    CHECK(result.issues.empty());
    CHECK(Flow::IsExactly(
        StateOf(FactsFor(result, 0U).outgoing, 2U),
        Flow::HandleKind::Weak));
}

TEST_CASE("ownership flow preserves unowned observations")
{
    const auto result = Flow::Analyze(Function(
        { Block(0U, {}, { Observe(3U, Flow::HandleKind::Unowned) }) },
        { { 3U, Flow::HandleKind::Unowned } }));
    CHECK(result.issues.empty());
}

TEST_CASE("ownership flow consumes a strong token")
{
    const auto result = Flow::Analyze(Function(
        { Block(0U, {}, { Consume(1U, Flow::HandleKind::Strong) }) },
        { { 1U, Flow::HandleKind::Strong } }));
    CHECK(result.issues.empty());
    CHECK(StateOf(FactsFor(result, 0U).outgoing, 1U) == Flow::kConsumed);
}

TEST_CASE("ownership flow reports a strong use after release")
{
    const auto result = Flow::Analyze(Function(
        { Block(
            0U,
            {},
            {
                Consume(1U, Flow::HandleKind::Strong, 0U),
                Observe(1U, Flow::HandleKind::Strong, 1U),
            }) },
        { { 1U, Flow::HandleKind::Strong } }));
    const auto issues = IssuesOf(result, Flow::IssueKind::UseAfterConsume);
    REQUIRE(issues.size() == 1U);
    CHECK(issues.front().instruction == 1U);
    CHECK(issues.front().handle == 1U);
    CHECK(issues.front().actual == Flow::kConsumed);
}

TEST_CASE("ownership flow reports a double weak release")
{
    const auto result = Flow::Analyze(Function(
        { Block(
            0U,
            {},
            {
                Consume(7U, Flow::HandleKind::Weak, 2U),
                Consume(7U, Flow::HandleKind::Weak, 3U),
            }) },
        { { 7U, Flow::HandleKind::Weak } }));
    const auto issues = IssuesOf(result, Flow::IssueKind::UseAfterConsume);
    REQUIRE(issues.size() == 1U);
    CHECK(issues.front().instruction == 3U);
}

TEST_CASE("ownership flow reports a double unowned release")
{
    const auto result = Flow::Analyze(Function(
        { Block(
            0U,
            {},
            {
                Consume(9U, Flow::HandleKind::Unowned),
                Consume(9U, Flow::HandleKind::Unowned, 1U),
            }) },
        { { 9U, Flow::HandleKind::Unowned } }));
    CHECK(IssuesOf(result, Flow::IssueKind::UseAfterConsume).size() == 1U);
}

TEST_CASE("ownership flow rejects releasing a weak handle as strong")
{
    const auto result = Flow::Analyze(Function(
        { Block(0U, {}, { Consume(2U, Flow::HandleKind::Strong) }) },
        { { 2U, Flow::HandleKind::Weak } }));
    const auto issues = IssuesOf(result, Flow::IssueKind::HandleKindMismatch);
    REQUIRE(issues.size() == 1U);
    CHECK(issues.front().expected == Flow::HandleKind::Strong);
    CHECK(Flow::Contains(issues.front().actual, Flow::kWeak));
}

TEST_CASE("ownership flow rejects locking an unowned handle")
{
    const auto result = Flow::Analyze(Function(
        { Block(0U, {}, { Observe(3U, Flow::HandleKind::Weak) }) },
        { { 3U, Flow::HandleKind::Unowned } }));
    CHECK(IssuesOf(result, Flow::IssueKind::HandleKindMismatch).size() == 1U);
}

TEST_CASE("ownership flow definitions establish their declared handle kind")
{
    const auto result = Flow::Analyze(Function(
        { Block(
            0U,
            {},
            {
                Define(1U, Flow::HandleKind::Strong),
                Define(2U, Flow::HandleKind::Weak, 1U),
                Define(3U, Flow::HandleKind::Unowned, 2U),
                Observe(1U, Flow::HandleKind::Strong, 3U),
                Observe(2U, Flow::HandleKind::Weak, 4U),
                Observe(3U, Flow::HandleKind::Unowned, 5U),
            }) }));
    CHECK(result.issues.empty());
}

TEST_CASE("ownership flow definition replaces consumed storage")
{
    const auto result = Flow::Analyze(Function(
        { Block(
            0U,
            {},
            {
                Consume(1U, Flow::HandleKind::Strong),
                Define(1U, Flow::HandleKind::Strong, 1U),
                Observe(1U, Flow::HandleKind::Strong, 2U),
            }) },
        { { 1U, Flow::HandleKind::Strong } }));
    CHECK(result.issues.empty());
    CHECK(Flow::IsExactly(
        StateOf(FactsFor(result, 0U).outgoing, 1U),
        Flow::HandleKind::Strong));
}

TEST_CASE("ownership flow forget removes a tracked token")
{
    const auto result = Flow::Analyze(Function(
        { Block(
            0U,
            {},
            {
                Define(1U, Flow::HandleKind::Strong),
                Forget(1U, 1U),
            }) }));
    CHECK(result.issues.empty());
    CHECK(StateOf(FactsFor(result, 0U).outgoing, 1U) == Flow::kAbsent);
}

TEST_CASE("ownership flow reports a release present on only one diamond path")
{
    const auto result = Flow::Analyze(Function(
        {
            Block(0U, { 1U, 2U }, {}),
            Block(1U, { 3U }, { Consume(1U, Flow::HandleKind::Strong) }),
            Block(2U, { 3U }, {}),
            Block(3U, {}, { Observe(1U, Flow::HandleKind::Strong) }),
        },
        { { 1U, Flow::HandleKind::Strong } }));
    const auto issues = IssuesOf(result, Flow::IssueKind::PathStateMismatch);
    REQUIRE(issues.size() == 1U);
    CHECK(issues.front().block == 3U);
    CHECK(Flow::Contains(issues.front().actual, Flow::kStrong));
    CHECK(Flow::Contains(issues.front().actual, Flow::kConsumed));
}

TEST_CASE("ownership flow accepts a release performed on every diamond path")
{
    const auto result = Flow::Analyze(Function(
        {
            Block(0U, { 1U, 2U }, {}),
            Block(1U, { 3U }, { Consume(1U, Flow::HandleKind::Strong) }),
            Block(2U, { 3U }, { Consume(1U, Flow::HandleKind::Strong) }),
            Block(3U, {}, {}),
        },
        { { 1U, Flow::HandleKind::Strong } }));
    CHECK(result.issues.empty());
    CHECK(StateOf(FactsFor(result, 3U).incoming, 1U) == Flow::kConsumed);
}

TEST_CASE("ownership flow reports kind disagreement across a join")
{
    const auto result = Flow::Analyze(Function(
        {
            Block(0U, { 1U, 2U }, {}),
            Block(1U, { 3U }, { Define(4U, Flow::HandleKind::Weak) }),
            Block(2U, { 3U }, { Define(4U, Flow::HandleKind::Unowned) }),
            Block(3U, {}, { Observe(4U, Flow::HandleKind::Weak) }),
        }));
    const auto issues = IssuesOf(result, Flow::IssueKind::PathStateMismatch);
    REQUIRE(issues.size() == 1U);
    CHECK(Flow::Contains(issues.front().actual, Flow::kWeak));
    CHECK(Flow::Contains(issues.front().actual, Flow::kUnowned));
}

TEST_CASE("ownership flow reports definition present on only one path")
{
    const auto result = Flow::Analyze(Function(
        {
            Block(0U, { 1U, 2U }, {}),
            Block(1U, { 3U }, { Define(5U, Flow::HandleKind::Strong) }),
            Block(2U, { 3U }, {}),
            Block(3U, {}, { Observe(5U, Flow::HandleKind::Strong) }),
        }));
    const auto issues = IssuesOf(result, Flow::IssueKind::PathStateMismatch);
    REQUIRE(issues.size() == 1U);
    CHECK(Flow::Contains(issues.front().actual, Flow::kStrong));
    CHECK(Flow::Contains(issues.front().actual, Flow::kAbsent));
}

TEST_CASE("ownership flow reaches a fixed point through a live loop")
{
    const auto result = Flow::Analyze(Function(
        {
            Block(0U, { 1U }, {}),
            Block(1U, { 1U, 2U }, { Observe(1U, Flow::HandleKind::Strong) }),
            Block(2U, {}, { Observe(1U, Flow::HandleKind::Strong) }),
        },
        { { 1U, Flow::HandleKind::Strong } }));
    CHECK(result.issues.empty());
}

TEST_CASE("ownership flow does not manufacture a token through an entry backedge")
{
    const auto result = Flow::Analyze(Function(
        {
            Block(0U, { 1U }, { Observe(1U, Flow::HandleKind::Strong) }),
            Block(1U, { 0U }, { Define(1U, Flow::HandleKind::Strong) }),
        }));
    // Definite-initialization owns the absent-only diagnostic. The ownership
    // analysis must still keep the entry fact absent instead of accepting the
    // value produced by a later iteration.
    CHECK(result.issues.empty());
    CHECK(StateOf(FactsFor(result, 0U).incoming, 1U) == Flow::kAbsent);
}

TEST_CASE("ownership flow ignores invalid operations in unreachable blocks")
{
    const auto result = Flow::Analyze(Function(
        {
            Block(0U, {}, {}),
            Block(
                8U,
                {},
                {
                    Consume(1U, Flow::HandleKind::Strong),
                    Observe(1U, Flow::HandleKind::Strong, 1U),
                }),
        }));
    CHECK(result.issues.empty());
    CHECK_FALSE(FactsFor(result, 8U).reachable);
}

TEST_CASE("ownership flow facts are independent of block presentation order")
{
    auto function = Function(
        {
            Block(0U, { 1U }, { Define(1U, Flow::HandleKind::Strong) }),
            Block(1U, {}, { Observe(1U, Flow::HandleKind::Strong) }),
        });
    const auto forward = Flow::Analyze(function);
    std::ranges::reverse(function.blocks);
    const auto reverse = Flow::Analyze(function);
    CHECK(forward == reverse);
}

TEST_CASE("ownership flow rejects a duplicate block")
{
    const auto result = Flow::Analyze(Function(
        {
            Block(0U, {}, {}),
            Block(0U, {}, {}),
        }));
    CHECK(IssuesOf(result, Flow::IssueKind::DuplicateBlock).size() == 1U);
}

TEST_CASE("ownership flow rejects a missing entry")
{
    Flow::Function function;
    function.entry = 4U;
    function.blocks = { Block(0U, {}, {}) };
    const auto result = Flow::Analyze(function);
    CHECK(IssuesOf(result, Flow::IssueKind::MissingEntry).size() == 1U);
    CHECK_FALSE(FactsFor(result, 0U).reachable);
}

TEST_CASE("ownership flow rejects an invalid branch target")
{
    const auto result = Flow::Analyze(Function(
        { Block(0U, { 99U }, {}) }));
    const auto issues = IssuesOf(result, Flow::IssueKind::InvalidTarget);
    REQUIRE(issues.size() == 1U);
    CHECK(issues.front().block == 0U);
    CHECK(issues.front().terminator);
}

TEST_CASE("ownership flow rejects handle zero in an initial state")
{
    const auto result = Flow::Analyze(Function(
        { Block(0U, {}, {}) },
        { { 0U, Flow::HandleKind::Strong } }));
    CHECK(IssuesOf(result, Flow::IssueKind::InvalidInitialHandle).size() == 1U);
}

TEST_CASE("ownership flow rejects handle zero in an action")
{
    const auto result = Flow::Analyze(Function(
        { Block(0U, {}, { Observe(0U, Flow::HandleKind::Strong, 7U) }) }));
    const auto issues = IssuesOf(result, Flow::IssueKind::InvalidActionHandle);
    REQUIRE(issues.size() == 1U);
    CHECK(issues.front().instruction == 7U);
}

TEST_CASE("ownership flow rejects conflicting initial handle kinds")
{
    const auto result = Flow::Analyze(Function(
        { Block(0U, {}, {}) },
        {
            { 1U, Flow::HandleKind::Strong },
            { 1U, Flow::HandleKind::Weak },
        }));
    const auto issues = IssuesOf(result, Flow::IssueKind::ConflictingInitialKind);
    REQUIRE(issues.size() == 1U);
    CHECK(Flow::Contains(issues.front().actual, Flow::kStrong));
}

TEST_CASE("ownership flow preserves terminator source locations")
{
    const auto result = Flow::Analyze(Function(
        { Block(
            0U,
            {},
            {
                Consume(1U, Flow::HandleKind::Strong, 0U),
                Observe(1U, Flow::HandleKind::Strong, 4U, true),
            }) },
        { { 1U, Flow::HandleKind::Strong } }));
    const auto issues = IssuesOf(result, Flow::IssueKind::UseAfterConsume);
    REQUIRE(issues.size() == 1U);
    CHECK(issues.front().terminator);
    CHECK(issues.front().instruction == 4U);
}

TEST_CASE("ownership flow state helpers distinguish every handle class")
{
    CHECK(Flow::StateFor(Flow::HandleKind::Strong) == Flow::kStrong);
    CHECK(Flow::StateFor(Flow::HandleKind::Weak) == Flow::kWeak);
    CHECK(Flow::StateFor(Flow::HandleKind::Unowned) == Flow::kUnowned);
    CHECK(Flow::IsExactly(Flow::kStrong, Flow::HandleKind::Strong));
    CHECK_FALSE(Flow::IsExactly(
        static_cast<Flow::StateMask>(Flow::kStrong | Flow::kConsumed),
        Flow::HandleKind::Strong));
}
