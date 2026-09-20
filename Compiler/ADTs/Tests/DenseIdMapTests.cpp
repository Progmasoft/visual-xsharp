// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "Visual/XSharp/ADTs/DenseIdMap.hpp"

namespace
{
    namespace ADTs = Visual::XSharp::ADTs;
    using Id = std::uint64_t;
} // namespace

TEST_CASE("DenseIdMap preserves the complete unsigned identity domain")
{
    constexpr auto kMaximum = std::numeric_limits<Id>::max();
    ADTs::DenseIdMap<Id, std::string> values;

    CHECK(values.TryEmplace(0U, "zero").inserted);
    CHECK(values.TryEmplace(kMaximum - 1U, "penultimate").inserted);
    CHECK(values.TryEmplace(kMaximum, "maximum").inserted);

    REQUIRE(values.Size() == 3U);
    CHECK(*values.Find(0U) == "zero");
    CHECK(*values.Find(kMaximum - 1U) == "penultimate");
    CHECK(*values.Find(kMaximum) == "maximum");
}

TEST_CASE("DenseIdMap reports duplicate insertion and supports assignment")
{
    ADTs::DenseIdMap<Id, std::string> values;
    const auto first = values.TryEmplace(42U, "first");
    const auto duplicate = values.TryEmplace(42U, "second");

    REQUIRE(first.inserted);
    CHECK_FALSE(duplicate.inserted);
    CHECK(first.value == duplicate.value);
    CHECK(*values.Find(42U) == "first");

    values.InsertOrAssign(42U, "assigned");
    CHECK(*values.Find(42U) == "assigned");
    CHECK(values.Find(7U) == nullptr);
}

TEST_CASE("DenseIdMap iteration exposes only domain identities")
{
    ADTs::DenseIdMap<Id, int> values;
    values.Reserve(3U);
    values.TryEmplace(8U, 80);
    values.TryEmplace(3U, 30);
    values.TryEmplace(5U, 50);

    std::vector<Id> identities;
    values.ForEach([&identities](const Id id, const int value) {
        CHECK(value == static_cast<int>(id * 10U));
        identities.push_back(id);
    });
    std::ranges::sort(identities);
    CHECK(identities == std::vector<Id>{ 3U, 5U, 8U });
}

TEST_CASE("DenseIdSet preserves boundary identities and duplicate semantics")
{
    constexpr auto kMaximum = std::numeric_limits<Id>::max();
    ADTs::DenseIdSet<Id> values;

    CHECK(values.Insert(0U));
    CHECK(values.Insert(kMaximum - 1U));
    CHECK(values.Insert(kMaximum));
    CHECK_FALSE(values.Insert(kMaximum));
    CHECK(values.Contains(0U));
    CHECK(values.Contains(kMaximum - 1U));
    CHECK(values.Contains(kMaximum));
    CHECK(values.Size() == 3U);
}
