// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

#include <algorithm>
#include <bit>
#include <limits>
#include <stdexcept>

#include "Visual/XSharp/Analysis/DenseBitSet.hpp"

namespace Visual::XSharp::Analysis
{
    namespace
    {
        [[nodiscard]] auto
        WordCount(const std::size_t bitCount) noexcept -> std::size_t
        {
            constexpr auto kBits = std::numeric_limits<std::uint64_t>::digits;
            return bitCount == 0U ? 0U : ((bitCount - 1U) / kBits) + 1U;
        }
    } // namespace

    DenseBitSet::DenseBitSet(const std::size_t bitCount, const bool value)
        : bitCount_(bitCount)
        , words_(WordCount(bitCount), value ? ~std::uint64_t{} : std::uint64_t{})
    {
        MaskUnusedBits();
    }

    auto
    DenseBitSet::Size() const noexcept -> std::size_t
    {
        return bitCount_;
    }

    auto
    DenseBitSet::Empty() const noexcept -> bool
    {
        return bitCount_ == 0U;
    }

    auto
    DenseBitSet::None() const noexcept -> bool
    {
        return std::ranges::all_of(words_, [](const auto word) {
            return word == 0U;
        });
    }

    auto
    DenseBitSet::Any() const noexcept -> bool
    {
        return !None();
    }

    auto
    DenseBitSet::Count() const noexcept -> std::size_t
    {
        std::size_t count{};
        for (const auto word : words_)
            count += std::popcount(word);
        return count;
    }

    auto
    DenseBitSet::Test(const std::size_t index) const noexcept -> bool
    {
        if (index >= bitCount_)
            return false;
        const auto word = index / kWordBits;
        const auto bit = index % kWordBits;
        return (words_[word] & (std::uint64_t{ 1U } << bit)) != 0U;
    }

    void
    DenseBitSet::Set(const std::size_t index) noexcept
    {
        if (index >= bitCount_)
            return;
        words_[index / kWordBits] |= std::uint64_t{ 1U } << (index % kWordBits);
    }

    void
    DenseBitSet::Reset(const std::size_t index) noexcept
    {
        if (index >= bitCount_)
            return;
        words_[index / kWordBits] &= ~(std::uint64_t{ 1U } << (index % kWordBits));
    }

    void
    DenseBitSet::Assign(const std::size_t index, const bool value) noexcept
    {
        if (value)
            Set(index);
        else
            Reset(index);
    }

    void
    DenseBitSet::Clear() noexcept
    {
        std::ranges::fill(words_, std::uint64_t{});
    }

    void
    DenseBitSet::Fill() noexcept
    {
        std::ranges::fill(words_, ~std::uint64_t{});
        MaskUnusedBits();
    }

    void
    DenseBitSet::UnionWith(const DenseBitSet &other)
    {
        if (!Compatible(other))
            throw std::invalid_argument("cannot union dense bit sets with different sizes");
        for (std::size_t index = 0U; index < words_.size(); ++index)
            words_[index] |= other.words_[index];
    }

    void
    DenseBitSet::IntersectWith(const DenseBitSet &other)
    {
        if (!Compatible(other))
            throw std::invalid_argument("cannot intersect dense bit sets with different sizes");
        for (std::size_t index = 0U; index < words_.size(); ++index)
            words_[index] &= other.words_[index];
    }

    void
    DenseBitSet::Subtract(const DenseBitSet &other)
    {
        if (!Compatible(other))
            throw std::invalid_argument("cannot subtract dense bit sets with different sizes");
        for (std::size_t index = 0U; index < words_.size(); ++index)
            words_[index] &= ~other.words_[index];
        MaskUnusedBits();
    }

    auto
    DenseBitSet::SetIndices() const -> std::vector<std::size_t>
    {
        std::vector<std::size_t> indices;
        indices.reserve(Count());
        for (std::size_t wordIndex = 0U; wordIndex < words_.size(); ++wordIndex)
        {
            auto word = words_[wordIndex];
            while (word != 0U)
            {
                const auto bit = static_cast<std::size_t>(std::countr_zero(word));
                indices.push_back(wordIndex * kWordBits + bit);
                word &= word - 1U;
            }
        }
        return indices;
    }

    auto
    DenseBitSet::Compatible(const DenseBitSet &other) const noexcept -> bool
    {
        return bitCount_ == other.bitCount_;
    }

    void
    DenseBitSet::MaskUnusedBits() noexcept
    {
        if (words_.empty() || bitCount_ % kWordBits == 0U)
            return;
        const auto used = bitCount_ % kWordBits;
        words_.back() &= (std::uint64_t{ 1U } << used) - 1U;
    }
} // namespace Visual::XSharp::Analysis
