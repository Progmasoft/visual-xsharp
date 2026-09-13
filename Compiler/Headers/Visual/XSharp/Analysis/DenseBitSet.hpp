// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Visual::XSharp::Analysis
{
    // DenseBitSet is the storage engine for compiler dataflow lattices whose
    // universes are catalogued before iteration. It deliberately exposes only
    // bounds-checked semantic operations; passes never depend on word layout.
    class DenseBitSet final
    {
    public:
        DenseBitSet() = default;
        explicit DenseBitSet(std::size_t bitCount, bool value = false);

        [[nodiscard]] auto
        Size() const noexcept -> std::size_t;

        [[nodiscard]] auto
        Empty() const noexcept -> bool;

        [[nodiscard]] auto
        None() const noexcept -> bool;

        [[nodiscard]] auto
        Any() const noexcept -> bool;

        [[nodiscard]] auto
        Count() const noexcept -> std::size_t;

        [[nodiscard]] auto
        Test(std::size_t index) const noexcept -> bool;

        void
        Set(std::size_t index) noexcept;

        void
        Reset(std::size_t index) noexcept;

        void
        Assign(std::size_t index, bool value) noexcept;

        void
        Clear() noexcept;

        void
        Fill() noexcept;

        void
        UnionWith(const DenseBitSet &other);

        void
        IntersectWith(const DenseBitSet &other);

        void
        Subtract(const DenseBitSet &other);

        [[nodiscard]] auto
        SetIndices() const -> std::vector<std::size_t>;

        [[nodiscard]] auto
        operator==(const DenseBitSet &) const -> bool = default;

    private:
        static constexpr std::size_t kWordBits = 64U;

        [[nodiscard]] auto
        Compatible(const DenseBitSet &other) const noexcept -> bool;

        void
        MaskUnusedBits() noexcept;

        std::size_t bitCount_{};
        std::vector<std::uint64_t> words_;
    };
} // namespace Visual::XSharp::Analysis
