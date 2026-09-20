// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

#include <limits>
#include <stdexcept>

#include "Visual/XSharp/Analysis/DenseBitSet.hpp"

namespace Visual::XSharp::Analysis
{
    namespace
    {
        [[nodiscard]] auto
        CheckedBitCount(const std::size_t bitCount) -> unsigned
        {
            if (bitCount > std::numeric_limits<unsigned>::max())
                throw std::length_error("dense bit set exceeds LLVM BitVector's index domain");
            return static_cast<unsigned>(bitCount);
        }
    } // namespace

    DenseBitSet::DenseBitSet(const std::size_t bitCount, const bool value)
        : bits_(CheckedBitCount(bitCount), value)
    {
    }

    auto
    DenseBitSet::Size() const noexcept -> std::size_t
    {
        return bits_.size();
    }

    auto
    DenseBitSet::Empty() const noexcept -> bool
    {
        return bits_.empty();
    }

    auto
    DenseBitSet::None() const noexcept -> bool
    {
        return bits_.none();
    }

    auto
    DenseBitSet::Any() const noexcept -> bool
    {
        return bits_.any();
    }

    auto
    DenseBitSet::Count() const noexcept -> std::size_t
    {
        return bits_.count();
    }

    auto
    DenseBitSet::Test(const std::size_t index) const noexcept -> bool
    {
        return index < bits_.size() && bits_.test(static_cast<unsigned>(index));
    }

    void
    DenseBitSet::Set(const std::size_t index) noexcept
    {
        if (index < bits_.size())
            bits_.set(static_cast<unsigned>(index));
    }

    void
    DenseBitSet::Reset(const std::size_t index) noexcept
    {
        if (index < bits_.size())
            bits_.reset(static_cast<unsigned>(index));
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
        bits_.reset();
    }

    void
    DenseBitSet::Fill() noexcept
    {
        bits_.set();
    }

    void
    DenseBitSet::UnionWith(const DenseBitSet &other)
    {
        if (!Compatible(other))
            throw std::invalid_argument("cannot union dense bit sets with different sizes");
        bits_ |= other.bits_;
    }

    void
    DenseBitSet::IntersectWith(const DenseBitSet &other)
    {
        if (!Compatible(other))
            throw std::invalid_argument("cannot intersect dense bit sets with different sizes");
        bits_ &= other.bits_;
    }

    void
    DenseBitSet::Subtract(const DenseBitSet &other)
    {
        if (!Compatible(other))
            throw std::invalid_argument("cannot subtract dense bit sets with different sizes");
        bits_.reset(other.bits_);
    }

    auto
    DenseBitSet::SetIndices() const -> std::vector<std::size_t>
    {
        std::vector<std::size_t> indices;
        for (const auto index : bits_.set_bits())
            indices.push_back(index);
        return indices;
    }

    auto
    DenseBitSet::Compatible(const DenseBitSet &other) const noexcept -> bool
    {
        return bits_.size() == other.bits_.size();
    }
} // namespace Visual::XSharp::Analysis
