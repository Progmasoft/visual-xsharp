// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

#include <benchmark/benchmark.h>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "Visual/XSharp/Core/CorePrep/Prepare.hpp"
#include "Visual/XSharp/Core/IR.hpp"
#include "Visual/XSharp/Core/Verifier.hpp"
#include "Visual/XSharp/Core/Wire.hpp"

namespace
{
    namespace Core = Visual::XSharp::Core;

    [[nodiscard]] auto
    Symbol(std::uint64_t id, std::u32string spelling) -> Core::SymbolName
    {
        return { id, std::move(spelling) };
    }

    [[nodiscard]] auto
    Integer(std::int64_t value) -> Core::Expression
    {
        return Core::Expression::Constant(value, Core::Type::int64());
    }

    [[nodiscard]] auto
    Variable(std::uint64_t id) -> Core::Expression
    {
        return Core::Expression::Variable(Symbol(id, U"value"),
                                          Core::Type::int64());
    }

    // Each generated function is independently valid and has the same shape.
    // Scaling the function count therefore grows both symbol tables and the
    // recursive statement/expression work without changing benchmark meaning.
    [[nodiscard]] auto
    MakeModule(std::size_t functionCount) -> Core::Module
    {
        std::vector<Core::Function> functions;
        functions.reserve(functionCount);
        for (std::size_t index = 0; index < functionCount; ++index)
        {
            const auto base = static_cast<std::uint64_t>(index * 4U + 1U);
            auto sum = Core::Expression::InvokePrimitive(
                Core::Primitive::Add,
                { Integer(static_cast<std::int64_t>(index)), Integer(1) },
                Core::Type::int64());
            auto comparison = Core::Expression::InvokePrimitive(
                Core::Primitive::GreaterEqual,
                { Variable(base + 1U), Integer(1) },
                Core::Type::boolean());
            std::vector<Core::Statement> body;
            body.emplace_back(
                Core::Statement::Bind({ Symbol(base + 1U, U"value"),
                                        Core::Type::int64(),
                                        true,
                                        std::move(sum) }));
            body.emplace_back(Core::Statement::If(
                std::move(comparison),
                std::vector<Core::Statement>{
                    Core::Statement::Assign(Symbol(base + 1U, U"value"),
                                            Integer(2)) },
                std::vector<Core::Statement>{
                    Core::Statement::Assign(Symbol(base + 1U, U"value"),
                                            Integer(0)) }));
            body.emplace_back(Core::Statement::Return(Variable(base + 1U)));
            functions.push_back({ Symbol(base, U"Function"),
                                  {},
                                  Core::Type::int64(),
                                  std::move(body) });
        }
        return { { U"Benchmark" }, std::move(functions) };
    }

    void
    RequireValid(const Core::Module &module)
    {
        if (!Core::Verify(module).empty())
            throw std::runtime_error("Core benchmark fixture is not valid");
    }

    void
    CoreVerify(benchmark::State &state)
    {
        const auto module
            = MakeModule(static_cast<std::size_t>(state.range(0)));
        RequireValid(module);
        for (auto _ : state)
        {
            const auto issues = Core::Verify(module);
            benchmark::DoNotOptimize(issues.data());
            benchmark::DoNotOptimize(issues.size());
        }
        state.SetItemsProcessed(state.iterations() * state.range(0));
        state.SetComplexityN(state.range(0));
    }

    void
    CoreEncode(benchmark::State &state)
    {
        const auto module
            = MakeModule(static_cast<std::size_t>(state.range(0)));
        RequireValid(module);
        for (auto _ : state)
        {
            const auto encoded = Core::Wire::Encode(module);
            benchmark::DoNotOptimize(encoded.bytes.data());
            benchmark::DoNotOptimize(encoded.bytes.size());
        }
        state.SetItemsProcessed(state.iterations() * state.range(0));
        state.SetComplexityN(state.range(0));
    }

    void
    CoreDecode(benchmark::State &state)
    {
        const auto module
            = MakeModule(static_cast<std::size_t>(state.range(0)));
        RequireValid(module);
        const auto encoded = Core::Wire::Encode(module);
        if (!encoded)
            throw std::runtime_error("benchmark fixture could not be encoded");
        for (auto _ : state)
        {
            const auto decoded = Core::Wire::Decode(encoded.bytes);
            benchmark::DoNotOptimize(decoded.module.has_value());
            benchmark::DoNotOptimize(
                decoded.module ? decoded.module->functions.data() : nullptr);
        }
        state.SetBytesProcessed(
            state.iterations()
            * static_cast<std::int64_t>(encoded.bytes.size()));
        state.SetItemsProcessed(state.iterations() * state.range(0));
        state.SetComplexityN(state.range(0));
    }

    void
    CorePrepare(benchmark::State &state)
    {
        const auto module
            = MakeModule(static_cast<std::size_t>(state.range(0)));
        RequireValid(module);
        for (auto _ : state)
        {
            const auto prepared = Core::CorePrep::Prepare(module);
            benchmark::DoNotOptimize(prepared.functions.data());
            benchmark::DoNotOptimize(prepared.functions.size());
        }
        state.SetItemsProcessed(state.iterations() * state.range(0));
        state.SetComplexityN(state.range(0));
    }

    constexpr auto kMinimumFunctions = 8;
    constexpr auto kMaximumFunctions = 512;
} // namespace

BENCHMARK(CoreVerify)
    ->RangeMultiplier(4)
    ->Range(kMinimumFunctions, kMaximumFunctions)
    ->Complexity();
BENCHMARK(CoreEncode)
    ->RangeMultiplier(4)
    ->Range(kMinimumFunctions, kMaximumFunctions)
    ->Complexity();
BENCHMARK(CoreDecode)
    ->RangeMultiplier(4)
    ->Range(kMinimumFunctions, kMaximumFunctions)
    ->Complexity();
BENCHMARK(CorePrepare)
    ->RangeMultiplier(4)
    ->Range(kMinimumFunctions, kMaximumFunctions)
    ->Complexity();
