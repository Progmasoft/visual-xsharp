// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "Visual/XSharp/Core/CorePrep.hpp"
#include "Visual/XSharp/Namespace.hpp"

namespace Visual::XSharp::Core::Template
{
    using TypeBindingMap = std::unordered_map<::visual_xsharp::core::SymbolId, ::visual_xsharp::core::Type>;
    using ValueBindingMap = std::unordered_map<::visual_xsharp::core::SymbolId, ::visual_xsharp::core::TemplateValue>;

    enum class IssueKind : std::uint8_t
    {
        DepthExceeded,
        EmptyQualifiedName,
        EmptyNamePart,
        InvalidTypeArgument,
        InvalidValueParameter,
        UnboundParameter,
        InvalidInteger,
        InvalidCharacter,
        NegativeArraySize,
        MalformedArrayFamily
    };

    struct Issue final
    {
        IssueKind kind{ IssueKind::InvalidTypeArgument };
        std::vector<std::size_t> path;
        std::string message;

        [[nodiscard]] auto
        operator==(const Issue &) const -> bool = default;
    };

    struct Metrics final
    {
        std::size_t typeNodes{};
        std::size_t typeArguments{};
        std::size_t valueArguments{};
        std::size_t parameterReferences{};
        std::size_t maximumDepth{};

        [[nodiscard]] auto
        operator==(const Metrics &) const -> bool = default;
    };

    struct ArrayShape final
    {
        enum class Kind : std::uint8_t
        {
            Builtin,
            Dynamic,
            Fixed
        };

        Kind kind{ Kind::Builtin };
        ::visual_xsharp::core::Type element;
        std::optional<::visual_xsharp::core::IntegerLiteral> size;

        [[nodiscard]] auto
        operator==(const ArrayShape &) const -> bool = default;
    };

    // Identify the three array spellings without inventing a runtime class for
    // built-in []T or a second class for fixed System.Array<T, N>.
    [[nodiscard]] auto
    ClassifyArray(const ::visual_xsharp::core::Type &type) -> std::optional<ArrayShape>;

    // Validate the structural specialization key before an artifact writer or
    // monomorphization cache accepts it. Paths index ordered template/function
    // components from the root and are stable enough for diagnostics.
    [[nodiscard]] auto
    Validate(const ::visual_xsharp::core::Type &type, std::size_t maximumDepth = 128U)
        -> std::vector<Issue>;

    [[nodiscard]] auto
    Measure(const ::visual_xsharp::core::Type &type) -> Metrics;
    [[nodiscard]] auto
    CollectParameters(const ::visual_xsharp::core::Type &type)
        -> std::vector<::visual_xsharp::core::SymbolId>;
    [[nodiscard]] auto
    IsConcrete(const ::visual_xsharp::core::Type &type) -> bool;

    // Substitute type and value parameters independently. Missing bindings are
    // deliberately preserved so callers can perform partial specialization.
    [[nodiscard]] auto
    Substitute(const ::visual_xsharp::core::Type &type,
               const TypeBindingMap &typeBindings,
               const ValueBindingMap &valueBindings)
        -> ::visual_xsharp::core::Type;

    // The identity is human-readable but length-prefixed and kind-tagged. It is
    // suitable as a deterministic cache key, never as a user-facing type name.
    [[nodiscard]] auto
    RenderIdentity(const ::visual_xsharp::core::Type &type) -> std::string;

    using SpecializationId = std::uint64_t;

    struct Specialization final
    {
        SpecializationId id{};
        std::string identity;
        ::visual_xsharp::core::Type type;

        [[nodiscard]] auto
        operator==(const Specialization &) const -> bool = default;
    };

    struct InternResult final
    {
        std::optional<Specialization> specialization;
        std::vector<Issue> issues;
        bool inserted{};

        [[nodiscard]] explicit
        operator bool() const noexcept
        {
            return specialization.has_value();
        }
    };

    // The table is safe to share between parallel frontend jobs. A read pass is
    // attempted before the exclusive insertion pass; the identity is checked
    // again after acquiring the writer lock to coalesce races deterministically.
    class SpecializationTable final
    {
    public:
        SpecializationTable() = default;
        SpecializationTable(const SpecializationTable &) = delete;
        auto
        operator=(const SpecializationTable &) -> SpecializationTable & = delete;

        [[nodiscard]] auto
        Intern(const ::visual_xsharp::core::Type &type) -> InternResult;
        [[nodiscard]] auto
        Find(SpecializationId id) const -> std::optional<Specialization>;
        [[nodiscard]] auto
        Find(const ::visual_xsharp::core::Type &type) const -> std::optional<Specialization>;
        [[nodiscard]] auto
        Snapshot() const -> std::vector<Specialization>;
        [[nodiscard]] auto
        Size() const -> std::size_t;

    private:
        mutable std::shared_mutex mutex_;
        std::unordered_map<std::string, SpecializationId> byIdentity_;
        std::vector<Specialization> entries_;
    };
} // namespace Visual::XSharp::Core::Template
