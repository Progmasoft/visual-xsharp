// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1
#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/Hashing.h>
#include <utility>

namespace Visual::XSharp::ADTs
{
    // LLVM's integral DenseMapInfo reserves two values for empty and tombstone
    // buckets. Compiler identities are wire-level values, so silently excluding
    // two otherwise valid IDs would make validation depend on the container.
    // DenseIdMap moves bucket state into a tagged key and preserves the complete
    // unsigned identity domain, including zero and the two largest values.
    template<std::unsigned_integral Id, typename Value>
    class DenseIdMap final
    {
        enum class Slot : std::uint8_t
        {
            Normal,
            Empty,
            Tombstone
        };

        struct Key final
        {
            Id value{};
            Slot slot{ Slot::Normal };

            [[nodiscard]] static auto
            Normal(const Id value) noexcept -> Key
            {
                return { value, Slot::Normal };
            }

            [[nodiscard]] auto
            operator==(const Key &other) const noexcept -> bool
            {
                return slot == other.slot && (slot != Slot::Normal || value == other.value);
            }
        };

        struct KeyInfo final
        {
            [[nodiscard]] static inline auto
            getEmptyKey() noexcept -> Key
            {
                return { Id{}, Slot::Empty };
            }

            [[nodiscard]] static inline auto
            getTombstoneKey() noexcept -> Key
            {
                return { Id{}, Slot::Tombstone };
            }

            [[nodiscard]] static auto
            getHashValue(const Key &key) noexcept -> unsigned
            {
                return static_cast<unsigned>(llvm::hash_value(key.value));
            }

            [[nodiscard]] static auto
            isEqual(const Key &left, const Key &right) noexcept -> bool
            {
                return left == right;
            }
        };

        using Storage = llvm::DenseMap<Key, Value, KeyInfo>;

    public:
        struct InsertResult final
        {
            Value *value{};
            bool inserted{};
        };

        [[nodiscard]] auto
        Empty() const noexcept -> bool
        {
            return values_.empty();
        }

        [[nodiscard]] auto
        Size() const noexcept -> std::size_t
        {
            return values_.size();
        }

        void
        Reserve(const std::size_t count)
        {
            values_.reserve(count);
        }

        [[nodiscard]] auto
        Contains(const Id id) const -> bool
        {
            return values_.contains(Key::Normal(id));
        }

        [[nodiscard]] auto
        Find(const Id id) -> Value *
        {
            const auto found = values_.find(Key::Normal(id));
            return found == values_.end() ? nullptr : &found->second;
        }

        [[nodiscard]] auto
        Find(const Id id) const -> const Value *
        {
            const auto found = values_.find(Key::Normal(id));
            return found == values_.end() ? nullptr : &found->second;
        }

        template<typename... Arguments>
        auto
        TryEmplace(const Id id, Arguments &&...arguments) -> InsertResult
        {
            auto [found, inserted] = values_.try_emplace(
                Key::Normal(id),
                std::forward<Arguments>(arguments)...);
            return { &found->second, inserted };
        }

        void
        InsertOrAssign(const Id id, Value value)
        {
            if (auto *stored = Find(id))
            {
                *stored = std::move(value);
                return;
            }
            static_cast<void>(TryEmplace(id, std::move(value)));
        }

        template<typename Visitor>
        void
        ForEach(Visitor &&visitor)
        {
            for (auto &[key, value] : values_)
                visitor(key.value, value);
        }

        template<typename Visitor>
        void
        ForEach(Visitor &&visitor) const
        {
            for (const auto &[key, value] : values_)
                visitor(key.value, value);
        }

    private:
        Storage values_;
    };

    template<std::unsigned_integral Id>
    class DenseIdSet final
    {
    public:
        [[nodiscard]] auto
        Empty() const noexcept -> bool
        {
            return values_.Empty();
        }

        [[nodiscard]] auto
        Size() const noexcept -> std::size_t
        {
            return values_.Size();
        }

        void
        Reserve(const std::size_t count)
        {
            values_.Reserve(count);
        }

        [[nodiscard]] auto
        Contains(const Id id) const -> bool
        {
            return values_.Contains(id);
        }

        [[nodiscard]] auto
        Insert(const Id id) -> bool
        {
            return values_.TryEmplace(id, std::uint8_t{}).inserted;
        }

        template<typename Visitor>
        void
        ForEach(Visitor &&visitor) const
        {
            values_.ForEach([&visitor](const Id id, const std::uint8_t) {
                visitor(id);
            });
        }

    private:
        DenseIdMap<Id, std::uint8_t> values_;
    };
} // namespace Visual::XSharp::ADTs
