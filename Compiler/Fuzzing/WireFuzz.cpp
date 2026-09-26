// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

#include <stdexcept>
#include <utility>

#include "Visual/XSharp/Core/CorePrep/Wire.hpp"
#include "Visual/XSharp/Core/Wire.hpp"
#include "Visual/XSharp/Xmm/Wire.hpp"
#include "Visual/XSharp/Xpp/Wire.hpp"
#include "WireFuzz.hpp"

namespace Visual::XSharp::Fuzzing
{
    namespace
    {
        constexpr std::size_t kMaximumInputBytes = 16U * 1024U;

        [[nodiscard]] auto
        CoreLimits() -> Core::Wire::Limits
        {
            Core::Wire::Limits limits;
            limits.maximumWireBytes = kMaximumInputBytes;
            limits.maximumTextScalars = 256U;
            limits.maximumFunctions = 32U;
            limits.maximumParameters = 32U;
            limits.maximumStatements = 128U;
            limits.maximumOperands = 32U;
            limits.maximumTypeDepth = 16U;
            limits.maximumExpressionDepth = 32U;
            limits.maximumNumericBytes = 128U;
            return limits;
        }

        [[nodiscard]] auto
        CorePrepLimits() -> ::visual_xsharp::core::wire::Limits
        {
            ::visual_xsharp::core::wire::Limits limits;
            limits.maximum_wire_bytes = kMaximumInputBytes;
            limits.maximum_string_code_points = 256U;
            limits.maximum_functions = 32U;
            limits.maximum_parameters_per_function = 32U;
            limits.maximum_blocks_per_function = 64U;
            limits.maximum_instructions_per_block = 128U;
            limits.maximum_operands_per_instruction = 32U;
            limits.maximum_type_depth = 16U;
            limits.maximum_numeric_bytes = 128U;
            return limits;
        }

        [[nodiscard]] auto
        ArtifactLimits() -> Artifact::Wire::Limits
        {
            Artifact::Wire::Limits limits;
            limits.maximumWireBytes = kMaximumInputBytes;
            limits.maximumTextScalars = 256U;
            limits.maximumFunctions = 32U;
            limits.maximumParameters = 32U;
            limits.maximumBlocks = 64U;
            limits.maximumInstructions = 128U;
            limits.maximumOperands = 32U;
            limits.maximumTypeDepth = 16U;
            limits.maximumNumericBytes = 128U;
            return limits;
        }

        void
        CheckCore(std::span<const std::uint8_t> bytes)
        {
            const auto limits = CoreLimits();
            const auto decoded = Core::Wire::Decode(bytes, limits);
            if (!decoded)
                return;
            const auto encoded = Core::Wire::Encode(*decoded.module, limits);
            if (!encoded)
                return; // A structural decode need not be semantically valid.
            const auto again = Core::Wire::Decode(encoded.bytes, limits);
            if (!again || *again.module != *decoded.module)
                throw std::logic_error(
                    "Core wire round trip changed the model");
        }

        void
        CheckCorePrep(std::span<const std::uint8_t> bytes)
        {
            const auto limits = CorePrepLimits();
            const auto decoded
                = ::visual_xsharp::core::wire::decode(bytes, limits);
            if (!decoded)
                return;
            const auto encoded
                = ::visual_xsharp::core::wire::encode(*decoded.module, limits);
            if (!encoded)
                return;
            const auto again
                = ::visual_xsharp::core::wire::decode(encoded.bytes, limits);
            if (!again || *again.module != *decoded.module)
                throw std::logic_error(
                    "CorePrep wire round trip changed the model");
        }

        void
        CheckXpp(std::span<const std::uint8_t> bytes)
        {
            const auto limits = ArtifactLimits();
            const auto decoded = Xpp::Wire::Decode(bytes, limits);
            if (!decoded)
                return;
            const auto encoded = Xpp::Wire::Encode(*decoded.module, limits);
            if (!encoded)
                return;
            const auto again = Xpp::Wire::Decode(encoded.bytes, limits);
            if (!again || *again.module != *decoded.module)
                throw std::logic_error("Xpp wire round trip changed the model");
        }

        void
        CheckXmm(std::span<const std::uint8_t> bytes)
        {
            const auto limits = ArtifactLimits();
            const auto decoded = Xmm::Wire::Decode(bytes, limits);
            if (!decoded)
                return;
            const auto encoded = Xmm::Wire::Encode(*decoded.module, limits);
            if (!encoded)
                return;
            const auto again = Xmm::Wire::Decode(encoded.bytes, limits);
            if (!again || *again.module != *decoded.module)
                throw std::logic_error("Xmm wire round trip changed the model");
        }

        [[nodiscard]] auto
        WithSelector(std::uint8_t selector, std::vector<std::uint8_t> bytes)
            -> std::vector<std::uint8_t>
        {
            bytes.insert(bytes.begin(), selector);
            return bytes;
        }
    } // namespace

    void
    ExerciseWire(std::span<const std::uint8_t> input)
    {
        if (input.empty() || input.size() > kMaximumInputBytes + 1U)
            return;
        const auto bytes = input.subspan(1U);
        switch (input.front() % 4U)
        {
            case 0U:
                CheckCore(bytes);
                break;
            case 1U:
                CheckCorePrep(bytes);
                break;
            case 2U:
                CheckXpp(bytes);
                break;
            default:
                CheckXmm(bytes);
                break;
        }
    }

    auto
    WireSeeds() -> std::vector<std::vector<std::uint8_t>>
    {
        // Valid, small documents let mutations reach counts, names, and the
        // versioned body rather than spending every case on magic rejection.
        const Core::Module core{ { U"Demo" }, {} };
        const ::visual_xsharp::core::CorePrepModule corePrep{ { U"Demo" }, {} };
        ::visual_xsharp::xpp::Function xppFunction;
        xppFunction.symbol = { 1U, U"Main" };
        xppFunction.entry = 0U;
        ::visual_xsharp::xpp::Terminator xppReturn;
        xppReturn.kind = ::visual_xsharp::xpp::Terminator::Kind::Return;
        xppReturn.value = { ::visual_xsharp::xpp::Operand::Kind::Literal,
                            Core::Type::unit(),
                            0U,
                            std::monostate{} };
        xppFunction.blocks.push_back({ 0U, {}, xppReturn });
        const ::visual_xsharp::xpp::Module xpp{ { U"Demo" },
                                                { std::move(xppFunction) } };

        ::visual_xsharp::xmm::Function xmmFunction;
        xmmFunction.symbol = { 1U, U"Main" };
        xmmFunction.entry = 0U;
        ::visual_xsharp::xmm::Terminator xmmReturn;
        xmmReturn.kind = ::visual_xsharp::xmm::Terminator::Kind::Return;
        xmmReturn.value = { ::visual_xsharp::xmm::Value::Kind::Immediate,
                            Core::Type::unit(),
                            0U,
                            0U,
                            std::monostate{} };
        xmmFunction.blocks.push_back({ 0U, {}, xmmReturn });
        const ::visual_xsharp::xmm::Module xmm{ { U"Demo" },
                                                { std::move(xmmFunction) } };
        const auto coreBytes = Core::Wire::Encode(core, CoreLimits());
        const auto corePrepBytes
            = ::visual_xsharp::core::wire::encode(corePrep, CorePrepLimits());
        const auto xppBytes = Xpp::Wire::Encode(xpp, ArtifactLimits());
        const auto xmmBytes = Xmm::Wire::Encode(xmm, ArtifactLimits());
        if (!coreBytes)
            throw std::logic_error("Could not create Core wire fuzz seed: "
                                   + coreBytes.error->message);
        if (!corePrepBytes)
            throw std::logic_error("Could not create CorePrep wire fuzz seed: "
                                   + corePrepBytes.error->message);
        if (!xppBytes)
            throw std::logic_error("Could not create Xpp wire fuzz seed: "
                                   + xppBytes.error->message);
        if (!xmmBytes)
            throw std::logic_error("Could not create Xmm wire fuzz seed: "
                                   + xmmBytes.error->message);
        return { WithSelector(0U, coreBytes.bytes),
                 WithSelector(1U, corePrepBytes.bytes),
                 WithSelector(2U, xppBytes.bytes),
                 WithSelector(3U, xmmBytes.bytes) };
    }
} // namespace Visual::XSharp::Fuzzing
