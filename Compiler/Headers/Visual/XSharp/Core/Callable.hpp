// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

#include "Visual/XSharp/Core/CorePrep.hpp"

namespace Visual::XSharp::Core::Callable
{
    // Function Type stores its public parameters followed by its result. This view
    // removes that encoding detail from verifier and backend code while retaining an
    // owning representation that is safe to keep after the source Type is moved.
    struct Signature final
    {
        std::vector<::visual_xsharp::core::Type> parameters;
        ::visual_xsharp::core::Type result{ ::visual_xsharp::core::Type::unit() };

        [[nodiscard]] auto
        operator==(const Signature &) const -> bool = default;
    };

    enum class ClosureContractError
    {
        None,
        ResultIsNotCallable,
        TargetHasTooFewParameters,
        CaptureTypeMismatch,
        PublicParameterCountMismatch,
        PublicParameterTypeMismatch,
        ResultTypeMismatch
    };

    struct ClosureContract final
    {
        ClosureContractError error{ ClosureContractError::None };
        std::size_t index{};
        Signature publicSignature;

        [[nodiscard]] explicit
        operator bool() const noexcept
        {
            return error == ClosureContractError::None;
        }
    };

    [[nodiscard]] auto
    Decompose(const ::visual_xsharp::core::Type &type) -> std::optional<Signature>;

    [[nodiscard]] auto
    Compose(Signature signature) -> ::visual_xsharp::core::Type;

    // A lifted closure target receives hidden capture parameters first, followed by
    // the public callable parameters. Its return type must equal the callable result.
    // Keeping this contract in one place prevents Xpp, Xmm, and LLVM from accepting
    // subtly different closure layouts.
    [[nodiscard]] auto
    ValidateClosure(
        std::span<const ::visual_xsharp::core::Type> captures,
        std::span<const ::visual_xsharp::core::Type> targetParameters,
        const ::visual_xsharp::core::Type &targetResult,
        const ::visual_xsharp::core::Type &publicCallable) -> ClosureContract;
} // namespace Visual::XSharp::Core::Callable
