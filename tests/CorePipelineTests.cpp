// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

#include "Visual/XSharp/Core/CorePrep/Prepare.hpp"
#include "Visual/XSharp/Core/Verifier.hpp"
#include "Visual/XSharp/Core/Wire.hpp"
#include "Visual/XSharp/Pipeline.hpp"
#include "xs/sources/driver/CorePipeline.hpp"

#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <ranges>
#include <sstream>

namespace
{
namespace Core = Visual::XSharp::Core;

[[nodiscard]] auto GoldenModule() -> Core::Module
{
    return {{U"Demo"},
            {{{1U, U"Main"},
              {},
              Core::Type::unit(),
              {Core::Statement::Return(Core::Expression::Constant(std::monostate{}, Core::Type::unit()))}}}};
}

[[nodiscard]] auto PipelineModule() -> Core::Module
{
    const auto integer = [](std::int64_t value) { return Core::Expression::Constant(value, Core::Type::int64()); };
    const auto variable = [](std::uint64_t id, std::u32string spelling, Core::Type type)
    { return Core::Expression::Variable({id, std::move(spelling)}, std::move(type)); };
    const auto sumType = Core::Type::function({Core::Type::int64(), Core::Type::int64()}, Core::Type::int64());
    Core::Function sum{{10U, U"Sum"},
                       {{{11U, U"left"}, Core::Type::int64()}, {{12U, U"right"}, Core::Type::int64()}},
                       Core::Type::int64(),
                       {Core::Statement::Return(Core::Expression::InvokePrimitive(
                           Core::Primitive::Add,
                           {variable(11U, U"left", Core::Type::int64()), variable(12U, U"right", Core::Type::int64())},
                           Core::Type::int64()))}};

    auto call =
        Core::Expression::Apply(variable(10U, U"Sum", sumType), {integer(20), integer(22)}, Core::Type::int64());
    auto condition = Core::Expression::InvokePrimitive(Core::Primitive::GreaterEqual,
                                                       {variable(21U, U"value", Core::Type::int64()), integer(40)},
                                                       Core::Type::boolean());
    Core::Function main{
        {20U, U"Main"},
        {},
        Core::Type::unit(),
        {Core::Statement::Bind({{21U, U"value"}, Core::Type::int64(), true, std::move(call)}),
         Core::Statement::If(
             std::move(condition),
             {Core::Statement::Assign({21U, U"value"}, Core::Expression::InvokePrimitive(
                                                           Core::Primitive::Add,
                                                           {variable(21U, U"value", Core::Type::int64()), integer(1)},
                                                           Core::Type::int64()))},
             {Core::Statement::Assign({21U, U"value"}, integer(0))}),
         Core::Statement::Return(Core::Expression::Constant(std::monostate{}, Core::Type::unit()))}};
    return {{U"Name"}, {std::move(sum), std::move(main)}};
}

[[nodiscard]] auto ReadGoldenHex() -> std::vector<std::uint8_t>
{
    std::ifstream stream(XS_CORE_GOLDEN_PATH);
    REQUIRE(stream);
    std::vector<std::uint8_t> bytes;
    std::string line;
    while(std::getline(stream, line))
    {
        if(const auto comment = line.find('#'); comment != std::string::npos)
            line.erase(comment);
        std::istringstream tokens(line);
        std::string token;
        while(tokens >> token)
            bytes.push_back(static_cast<std::uint8_t>(std::stoul(token, nullptr, 16)));
    }
    return bytes;
}

[[nodiscard]] auto HasIssue(const std::vector<Core::VerificationIssue> &issues, std::string_view code) -> bool
{
    return std::ranges::any_of(issues, [code](const auto &issue) { return issue.code == code; });
}
} // namespace

TEST_CASE("native VXCR v1 codec matches the Haskell golden contract")
{
    const auto expected = ReadGoldenHex();
    const auto encoded = Core::Wire::Encode(GoldenModule());
    REQUIRE(encoded);
    REQUIRE(encoded.bytes == expected);
    const auto decoded = Core::Wire::Decode(expected);
    REQUIRE(decoded);
    REQUIRE(decoded.module == GoldenModule());
}

TEST_CASE("VXCR reader rejects malformed boundaries and configured limits")
{
    const auto encoded = Core::Wire::Encode(PipelineModule());
    REQUIRE(encoded);

    SECTION("wrong transport")
    {
        auto bytes = encoded.bytes;
        bytes[3] = 'P';
        const auto decoded = Core::Wire::Decode(bytes);
        REQUIRE_FALSE(decoded);
        REQUIRE(decoded.error->kind == Core::Wire::ErrorKind::InvalidMagic);
    }
    SECTION("truncation")
    {
        auto bytes = encoded.bytes;
        bytes.pop_back();
        const auto decoded = Core::Wire::Decode(bytes);
        REQUIRE_FALSE(decoded);
        REQUIRE(decoded.error->kind == Core::Wire::ErrorKind::TruncatedInput);
    }
    SECTION("trailing input")
    {
        auto bytes = encoded.bytes;
        bytes.push_back(0U);
        const auto decoded = Core::Wire::Decode(bytes);
        REQUIRE_FALSE(decoded);
        REQUIRE(decoded.error->kind == Core::Wire::ErrorKind::TrailingInput);
    }
    SECTION("byte limit")
    {
        Core::Wire::Limits limits;
        limits.maximumWireBytes = encoded.bytes.size() - 1U;
        const auto decoded = Core::Wire::Decode(encoded.bytes, limits);
        REQUIRE_FALSE(decoded);
        REQUIRE(decoded.error->kind == Core::Wire::ErrorKind::LimitExceeded);
    }
}

TEST_CASE("native Core verifier blocks invalid references mutation and returns")
{
    SECTION("undefined symbol")
    {
        auto module = GoldenModule();
        module.functions.front().returnType = Core::Type::int64();
        module.functions.front().body.front() =
            Core::Statement::Return(Core::Expression::Variable({90U, U"missing"}, Core::Type::int64()));
        REQUIRE(HasIssue(Core::Verify(module), "VXC1020"));
    }
    SECTION("immutable assignment")
    {
        auto module = GoldenModule();
        auto &body = module.functions.front().body;
        body.insert(body.begin(),
                    Core::Statement::Bind({{2U, U"value"},
                                           Core::Type::int64(),
                                           false,
                                           Core::Expression::Constant(std::int64_t{1}, Core::Type::int64())}));
        body.insert(
            body.begin() + 1,
            Core::Statement::Assign({2U, U"value"}, Core::Expression::Constant(std::int64_t{2}, Core::Type::int64())));
        REQUIRE(HasIssue(Core::Verify(module), "VXC1013"));
    }
    SECTION("missing return path")
    {
        auto module = GoldenModule();
        module.functions.front().returnType = Core::Type::int64();
        module.functions.front().body = {Core::Statement::If(
            Core::Expression::Constant(true, Core::Type::boolean()),
            {Core::Statement::Return(Core::Expression::Constant(std::int64_t{1}, Core::Type::int64()))}, {})};
        REQUIRE(HasIssue(Core::Verify(module), "VXC1005"));
    }
}

TEST_CASE("Core adapter creates explicit CorePrep CFG and temporaries")
{
    const auto module = PipelineModule();
    REQUIRE(Core::Verify(module).empty());
    const auto prepared = Core::CorePrep::Prepare(module);
    REQUIRE(prepared.functions.size() == 2U);
    REQUIRE(prepared.functions.at(1).blocks.size() == 4U);
    REQUIRE(prepared.functions.at(1).blocks.front().terminator.kind == visual_xsharp::core::Terminator::Kind::Branch);
    REQUIRE(prepared.functions.at(1).blocks.at(1).instructions.size() == 2U);
    REQUIRE(visual_xsharp::core::verify(prepared).empty());
}

TEST_CASE("VXCR RAM pipeline reaches optimized Xpp Xmm and LLVM")
{
    const auto encoded = Core::Wire::Encode(PipelineModule());
    REQUIRE(encoded);
    const auto result = Visual::XSharp::Pipeline::ConsumeCore(encoded.bytes);
    REQUIRE(result);
    REQUIRE(result.core);
    REQUIRE(result.core_prep);
    REQUIRE(result.xpp);
    REQUIRE(result.xmm);
    REQUIRE(result.llvm);
    REQUIRE(result.coreVerificationIssues.empty());
    REQUIRE(result.verification_issues.empty());
    REQUIRE(result.xpp->functions.at(1).blocks.size() == 4U);
}

TEST_CASE("VXCR RAM pipeline never lowers semantically invalid Core")
{
    auto module = PipelineModule();
    module.functions.at(1).body.front().binding.value.type = Core::Type::boolean();
    const auto encoded = Core::Wire::Encode(module);
    REQUIRE(encoded);
    const auto result = Visual::XSharp::Pipeline::ConsumeCore(encoded.bytes);
    REQUIRE_FALSE(result);
    REQUIRE(result.core);
    REQUIRE(HasIssue(result.coreVerificationIssues, "VXC1011"));
    REQUIRE_FALSE(result.core_prep);
    REQUIRE_FALSE(result.xpp);
    REQUIRE_FALSE(result.xmm);
    REQUIRE_FALSE(result.llvm);
}

TEST_CASE("Core artifact driver validates and emits explicit LLVM artifacts")
{
    const auto encoded = Core::Wire::Encode(GoldenModule());
    REQUIRE(encoded);
    const auto directory = std::filesystem::temp_directory_path() / "visual-xsharp-core-driver";
    std::filesystem::create_directories(directory);
    const auto corePath = directory / "Golden.core";
    const auto llvmPath = directory / "Golden.ll";
    {
        std::ofstream stream(corePath, std::ios::binary | std::ios::trunc);
        stream.write(reinterpret_cast<const char *>(encoded.bytes.data()),
                     static_cast<std::streamsize>(encoded.bytes.size()));
    }
    const auto settings = xs_cli_default_compiler_settings();
    REQUIRE(xs_driver_process_core_artifact(corePath.string().c_str(), XS_CLI_COMMAND_CHECK, XS_BUILD_OUTPUT_NONE,
                                            &settings, nullptr));
    REQUIRE(xs_driver_process_core_artifact(corePath.string().c_str(), XS_CLI_COMMAND_BUILD, XS_BUILD_OUTPUT_LLVM_LL,
                                            &settings, nullptr));
    REQUIRE(std::filesystem::file_size(llvmPath) > 0U);
    std::filesystem::remove_all(directory);
}
