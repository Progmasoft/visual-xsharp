// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "Visual/XSharp/Core/CorePrep.hpp"

namespace visual_xsharp::core
{
    // StorageClass is a semantic property, not a target layout. In particular, a CoW
    // value can contain a hidden shared buffer while still obeying value semantics.
    enum class StorageClass : std::uint8_t
    {
        TrivialValue,
        CopyOnWriteValue,
        AarcReference,
        Unresolved
    };

    // The declaration family is carried separately from Type until the complete source
    // declaration catalog is serialized in Core. This prevents a Named type from being
    // guessed from spelling, which would make separate compilation ABI-unsafe.
    enum class NominalKind : std::uint8_t
    {
        Data,
        Type,
        ClassicEnum,
        Class,
        DataClass,
        EnumClass,
        Object,
        Interface
    };

    struct NominalTypeDeclaration final
    {
        std::vector<std::u32string> name;
        NominalKind kind{ NominalKind::Class };

        [[nodiscard]] auto
        operator==(const NominalTypeDeclaration &) const -> bool = default;
    };

    // A type's spelling alone never decides ownership. The catalog is the
    // explicit semantic bridge from a resolved qualified name to its source
    // declaration family. A sorted flat representation provides allocation-
    // free logarithmic lookup, deterministic traversal, and no process-
    // dependent hash function for Unicode names.
    class NominalTypeCatalog final
    {
    public:
        [[nodiscard]] auto
        Register(std::vector<std::u32string> name, NominalKind kind) -> bool;
        [[nodiscard]] auto
        Lookup(std::span<const std::u32string> name) const noexcept -> std::optional<NominalKind>;
        [[nodiscard]] auto
        Size() const noexcept -> std::size_t;
        [[nodiscard]] auto
        Empty() const noexcept -> bool;

    private:
        std::vector<NominalTypeDeclaration> declarations_;
    };

    [[nodiscard]] auto
    ClassifyNominal(NominalKind kind) noexcept -> StorageClass;
    [[nodiscard]] auto
    ClassifyType(const Type &type, std::optional<NominalKind> nominal = std::nullopt) noexcept
        -> StorageClass;
    [[nodiscard]] auto
    ClassifyType(const Type &type, const NominalTypeCatalog &catalog) noexcept -> StorageClass;
    [[nodiscard]] auto
    UsesAarc(const Type &type, std::optional<NominalKind> nominal = std::nullopt) noexcept -> bool;
    [[nodiscard]] auto
    UsesCopyOnWrite(const Type &type, std::optional<NominalKind> nominal = std::nullopt) noexcept
        -> bool;
    [[nodiscard]] auto
    UsesAarc(const Type &type, const NominalTypeCatalog &catalog) noexcept -> bool;
    [[nodiscard]] auto
    UsesCopyOnWrite(const Type &type, const NominalTypeCatalog &catalog) noexcept -> bool;
} // namespace visual_xsharp::core
