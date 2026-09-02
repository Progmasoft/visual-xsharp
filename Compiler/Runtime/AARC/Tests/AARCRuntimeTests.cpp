// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <thread>
#include <vector>

#include "Visual/XSharp/Core/Ownership.hpp"
#include "Visual/XSharp/Runtime/AARC.hpp"

namespace
{
    namespace Aarc = Visual::XSharp::Runtime::Aarc;
    namespace Core = visual_xsharp::core;

    struct Payload final
    {
        std::uint64_t marker{};
    };

    std::atomic_uint32_t destructions{};

    void
    DestroyPayload(void *value) noexcept
    {
        auto *payload = static_cast<Payload *>(value);
        payload->marker = 0U;
        destructions.fetch_add(1U, std::memory_order_relaxed);
    }

    const Aarc::TypeMetadata kMetadata{
        Aarc::kAbiVersion,
        0U,
        sizeof(Payload),
        alignof(Payload),
        DestroyPayload,
        "Tests.Payload"
    };
} // namespace

TEST_CASE("type storage classes follow the language declaration families")
{
    CHECK(Core::ClassifyType(Core::Type::int64()) == Core::StorageClass::TrivialValue);
    CHECK(Core::ClassifyType(Core::Type::string()) == Core::StorageClass::AarcReference);
    CHECK(Core::ClassifyType(Core::Type::function({}, Core::Type::unit())) == Core::StorageClass::AarcReference);
    CHECK(Core::ClassifyNominal(Core::NominalKind::Data) == Core::StorageClass::CopyOnWriteValue);
    CHECK(Core::ClassifyNominal(Core::NominalKind::Type) == Core::StorageClass::CopyOnWriteValue);
    CHECK(Core::ClassifyNominal(Core::NominalKind::ClassicEnum) == Core::StorageClass::CopyOnWriteValue);
    CHECK(Core::ClassifyNominal(Core::NominalKind::Class) == Core::StorageClass::AarcReference);
    CHECK(Core::ClassifyNominal(Core::NominalKind::DataClass) == Core::StorageClass::AarcReference);
    CHECK(Core::ClassifyNominal(Core::NominalKind::EnumClass) == Core::StorageClass::AarcReference);
    CHECK(Core::ClassifyNominal(Core::NominalKind::Object) == Core::StorageClass::AarcReference);
    CHECK(Core::ClassifyNominal(Core::NominalKind::Interface) == Core::StorageClass::AarcReference);
    CHECK(Core::ClassifyType(Core::Type::named({ U"Unknown" })) == Core::StorageClass::Unresolved);
}

TEST_CASE("strong references destroy the payload exactly once")
{
    destructions.store(0U);
    auto *payload = static_cast<Payload *>(Aarc::Allocate(kMetadata));
    REQUIRE(payload != nullptr);
    payload->marker = 0xAACC55U;

    CHECK(Aarc::Header(payload)->strongCount.load() == 1U);
    CHECK(Aarc::RetainStrong(payload) == payload);
    CHECK(Aarc::Header(payload)->strongCount.load() == 2U);
    Aarc::ReleaseStrong(payload);
    CHECK(destructions.load() == 0U);
    Aarc::ReleaseStrong(payload);
    CHECK(destructions.load() == 1U);
}

TEST_CASE("allocation validates ABI version size and alignment")
{
    auto metadata = kMetadata;
    metadata.abiVersion = Aarc::kAbiVersion + 1U;
    CHECK(Aarc::Allocate(metadata) == nullptr);
    metadata = kMetadata;
    metadata.instanceSize = 0U;
    CHECK(Aarc::Allocate(metadata) == nullptr);
    metadata = kMetadata;
    metadata.instanceAlignment = 3U;
    CHECK(Aarc::Allocate(metadata) == nullptr);

    metadata = kMetadata;
    metadata.instanceAlignment = 64U;
    auto *payload = Aarc::Allocate(metadata);
    REQUIRE(payload != nullptr);
    CHECK(reinterpret_cast<std::uintptr_t>(payload) % 64U == 0U);
    Aarc::ReleaseStrong(payload);
}

TEST_CASE("weak lock retains a live object and expires after destruction")
{
    destructions.store(0U);
    auto *payload = static_cast<Payload *>(Aarc::Allocate(kMetadata));
    REQUIRE(payload != nullptr);
    const auto weak = Aarc::MakeWeak(payload);
    REQUIRE(weak.header != nullptr);
    const auto copied = Aarc::CopyWeak(weak);
    CHECK(copied.header == weak.header);

    auto *locked = Aarc::LockWeak(weak);
    REQUIRE(locked == payload);
    Aarc::ReleaseStrong(locked);
    Aarc::ReleaseStrong(payload);
    CHECK(destructions.load() == 1U);
    CHECK(Aarc::LockWeak(weak) == nullptr);
    CHECK(weak.header->state.load() == Aarc::ObjectState::Destroyed);
    Aarc::ReleaseWeak(copied);
    Aarc::ReleaseWeak(weak);
}

TEST_CASE("unowned handles retain only the control header")
{
    destructions.store(0U);
    auto *payload = static_cast<Payload *>(Aarc::Allocate(kMetadata));
    REQUIRE(payload != nullptr);
    const auto unowned = Aarc::MakeUnowned(payload);
    const auto copied = Aarc::CopyUnowned(unowned);
    CHECK(copied.header == unowned.header);
    auto *loaded = Aarc::LoadUnowned(unowned);
    REQUIRE(loaded == payload);
    Aarc::ReleaseStrong(loaded);

    Aarc::ReleaseStrong(payload);
    CHECK(destructions.load() == 1U);
    CHECK(Aarc::LoadUnowned(unowned) == nullptr);
    Aarc::ReleaseUnowned(copied);
    Aarc::ReleaseUnowned(unowned);
}

TEST_CASE("concurrent strong retain and release preserves one owner")
{
    destructions.store(0U);
    auto *payload = static_cast<Payload *>(Aarc::Allocate(kMetadata));
    REQUIRE(payload != nullptr);

    constexpr std::size_t kThreadCount = 8U;
    constexpr std::size_t kIterations = 2000U;
    std::atomic_bool retainedEveryTime{ true };
    std::vector<std::thread> workers;
    workers.reserve(kThreadCount);
    for (std::size_t thread = 0; thread < kThreadCount; ++thread)
        workers.emplace_back([payload, &retainedEveryTime] {
            for (std::size_t index = 0; index < kIterations; ++index)
            {
                if (Aarc::RetainStrong(payload) != payload)
                    retainedEveryTime.store(false, std::memory_order_relaxed);
                Aarc::ReleaseStrong(payload);
            }
        });
    for (auto &worker : workers)
        worker.join();

    CHECK(retainedEveryTime.load(std::memory_order_relaxed));
    CHECK(Aarc::Header(payload)->strongCount.load() == 1U);
    CHECK(destructions.load() == 0U);
    Aarc::ReleaseStrong(payload);
    CHECK(destructions.load() == 1U);
}

TEST_CASE("Unicode scalar string factory creates an AARC object")
{
    const std::uint32_t scalars[]{ 0x56U, 0x69U, 0x73U, 0x75U, 0x61U, 0x6cU, 0x20U, 0x58U, 0x23U };
    auto *string = vxs_aarc_string_literal(scalars, std::size(scalars));
    REQUIRE(string != nullptr);
    auto *header = Aarc::Header(string);
    REQUIRE(header != nullptr);
    CHECK(header->metadata->instanceSize != 0U);
    CHECK(header->strongCount.load() == 1U);
    Aarc::ReleaseStrong(string);
    const std::uint32_t surrogate[]{ 0xd800U };
    CHECK(vxs_aarc_string_literal(surrogate, 1U) == nullptr);
    const std::uint32_t outsideUnicode[]{ 0x110000U };
    CHECK(vxs_aarc_string_literal(outsideUnicode, 1U) == nullptr);
}
