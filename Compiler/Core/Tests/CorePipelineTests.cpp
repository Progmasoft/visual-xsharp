// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <ranges>
#include <sstream>

#include "Compiler/Driver/CorePipeline.hpp"
#include "Visual/XSharp/Core/CorePrep/Prepare.hpp"
#include "Visual/XSharp/Core/Verifier.hpp"
#include "Visual/XSharp/Core/Wire.hpp"
#include "Visual/XSharp/Pipeline.hpp"

#ifdef _WIN32
#    include <process.h>
#endif

namespace
{
    namespace Core = Visual::XSharp::Core;

    [[nodiscard]] auto
    GoldenModule() -> Core::Module
    {
        return { { U"Demo" },
                 { { { 1U, U"Main" },
                     {},
                     Core::Type::unit(),
                     { Core::Statement::Return(Core::Expression::Constant(std::monostate{}, Core::Type::unit())) } } } };
    }

    [[nodiscard]] auto
    PipelineModule() -> Core::Module
    {
        const auto integer = [](std::int64_t value) {
            return Core::Expression::Constant(value, Core::Type::int64());
        };
        const auto variable = [](std::uint64_t id, std::u32string spelling, Core::Type type) {
            return Core::Expression::Variable({ id, std::move(spelling) }, std::move(type));
        };
        const auto sumType = Core::Type::function({ Core::Type::int64(), Core::Type::int64() }, Core::Type::int64());
        Core::Function sum{ { 10U, U"Sum" },
                            { { { 11U, U"left" }, Core::Type::int64() }, { { 12U, U"right" }, Core::Type::int64() } },
                            Core::Type::int64(),
                            { Core::Statement::Return(Core::Expression::InvokePrimitive(
                                Core::Primitive::Add,
                                { variable(11U, U"left", Core::Type::int64()), variable(12U, U"right", Core::Type::int64()) },
                                Core::Type::int64())) } };

        auto call = Core::Expression::Apply(variable(10U, U"Sum", sumType), { integer(20), integer(22) }, Core::Type::int64());
        auto condition = Core::Expression::InvokePrimitive(Core::Primitive::GreaterEqual,
                                                           { variable(21U, U"value", Core::Type::int64()), integer(40) },
                                                           Core::Type::boolean());
        Core::Function main{
            { 20U, U"Main" },
            {},
            Core::Type::unit(),
            { Core::Statement::Bind({ { 21U, U"value" }, Core::Type::int64(), true, std::move(call) }),
              Core::Statement::If(
                  std::move(condition),
                  { Core::Statement::Assign({ 21U, U"value" }, Core::Expression::InvokePrimitive(Core::Primitive::Add, { variable(21U, U"value", Core::Type::int64()), integer(1) }, Core::Type::int64())) },
                  { Core::Statement::Assign({ 21U, U"value" }, integer(0)) }),
              Core::Statement::Return(Core::Expression::Constant(std::monostate{}, Core::Type::unit())) }
        };
        return { { U"Name" }, { std::move(sum), std::move(main) } };
    }

    [[nodiscard]] auto
    ClosureModule() -> Core::Module
    {
        const auto integer = [](std::int64_t value) {
            return Core::Expression::Constant(value, Core::Type::int64());
        };
        const auto variable = [](std::uint64_t id, std::u32string spelling, Core::Type type) {
            return Core::Expression::Variable({ id, std::move(spelling) }, std::move(type));
        };
        const auto callableType = Core::Type::function({}, Core::Type::int64());
        Core::Capture capture{
            Core::CaptureMode::Strong,
            { 4U, U"captured" },
            Core::Type::int64(),
            std::make_shared<Core::Expression>(variable(2U, U"seed", Core::Type::int64())),
        };
        auto closure = Core::Expression::Closure(
            { std::move(capture) },
            {},
            Core::Type::int64(),
            { Core::Statement::Return(variable(4U, U"captured", Core::Type::int64())) },
            callableType);
        Core::Function main{
            { 1U, U"Main" },
            {},
            Core::Type::unit(),
            {
                Core::Statement::Bind({ { 2U, U"seed" }, Core::Type::int64(), false, integer(42) }),
                Core::Statement::Bind({ { 3U, U"answer" }, callableType, false, std::move(closure) }),
                Core::Statement::Return(Core::Expression::Constant(std::monostate{}, Core::Type::unit())),
            },
        };
        return { { U"ClosureBoundary" }, { std::move(main) } };
    }

    [[nodiscard]] auto
    ReadGoldenHex(std::string_view filename = "wire-v4.hex") -> std::vector<std::uint8_t>
    {
        const auto path = std::filesystem::path(__FILE__).parent_path() / "Fixtures" / "Core" / std::filesystem::path(filename);
        std::ifstream stream(path);
        REQUIRE(stream);
        std::vector<std::uint8_t> bytes;
        std::string line;
        while (std::getline(stream, line))
        {
            if (const auto comment = line.find('#'); comment != std::string::npos)
                line.erase(comment);
            std::istringstream tokens(line);
            std::string token;
            while (tokens >> token)
                bytes.push_back(static_cast<std::uint8_t>(std::stoul(token, nullptr, 16)));
        }
        return bytes;
    }

    [[nodiscard]] auto
    HasIssue(const std::vector<Core::VerificationIssue> &issues, std::string_view code) -> bool
    {
        return std::ranges::any_of(issues, [code](const auto &issue) {
            return issue.code == code;
        });
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

TEST_CASE("VXCR v4 carries Haskell Core closure fields into native Core")
{
    const auto source = ClosureModule();
    REQUIRE(Core::Verify(source).empty());
    const auto encoded = Core::Wire::Encode(source);
    REQUIRE(encoded);

    const auto decoded = Core::Wire::Decode(encoded.bytes);
    REQUIRE(decoded);
    CHECK(decoded.module == source);
    const auto &closure = decoded.module->functions.front().body.at(1).binding.value;
    REQUIRE(closure.kind == Core::Expression::Kind::Closure);
    REQUIRE(closure.captures.size() == 1U);
    CHECK(closure.captures.front().mode == Core::CaptureMode::Strong);
    CHECK(closure.captures.front().symbol.id == 4U);
    REQUIRE(closure.closureBody);
    CHECK(closure.closureBody->size() == 1U);
}

TEST_CASE("native pipeline consumes a closure artifact emitted by Haskell")
{
    // This golden file is emitted from closure-boundary.vxs by vxs-frontend,
    // rather than re-encoded by the C++ model. It therefore locks the actual
    // cross-language expression tag and field order that production uses.
    const auto bytes = ReadGoldenHex("wire-v4-closure.hex");
    const auto decoded = Core::Wire::Decode(bytes);
    REQUIRE(decoded);
    REQUIRE(Core::Verify(*decoded.module).empty());
    const auto &body = decoded.module->functions.front().body;
    REQUIRE(body.size() == 3U);
    REQUIRE(body.at(1).kind == Core::Statement::Kind::Evaluate);
    CHECK(body.at(1).expression.kind == Core::Expression::Kind::Closure);
    CHECK(body.at(1).expression.type.kind == Core::Type::Kind::Function);
    INFO("closure type kind=" << static_cast<int>(body.at(1).expression.type.kind)
                              << " components=" << body.at(1).expression.type.components.size());

    const auto result = Visual::XSharp::Pipeline::ConsumeCore(bytes);
    INFO("core verification issues=" << result.coreVerificationIssues.size());
    INFO("CorePrep verification issues=" << result.verification_issues.size());
    INFO("Xpp verification issues=" << result.xppVerificationIssues.size());
    INFO("Xmm verification issues=" << result.xmmVerificationIssues.size());
    std::string corePrepDiagnostics;
    for (const auto &issue : result.verification_issues)
        corePrepDiagnostics += issue.code + ": " + issue.message + "\n";
    INFO(corePrepDiagnostics);
    std::string xmmDiagnostics;
    for (const auto &issue : result.xmmVerificationIssues)
        xmmDiagnostics += issue.code + ": " + issue.message + "\n";
    INFO(xmmDiagnostics);
    for (const auto &issue : result.coreVerificationIssues)
        INFO(issue.code << ": " << issue.message);
    for (const auto &issue : result.verification_issues)
        INFO(issue.code << ": " << issue.message);
    for (const auto &issue : result.xppVerificationIssues)
        INFO(issue.code << ": " << issue.message);
    for (const auto &issue : result.xmmVerificationIssues)
        INFO(issue.code << ": " << issue.message);
    if (result.llvm_error)
        INFO(result.llvm_error->code << ": " << result.llvm_error->message);
    INFO("core=" << result.core.has_value()
                 << " coreprep=" << result.core_prep.has_value()
                 << " xpp=" << result.xpp.has_value()
                 << " xmm=" << result.xmm.has_value()
                 << " llvm=" << result.llvm.has_value());
    REQUIRE(result);
    REQUIRE(result.core_prep);
    CHECK(result.core_prep->functions.size() == 2U);
    CHECK(result.xmmVerificationIssues.empty());
}

TEST_CASE("Core closure conversion lifts a target and preserves capture metadata")
{
    const auto prepared = Core::CorePrep::Prepare(ClosureModule());
    REQUIRE(prepared.functions.size() == 2U);
    const auto &main = prepared.functions.front();
    const auto &lifted = prepared.functions.back();
    REQUIRE(main.blocks.size() == 1U);
    const auto closure = std::ranges::find_if(main.blocks.front().instructions, [](const auto &instruction) {
        return instruction.operation == visual_xsharp::core::Operation::MakeClosure;
    });
    REQUIRE(closure != main.blocks.front().instructions.end());
    CHECK(closure->closure_function.id == lifted.symbol.id);
    REQUIRE(closure->captures.size() == 1U);
    CHECK(closure->captures.front().symbol.id == lifted.parameters.front().symbol.id);
    CHECK(closure->captures.front().value.symbol.id == 2U);
    CHECK(visual_xsharp::core::verify(prepared).empty());
}

TEST_CASE("VXCR closure survives the complete RAM compiler pipeline")
{
    const auto encoded = Core::Wire::Encode(ClosureModule());
    REQUIRE(encoded);
    const auto result = Visual::XSharp::Pipeline::ConsumeCore(encoded.bytes);
    REQUIRE(result);
    REQUIRE(result.core_prep);
    REQUIRE(result.xpp);
    REQUIRE(result.xmm);
    REQUIRE(result.llvm);
    CHECK(result.core_prep->functions.size() == 2U);
    CHECK(result.verification_issues.empty());
    CHECK(result.xppVerificationIssues.empty());
    CHECK(result.xmmVerificationIssues.empty());
}

TEST_CASE("Core verifier rejects incomplete closure payloads before wire lowering")
{
    SECTION("missing capture initializer")
    {
        auto module = ClosureModule();
        module.functions.front().body.at(1).binding.value.captures.front().value.reset();
        CHECK(HasIssue(Core::Verify(module), "VXC1035"));
    }
    SECTION("callable arity mismatch")
    {
        auto module = ClosureModule();
        auto &closure = module.functions.front().body.at(1).binding.value;
        closure.type = Core::Type::function({ Core::Type::int64() }, Core::Type::int64());
        CHECK(HasIssue(Core::Verify(module), "VXC1041"));
    }
    SECTION("closure result mismatch")
    {
        auto module = ClosureModule();
        auto &closure = module.functions.front().body.at(1).binding.value;
        closure.closureReturnType = Core::Type::boolean();
        CHECK(HasIssue(Core::Verify(module), "VXC1041"));
    }
    SECTION("invalid capture ownership mode")
    {
        auto module = ClosureModule();
        module.functions.front().body.at(1).binding.value.captures.front().mode = static_cast<Core::CaptureMode>(255U);
        CHECK(HasIssue(Core::Verify(module), "VXC1043"));
        CHECK_FALSE(Core::Wire::Encode(module));
    }
    SECTION("non-owning scalar capture")
    {
        auto module = ClosureModule();
        module.functions.front().body.at(1).binding.value.captures.front().mode = Core::CaptureMode::Weak;
        CHECK(HasIssue(Core::Verify(module), "VXC1044"));
    }
}

TEST_CASE("native Core accepts mutation of captured closure storage")
{
    auto module = ClosureModule();
    auto &closure = module.functions.front().body.at(1).binding.value;
    closure.closureBody = std::make_shared<std::vector<Core::Statement>>(
        std::vector<Core::Statement>{
            Core::Statement::Assign(
                { 4U, U"captured" },
                Core::Expression::Constant(std::int64_t{ 43 }, Core::Type::int64())),
            Core::Statement::Return(
                Core::Expression::Variable({ 4U, U"captured" }, Core::Type::int64())),
        });
    CHECK(Core::Verify(module).empty());
    const auto encoded = Core::Wire::Encode(module);
    REQUIRE(encoded);
    CHECK(Visual::XSharp::Pipeline::ConsumeCore(encoded.bytes));
}

TEST_CASE("native Core verifier blocks invalid references mutation and returns")
{
    SECTION("undefined symbol")
    {
        auto module = GoldenModule();
        module.functions.front().returnType = Core::Type::int64();
        module.functions.front().body.front() = Core::Statement::Return(Core::Expression::Variable({ 90U, U"missing" }, Core::Type::int64()));
        REQUIRE(HasIssue(Core::Verify(module), "VXC1020"));
    }
    SECTION("immutable assignment")
    {
        auto module = GoldenModule();
        auto &body = module.functions.front().body;
        body.insert(body.begin(),
                    Core::Statement::Bind({ { 2U, U"value" },
                                            Core::Type::int64(),
                                            false,
                                            Core::Expression::Constant(std::int64_t{ 1 }, Core::Type::int64()) }));
        body.insert(
            body.begin() + 1,
            Core::Statement::Assign({ 2U, U"value" }, Core::Expression::Constant(std::int64_t{ 2 }, Core::Type::int64())));
        REQUIRE(HasIssue(Core::Verify(module), "VXC1013"));
    }
    SECTION("missing return path")
    {
        auto module = GoldenModule();
        module.functions.front().returnType = Core::Type::int64();
        module.functions.front().body = { Core::Statement::If(
            Core::Expression::Constant(true, Core::Type::boolean()),
            { Core::Statement::Return(Core::Expression::Constant(std::int64_t{ 1 }, Core::Type::int64())) },
            {}) };
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

TEST_CASE("CorePrep canonicalizes numeric branch conditions before Xpp")
{
    Core::Module module{
        { U"NumericBranch" },
        { Core::Function{
            { 1U, U"Main" },
            {},
            Core::Type::unit(),
            { Core::Statement::If(
                Core::Expression::Constant(std::int64_t{ 7 }, Core::Type::int64()),
                { Core::Statement::Return(Core::Expression::Constant(std::monostate{}, Core::Type::unit())) },
                { Core::Statement::Return(Core::Expression::Constant(std::monostate{}, Core::Type::unit())) }) },
        } },
    };
    REQUIRE(Core::Verify(module).empty());
    const auto prepared = Core::CorePrep::Prepare(module);
    REQUIRE(visual_xsharp::core::verify(prepared).empty());
    const auto &entry = prepared.functions.front().blocks.front();
    REQUIRE(entry.terminator.kind == visual_xsharp::core::Terminator::Kind::Branch);
    CHECK(entry.terminator.value.type == Core::Type::boolean());
    REQUIRE(entry.instructions.size() == 1U);
    CHECK(entry.instructions.front().operation == visual_xsharp::core::Operation::NotEqual);
}

TEST_CASE("logical operands with distinct numeric types become canonical booleans")
{
    auto logical = Core::Expression::InvokePrimitive(
        Core::Primitive::LogicalAnd,
        {
            Core::Expression::Constant(std::int64_t{ 1 }, Core::Type::int64()),
            Core::Expression::Constant(
                visual_xsharp::core::FloatingLiteral{ "2" },
                Core::Type::float32()),
        },
        Core::Type::boolean());
    Core::Module module{
        { U"NumericLogical" },
        { Core::Function{
            { 1U, U"Evaluate" },
            {},
            Core::Type::boolean(),
            { Core::Statement::Return(std::move(logical)) },
        } },
    };
    REQUIRE(Core::Verify(module).empty());
    const auto prepared = Core::CorePrep::Prepare(module);
    REQUIRE(visual_xsharp::core::verify(prepared).empty());
    const auto &instructions = prepared.functions.front().blocks.front().instructions;
    REQUIRE(instructions.size() == 3U);
    CHECK(instructions.at(0).operation == visual_xsharp::core::Operation::NotEqual);
    CHECK(instructions.at(1).operation == visual_xsharp::core::Operation::NotEqual);
    CHECK(instructions.at(2).operation == visual_xsharp::core::Operation::LogicalAnd);
    const auto result = Visual::XSharp::Pipeline::ConsumeCore(Core::Wire::Encode(module).bytes);
    REQUIRE(result);
    CHECK(result.xmmVerificationIssues.empty());
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
    REQUIRE(result.xppVerificationIssues.empty());
    REQUIRE(result.xmmVerificationIssues.empty());
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

TEST_CASE("Core artifact driver validates and emits LLVM and native artifacts")
{
    const auto encoded = Core::Wire::Encode(GoldenModule());
    REQUIRE(encoded);
    const auto directory = std::filesystem::temp_directory_path() / "visual-xsharp-core-driver";
    std::filesystem::create_directories(directory);
    const auto corePath = directory / "Golden.core";
    const auto llvmPath = directory / "Golden.ll";
    const auto objectPath = directory / "Golden.o";
    const auto assemblyPath = directory / "Golden.asm";
    const auto executablePath = directory / "Golden.vxse";
    // Keep every explicit format beside one verified Core input, then exercise
    // the binary as a process to cover TargetMachine, LLD, and PE loading together.
    {
        std::ofstream stream(corePath, std::ios::binary | std::ios::trunc);
        stream.write(reinterpret_cast<const char *>(encoded.bytes.data()),
                     static_cast<std::streamsize>(encoded.bytes.size()));
    }
    const auto settings = xs_cli_default_compiler_settings();
    REQUIRE(xs_driver_process_core_artifact(corePath.string().c_str(), XS_CLI_COMMAND_CHECK, XS_BUILD_OUTPUT_NONE, &settings, nullptr));
    REQUIRE(xs_driver_process_core_artifact(corePath.string().c_str(), XS_CLI_COMMAND_BUILD, XS_BUILD_OUTPUT_LLVM_LL, &settings, nullptr));
    REQUIRE(std::filesystem::file_size(llvmPath) > 0U);
#ifdef _WIN32
    REQUIRE(xs_driver_process_core_artifact(corePath.string().c_str(), XS_CLI_COMMAND_BUILD, XS_BUILD_OUTPUT_OBJECT, &settings, nullptr));
    REQUIRE(xs_driver_process_core_artifact(corePath.string().c_str(), XS_CLI_COMMAND_BUILD, XS_BUILD_OUTPUT_ASSEMBLY, &settings, nullptr));
    REQUIRE(xs_driver_process_core_artifact(corePath.string().c_str(), XS_CLI_COMMAND_BUILD, XS_BUILD_OUTPUT_BINARY, &settings, nullptr));
    REQUIRE(std::filesystem::file_size(objectPath) > 0U);
    REQUIRE(std::filesystem::file_size(assemblyPath) > 0U);
    REQUIRE(std::filesystem::file_size(executablePath) > 0U);
    const std::vector<const wchar_t *> arguments{ executablePath.c_str(), nullptr };
    REQUIRE(_wspawnv(_P_WAIT, executablePath.c_str(), arguments.data()) == 0);
#endif
    std::filesystem::remove_all(directory);
}
