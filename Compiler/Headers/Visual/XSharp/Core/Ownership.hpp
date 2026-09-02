// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
#pragma once

#include <cstdint>
#include <optional>

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

    [[nodiscard]] auto
    ClassifyNominal(NominalKind kind) noexcept -> StorageClass;
    [[nodiscard]] auto
    ClassifyType(const Type &type, std::optional<NominalKind> nominal = std::nullopt) noexcept
        -> StorageClass;
    [[nodiscard]] auto
    UsesAarc(const Type &type, std::optional<NominalKind> nominal = std::nullopt) noexcept -> bool;
    [[nodiscard]] auto
    UsesCopyOnWrite(const Type &type, std::optional<NominalKind> nominal = std::nullopt) noexcept
        -> bool;
} // namespace visual_xsharp::core
