// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Visual/XSharp/Backend/LLVM.hpp"
#include "Visual/XSharp/Xmm/IR.hpp"

namespace Visual::XSharp::Interactive::Runtime
{
    class ScratchCell final
    {
    public:
        /** Reserve a private directory for one source/Core exchange with vxs-frontend. */
        ScratchCell();
        ScratchCell(const ScratchCell &) = delete;
        auto
        operator=(const ScratchCell &) -> ScratchCell & = delete;
        ~ScratchCell();
        ScratchCell(ScratchCell &&) = delete;
        auto
        operator=(ScratchCell &&) -> ScratchCell & = delete;

        [[nodiscard]] auto
        Valid() const noexcept -> bool;
        [[nodiscard]] auto
        SourcePath() const noexcept -> const std::filesystem::path &;
        [[nodiscard]] auto
        CorePath() const noexcept -> const std::filesystem::path &;

    private:
        std::filesystem::path directory_;
        std::filesystem::path source_;
        std::filesystem::path core_;
    };

    [[nodiscard]] auto
    WriteCellSource(const ScratchCell &cell,
                    std::uint64_t cellNumber,
                    std::string_view expression,
                    const std::optional<Backend::LLVM::JitValue> &previous) -> std::optional<std::string>;

    [[nodiscard]] auto
    RunFrontend(const std::filesystem::path &source, const std::filesystem::path &core) -> int;

    [[nodiscard]] auto
    ReadCore(const std::filesystem::path &path) -> std::optional<std::vector<std::uint8_t>>;

    [[nodiscard]] auto
    EvaluationSymbol(const visual_xsharp::xmm::Module &module, std::uint64_t cellNumber)
        -> std::optional<std::string>;
} // namespace Visual::XSharp::Interactive::Runtime
