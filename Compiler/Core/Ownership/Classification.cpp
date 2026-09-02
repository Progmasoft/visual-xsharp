// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

#include "Visual/XSharp/Core/Ownership.hpp"

namespace visual_xsharp::core
{
    auto
    ClassifyNominal(NominalKind kind) noexcept -> StorageClass
    {
        switch (kind)
        {
            case NominalKind::Data:
            case NominalKind::Type:
            case NominalKind::ClassicEnum:
                return StorageClass::CopyOnWriteValue;
            case NominalKind::Class:
            case NominalKind::DataClass:
            case NominalKind::EnumClass:
            case NominalKind::Object:
            case NominalKind::Interface:
                return StorageClass::AarcReference;
        }
        return StorageClass::Unresolved;
    }

    auto
    ClassifyType(const Type &type, std::optional<NominalKind> nominal) noexcept -> StorageClass
    {
        switch (type.kind)
        {
            case Type::Kind::Unit:
            case Type::Kind::Bool:
            case Type::Kind::Character:
            case Type::Kind::Int8:
            case Type::Kind::Int16:
            case Type::Kind::Int32:
            case Type::Kind::Int64:
            case Type::Kind::Int128:
            case Type::Kind::UInt8:
            case Type::Kind::UInt16:
            case Type::Kind::UInt32:
            case Type::Kind::UInt64:
            case Type::Kind::UInt128:
            case Type::Kind::Float16:
            case Type::Kind::Float32:
            case Type::Kind::Float64:
            case Type::Kind::Float128:
                return StorageClass::TrivialValue;

            // String is nullable and identity-bearing in the language contract. Its
            // character storage may itself use CoW, but the String value is an AARC ref.
            case Type::Kind::String:
            case Type::Kind::Function:
                return StorageClass::AarcReference;

            case Type::Kind::Named:
                return nominal ? ClassifyNominal(*nominal) : StorageClass::Unresolved;
            case Type::Kind::TypeVariable:
                return StorageClass::Unresolved;
        }
        return StorageClass::Unresolved;
    }

    auto
    UsesAarc(const Type &type, std::optional<NominalKind> nominal) noexcept -> bool
    {
        return ClassifyType(type, nominal) == StorageClass::AarcReference;
    }

    auto
    UsesCopyOnWrite(const Type &type, std::optional<NominalKind> nominal) noexcept -> bool
    {
        return ClassifyType(type, nominal) == StorageClass::CopyOnWriteValue;
    }
} // namespace visual_xsharp::core
