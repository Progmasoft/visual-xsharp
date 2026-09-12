// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1
#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "Visual/XSharp/Diagnostic/Protocol.hpp"

namespace Visual::XSharp::Diagnostic
{
    enum class AppendStatus
    {
        Added,
        Duplicate,
        LimitExceeded,
        Invalid
    };

    struct AppendResult final
    {
        AppendStatus status{ AppendStatus::Invalid };
        std::optional<Error> error;

        [[nodiscard]] explicit
        operator bool() const noexcept
        {
            return status == AppendStatus::Added || status == AppendStatus::Duplicate;
        }
    };

    // Collection is the common accumulation boundary for compiler stages. It
    // preserves first-emission order, coalesces byte-identical records, and
    // validates every record through the same rules as the wire encoder.
    class Collection final
    {
    public:
        explicit Collection(Limits limits = {});

        [[nodiscard]] auto
        Append(Record record) -> AppendResult;

        // Merge is transactional: an invalid record or capacity overflow leaves
        // the receiving collection unchanged. Duplicates remain successful.
        [[nodiscard]] auto
        Merge(const Document &document) -> AppendResult;

        [[nodiscard]] auto
        Size() const noexcept -> std::size_t;

        [[nodiscard]] auto
        Empty() const noexcept -> bool;

        [[nodiscard]] auto
        ErrorCount() const noexcept -> std::size_t;

        [[nodiscard]] auto
        WarningCount() const noexcept -> std::size_t;

        [[nodiscard]] auto
        Snapshot() const -> Document;

        // Take transfers the records and restores a reusable empty collection.
        [[nodiscard]] auto
        Take() -> Document;

        void
        Clear() noexcept;

    private:
        Limits limits_;
        std::vector<Record> records_;
        std::unordered_set<std::string> identities_;
        std::size_t errorCount_{};
        std::size_t warningCount_{};

        [[nodiscard]] auto
        Identity(const Record &record) const -> EncodeResult;
    };
} // namespace Visual::XSharp::Diagnostic
