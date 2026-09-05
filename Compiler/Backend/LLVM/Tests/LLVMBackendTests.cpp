// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

#include <algorithm>
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <concepts>
#include <filesystem>
#include <fstream>
#include <ranges>
#include <string_view>

#include "Visual/XSharp/Backend/LLVM.hpp"
#include "Visual/XSharp/Core/CorePrep/Wire.hpp"
#include "Visual/XSharp/Core/Scalar.hpp"
#include "Visual/XSharp/Pipeline.hpp"
#include "Visual/XSharp/Xmm/Verifier.hpp"

namespace
{
    using visual_xsharp::core::Atom;
    using visual_xsharp::core::Block;
    using visual_xsharp::core::CorePrepModule;
    using visual_xsharp::core::Function;
    using visual_xsharp::core::Instruction;
    using visual_xsharp::core::Operation;
    using visual_xsharp::core::Parameter;
    using visual_xsharp::core::SymbolName;
    using visual_xsharp::core::Terminator;
    using visual_xsharp::core::Type;
    namespace Llvm = Visual::XSharp::Backend::LLVM;

    auto
    Literal(std::int64_t value) -> Atom
    {
        return Atom::constant(value, Type::int64());
    }

    auto
    Variable(std::uint64_t symbol, Type type) -> Atom
    {
        return Atom::variable(SymbolName{ symbol, {} }, std::move(type));
    }

    auto
    Register(std::uint32_t reg, Type type) -> visual_xsharp::xmm::Value
    {
        return { visual_xsharp::xmm::Value::Kind::Register, std::move(type), reg, 0U, {} };
    }

    auto
    ArithmeticModule() -> CorePrepModule
    {
        Function calculate{ { 10, U"Calculate" },
                            { Parameter{ { 11, U"left" }, Type::int64() }, Parameter{ { 12, U"right" }, Type::int64() } },
                            Type::int64(),
                            0,
                            { Block{ 0,
                                     { Instruction{ Instruction::Kind::Bind,
                                                    { 13, U"sum" },
                                                    Type::int64(),
                                                    false,
                                                    Operation::Add,
                                                    { Variable(11, Type::int64()), Variable(12, Type::int64()) } },
                                       Instruction{ Instruction::Kind::Bind,
                                                    { 14, U"quotient" },
                                                    Type::int64(),
                                                    false,
                                                    Operation::FloorDivide,
                                                    { Variable(13, Type::int64()), Literal(3) } } },
                                     Terminator{ Terminator::Kind::Return, Variable(14, Type::int64()), 0, 0 } } } };
        Function main{ { 20, U"Main" },
                       {},
                       Type::unit(),
                       0,
                       { Block{ 0,
                                { Instruction{ Instruction::Kind::Bind,
                                               { 21, U"answer" },
                                               Type::int64(),
                                               false,
                                               Operation::Call,
                                               { Variable(10, Type::function({ Type::int64(), Type::int64() }, Type::int64())),
                                                 Literal(40),
                                                 Literal(2) } },
                                  Instruction{ Instruction::Kind::Bind,
                                               { 22, U"ok" },
                                               Type::boolean(),
                                               false,
                                               Operation::GreaterEqual,
                                               { Variable(21, Type::int64()), Literal(14) } } },
                                Terminator{ Terminator::Kind::Branch, Variable(22, Type::boolean()), 1, 2 } },
                         Block{ 1, {}, Terminator{ Terminator::Kind::Return, Atom::constant({}, Type::unit()), 0, 0 } },
                         Block{ 2, {}, Terminator{ Terminator::Kind::Return, Atom::constant({}, Type::unit()), 0, 0 } } } };
        return CorePrepModule{ { U"Backend", U"Contract" }, { std::move(calculate), std::move(main) } };
    }

    auto
    StringModule() -> CorePrepModule
    {
        Function message{ { 30, U"Message" },
                          {},
                          Type::string(),
                          0,
                          { Block{ 0,
                                   { Instruction{ Instruction::Kind::Bind,
                                                  { 31, U"text" },
                                                  Type::string(),
                                                  false,
                                                  Operation::Copy,
                                                  { Atom::constant(std::u32string{ U"Merhaba \U0001f30d" }, Type::string()) } } },
                                   Terminator{ Terminator::Kind::Return, Variable(31, Type::string()), 0, 0 } } } };
        return CorePrepModule{ { U"Unicode" }, { std::move(message) } };
    }

    auto
    LowerModule(const CorePrepModule &module, Llvm::OptimizationLevel optimization = Llvm::OptimizationLevel::Default)
        -> Llvm::Result
    {
        const auto xpp = visual_xsharp::xpp::optimize(visual_xsharp::xpp::lower(module));
        const auto xmm = visual_xsharp::xmm::optimize(visual_xsharp::xmm::lower(xpp));
        Llvm::Options options;
        options.optimization = optimization;
        return Llvm::Lower(xmm, options);
    }

    auto
    LowerMachineArtifact(const CorePrepModule &module, Llvm::MachineCodeEmission emission, bool executable = false)
        -> Llvm::Result
    {
        const auto xpp = visual_xsharp::xpp::optimize(visual_xsharp::xpp::lower(module));
        const auto xmm = visual_xsharp::xmm::optimize(visual_xsharp::xmm::lower(xpp));
        Llvm::Options options;
        options.optimization = Llvm::OptimizationLevel::Debug;
        options.machineCode = emission;
        options.executableEntry = executable;
        return Llvm::Lower(xmm, options);
    }

    [[nodiscard]] auto
    HasValidBitcodeMagic(const std::vector<std::uint8_t> &bytes) -> bool
    {
        if (bytes.size() < 4U)
            return false;
        // LLVM writes either the raw `BC C0 DE` stream or the Darwin-compatible
        // wrapper beginning `DE C0 17 0B`. Both containers carry the same bitcode
        // module and are accepted by LLVM's readers.
        const bool raw = bytes[0] == 0x42U && bytes[1] == 0x43U && bytes[2] == 0xC0U && bytes[3] == 0xDEU;
        const bool wrapped = bytes[0] == 0xDEU && bytes[1] == 0xC0U && bytes[2] == 0x17U && bytes[3] == 0x0BU;
        return raw || wrapped;
    }

    auto
    HasIssue(const std::vector<Llvm::Issue> &issues, std::string_view code) -> bool
    {
        return std::ranges::any_of(issues, [code](const auto &issue) {
            return issue.code == code;
        });
    }

    struct TemporaryArtifacts final
    {
        std::filesystem::path llvm_ir = std::filesystem::temp_directory_path() / "visual-xsharp-backend-test.ll";
        std::filesystem::path bitcode = std::filesystem::temp_directory_path() / "visual-xsharp-backend-test.bc";
        std::filesystem::path object = std::filesystem::temp_directory_path() / "visual-xsharp-backend-test.o";
        std::filesystem::path assembly = std::filesystem::temp_directory_path() / "visual-xsharp-backend-test.asm";

        TemporaryArtifacts()
        {
            std::error_code ignored;
            std::filesystem::remove(llvm_ir, ignored);
            std::filesystem::remove(bitcode, ignored);
            std::filesystem::remove(object, ignored);
            std::filesystem::remove(assembly, ignored);
        }
        ~TemporaryArtifacts()
        {
            std::error_code ignored;
            std::filesystem::remove(llvm_ir, ignored);
            std::filesystem::remove(bitcode, ignored);
            std::filesystem::remove(object, ignored);
            std::filesystem::remove(assembly, ignored);
        }
    };

    auto
    ScalarLiteral(const Type &type, visual_xsharp::core::Literal literal) -> CorePrepModule
    {
        Function value{ { 100, U"ScalarValue" },
                        {},
                        type,
                        0,
                        { Block{ 0,
                                 {},
                                 Terminator{ Terminator::Kind::Return,
                                             Atom::constant(std::move(literal), type),
                                             0,
                                             0 } } } };
        return CorePrepModule{ { U"Backend", U"Scalar" }, { std::move(value) } };
    }

    auto
    BinaryScalarOperation(const Type &type, Operation operation) -> CorePrepModule
    {
        Function value{ { 101, U"ScalarOperation" },
                        { Parameter{ { 103, U"left" }, type }, Parameter{ { 104, U"right" }, type } },
                        type,
                        0,
                        { Block{ 0,
                                 { Instruction{ Instruction::Kind::Bind,
                                                { 102, U"result" },
                                                type,
                                                false,
                                                operation,
                                                { Variable(103, type), Variable(104, type) } } },
                                 Terminator{ Terminator::Kind::Return,
                                             Variable(102, type),
                                             0,
                                             0 } } } };
        return CorePrepModule{ { U"Backend", U"Scalar" }, { std::move(value) } };
    }

    auto
    Integer(bool negative, std::initializer_list<std::uint8_t> magnitude) -> visual_xsharp::core::IntegerLiteral
    {
        return visual_xsharp::core::IntegerLiteral{ negative, std::vector<std::uint8_t>(magnitude) };
    }
} // namespace

TEST_CASE("verified Xmm lowers to valid in-memory LLVM IR and bitcode")
{
    const auto result = LowerModule(ArithmeticModule(), Llvm::OptimizationLevel::Debug);
    REQUIRE(result);
    REQUIRE_FALSE(result.artifact->empty());
    REQUIRE(result.artifact->function_count == 2);
    REQUIRE_FALSE(result.artifact->target_triple.empty());
    REQUIRE(result.artifact->llvm_ir.find("Backend.Contract.Calculate.10") != std::string::npos);
    REQUIRE(result.artifact->llvm_ir.find("Backend.Contract.Main.20") != std::string::npos);
    REQUIRE(result.artifact->llvm_ir.find("call i64") != std::string::npos);
    REQUIRE(result.artifact->bitcode.size() > 100);
    REQUIRE(HasValidBitcodeMagic(result.artifact->bitcode));
}

TEST_CASE("LLVM lowering preserves signed floor division semantics in IR")
{
    const auto result = LowerModule(ArithmeticModule(), Llvm::OptimizationLevel::Debug);
    REQUIRE(result);
    REQUIRE(result.artifact->llvm_ir.find("sdiv i64") != std::string::npos);
    REQUIRE(result.artifact->llvm_ir.find("srem i64") != std::string::npos);
    REQUIRE(result.artifact->llvm_ir.find("floor.adjust") != std::string::npos);
    REQUIRE(result.artifact->llvm_ir.find("floor.result") != std::string::npos);
}

TEST_CASE("Visual X# String literals use Unicode scalar storage instead of UTF-8 bytes")
{
    const auto result = LowerModule(StringModule(), Llvm::OptimizationLevel::Debug);
    REQUIRE(result);
    REQUIRE(result.artifact->llvm_ir.find("[10 x i32]") != std::string::npos);
    REQUIRE(result.artifact->llvm_ir.find("i32 127757") != std::string::npos);
    REQUIRE(result.artifact->llvm_ir.find("call ptr @vxs_aarc_string_literal") != std::string::npos);
    REQUIRE(result.artifact->llvm_ir.find("declare ptr @vxs_aarc_string_literal") != std::string::npos);
    REQUIRE(result.artifact->llvm_ir.find("c\"Merhaba") == std::string::npos);
}

TEST_CASE("LLVM scalar type lowering follows Visual X# fixed widths")
{
    const std::array<std::pair<Type, std::string_view>, 11> cases{
        std::pair{ Type::character(), "ret i32 127" },
        std::pair{ Type::int8(), "ret i8 127" },
        std::pair{ Type::int16(), "ret i16 127" },
        std::pair{ Type::int32(), "ret i32 127" },
        std::pair{ Type::int64(), "ret i64 127" },
        std::pair{ Type::int128(), "ret i128 127" },
        std::pair{ Type::uint8(), "ret i8 127" },
        std::pair{ Type::uint16(), "ret i16 127" },
        std::pair{ Type::uint32(), "ret i32 127" },
        std::pair{ Type::uint64(), "ret i64 127" },
        std::pair{ Type::uint128(), "ret i128 127" },
    };
    for (const auto &[type, expected] : cases)
    {
        CAPTURE(expected);
        const auto result = LowerModule(ScalarLiteral(type, Integer(false, { 0x7fU })), Llvm::OptimizationLevel::Debug);
        REQUIRE(result);
        CHECK(result.artifact->llvm_ir.find(expected) != std::string::npos);
    }
}

TEST_CASE("LLVM floating type lowering parses canonical payloads at declared precision")
{
    const std::array<std::pair<Type, std::string_view>, 4> cases{
        std::pair{ Type::float16(), "half" },
        std::pair{ Type::float32(), "float" },
        std::pair{ Type::float64(), "double" },
        std::pair{ Type::float128(), "fp128" },
    };
    for (const auto &[type, spelling] : cases)
    {
        CAPTURE(spelling);
        const auto result = LowerModule(
            ScalarLiteral(type, visual_xsharp::core::FloatingLiteral{ "1.25" }),
            Llvm::OptimizationLevel::Debug);
        REQUIRE(result);
        CHECK(result.artifact->llvm_ir.find(std::string("ret ") + std::string(spelling)) != std::string::npos);
    }
}

TEST_CASE("LLVM arithmetic selects signed unsigned and floating instructions from scalar type")
{
    const auto signedResult = LowerModule(
        BinaryScalarOperation(Type::int16(), Operation::Divide),
        Llvm::OptimizationLevel::Debug);
    REQUIRE(signedResult);
    CHECK(signedResult.artifact->llvm_ir.find("sdiv i16") != std::string::npos);

    const auto unsignedResult = LowerModule(
        BinaryScalarOperation(Type::uint16(), Operation::Divide),
        Llvm::OptimizationLevel::Debug);
    REQUIRE(unsignedResult);
    CHECK(unsignedResult.artifact->llvm_ir.find("udiv i16") != std::string::npos);

    const auto floatingResult = LowerModule(
        BinaryScalarOperation(Type::float64(), Operation::Divide),
        Llvm::OptimizationLevel::Debug);
    REQUIRE(floatingResult);
    CHECK(floatingResult.artifact->llvm_ir.find("fdiv double") != std::string::npos);
}

TEST_CASE("LLVM ordered comparison chooses signed unsigned and floating predicates")
{
    const auto lowerComparison = [](const Type &type) {
        auto module = BinaryScalarOperation(type, Operation::LessThan);
        auto &function = module.functions.front();
        function.return_type = Type::boolean();
        function.blocks.front().instructions.front().type = Type::boolean();
        function.blocks.front().terminator.value.type = Type::boolean();
        return LowerModule(module, Llvm::OptimizationLevel::Debug);
    };

    const auto signedResult = lowerComparison(Type::int32());
    REQUIRE(signedResult);
    CHECK(signedResult.artifact->llvm_ir.find("icmp slt i32") != std::string::npos);

    const auto unsignedResult = lowerComparison(Type::uint32());
    REQUIRE(unsignedResult);
    CHECK(unsignedResult.artifact->llvm_ir.find("icmp ult i32") != std::string::npos);

    const auto floatingResult = lowerComparison(Type::float32());
    REQUIRE(floatingResult);
    CHECK(floatingResult.artifact->llvm_ir.find("fcmp olt float") != std::string::npos);
}

TEST_CASE("LLVM floor division distinguishes signed unsigned and floating semantics")
{
    const auto signedResult = LowerModule(
        BinaryScalarOperation(Type::int32(), Operation::FloorDivide),
        Llvm::OptimizationLevel::Debug);
    REQUIRE(signedResult);
    CHECK(signedResult.artifact->llvm_ir.find("floor.adjust") != std::string::npos);

    const auto unsignedResult = LowerModule(
        BinaryScalarOperation(Type::uint32(), Operation::FloorDivide),
        Llvm::OptimizationLevel::Debug);
    REQUIRE(unsignedResult);
    CHECK(unsignedResult.artifact->llvm_ir.find("udiv i32") != std::string::npos);
    CHECK(unsignedResult.artifact->llvm_ir.find("floor.adjust") == std::string::npos);

    const auto floatingResult = LowerModule(
        BinaryScalarOperation(Type::float32(), Operation::FloorDivide),
        Llvm::OptimizationLevel::Debug);
    REQUIRE(floatingResult);
    CHECK(floatingResult.artifact->llvm_ir.find("llvm.floor.f32") != std::string::npos);
}

TEST_CASE("Xmm lowering retains function identities signatures and result types")
{
    const auto xpp = visual_xsharp::xpp::lower(ArithmeticModule());
    const auto xmm = visual_xsharp::xmm::lower(xpp);
    REQUIRE((xmm.functions[0].symbol == SymbolName{ 10, U"Calculate" }));
    REQUIRE(xmm.functions[0].parameter_types == std::vector<Type>{ Type::int64(), Type::int64() });
    REQUIRE(xmm.functions[0].blocks[0].instructions[0].result_type == Type::int64());
    const auto &call = xmm.functions[1].blocks[0].instructions[0];
    REQUIRE(call.opcode == visual_xsharp::xmm::Opcode::Call);
    REQUIRE(call.operands.front().kind == visual_xsharp::xmm::Value::Kind::Function);
    REQUIRE(call.operands.front().symbol == 10);
    REQUIRE(call.result_type == Type::int64());
}

TEST_CASE("Xmm verifier rejects undefined and redefined virtual registers")
{
    auto xmm = visual_xsharp::xmm::lower(visual_xsharp::xpp::lower(ArithmeticModule()));
    auto &instructions = xmm.functions.front().blocks.front().instructions;
    instructions.front().operands.front().reg = 999;
    instructions.back().destination = instructions.front().destination;
    instructions.back().result_type = Type::boolean();
    const auto issues = Llvm::Verify(xmm);
    const auto stageIssues = Visual::XSharp::Xmm::Verify(xmm);
    REQUIRE(HasIssue(issues, "VXL1011"));
    REQUIRE(HasIssue(issues, "VXL1026"));
    REQUIRE(stageIssues == issues);
    const auto result = Llvm::Lower(xmm);
    REQUIRE_FALSE(result);
    REQUIRE(result.error->kind == Llvm::ErrorKind::InvalidXmm);
    REQUIRE(result.error->issues.size() >= 2);
}

TEST_CASE("Xmm verifier rejects call signature and control-flow corruption")
{
    auto xmm = visual_xsharp::xmm::lower(visual_xsharp::xpp::lower(ArithmeticModule()));
    auto &main = xmm.functions.back();
    main.blocks.front().instructions.front().operands.pop_back();
    main.blocks.front().terminator.true_target = 404;
    const auto issues = Llvm::Verify(xmm);
    REQUIRE(HasIssue(issues, "VXL1016"));
    REQUIRE(HasIssue(issues, "VXL1030"));
}

TEST_CASE("Xmm verifier rejects unresolved type variables before LLVM construction")
{
    auto xmm = visual_xsharp::xmm::lower(visual_xsharp::xpp::lower(ArithmeticModule()));
    xmm.functions.front().return_type = Type::type_variable({ 900U, U"T" });
    const auto issues = Llvm::Verify(xmm);
    REQUIRE(HasIssue(issues, "VXL1005"));
    const auto result = Llvm::Lower(xmm);
    REQUIRE_FALSE(result);
    REQUIRE(result.error->code == "VXL2000");
}

TEST_CASE("LLVM lowers explicit AARC ownership instructions to the stable runtime ABI")
{
    const auto objectType = Type::named({ U"Tests", U"Object" });
    using XmmInstruction = visual_xsharp::xmm::Instruction;
    using XmmOpcode = visual_xsharp::xmm::Opcode;
    visual_xsharp::xmm::Function function{
        { 500U, U"Manage" },
        { 1U },
        { objectType },
        Type::unit(),
        0U,
        { visual_xsharp::xmm::Block{
            0U,
            {
                XmmInstruction{ XmmOpcode::RetainStrong, 2U, objectType, { Register(1U, objectType) }, true, 0U, {} },
                XmmInstruction{ XmmOpcode::MakeWeak, 3U, objectType, { Register(2U, objectType) }, true, 0U, {} },
                XmmInstruction{ XmmOpcode::LockWeak, 4U, objectType, { Register(3U, objectType) }, true, 0U, {} },
                XmmInstruction{ XmmOpcode::MakeUnowned, 5U, objectType, { Register(4U, objectType) }, true, 0U, {} },
                XmmInstruction{ XmmOpcode::LoadUnowned, 6U, objectType, { Register(5U, objectType) }, true, 0U, {} },
                XmmInstruction{ XmmOpcode::ReleaseStrong, 0U, Type::unit(), { Register(2U, objectType) }, false, 0U, {} },
                XmmInstruction{ XmmOpcode::ReleaseStrong, 0U, Type::unit(), { Register(4U, objectType) }, false, 0U, {} },
                XmmInstruction{ XmmOpcode::ReleaseWeak, 0U, Type::unit(), { Register(3U, objectType) }, false, 0U, {} },
                XmmInstruction{ XmmOpcode::ReleaseUnowned, 0U, Type::unit(), { Register(5U, objectType) }, false, 0U, {} },
            },
            { visual_xsharp::xmm::Terminator::Kind::Return, { visual_xsharp::xmm::Value::Kind::Immediate, Type::unit(), 0U, 0U, {} }, 0U, 0U } } }
    };
    const visual_xsharp::xmm::Module module{ { U"Aarc", U"Operations" }, { std::move(function) } };
    Llvm::Options options;
    options.optimization = Llvm::OptimizationLevel::Debug;
    const auto result = Llvm::Lower(module, options);
    REQUIRE(result);
    CHECK(result.artifact->llvm_ir.find("@vxs_aarc_retain_strong") != std::string::npos);
    CHECK(result.artifact->llvm_ir.find("@vxs_aarc_make_weak") != std::string::npos);
    CHECK(result.artifact->llvm_ir.find("@vxs_aarc_lock_weak") != std::string::npos);
    CHECK(result.artifact->llvm_ir.find("@vxs_aarc_make_unowned") != std::string::npos);
    CHECK(result.artifact->llvm_ir.find("@vxs_aarc_load_unowned") != std::string::npos);
    CHECK(result.artifact->llvm_ir.find("@vxs_aarc_release_strong") != std::string::npos);
    CHECK(result.artifact->llvm_ir.find("@vxs_aarc_release_weak") != std::string::npos);
    CHECK(result.artifact->llvm_ir.find("@vxs_aarc_release_unowned") != std::string::npos);
}

TEST_CASE("LLVM materializes closure payload ownership and its AARC destructor")
{
    using XmmBlock = visual_xsharp::xmm::Block;
    using XmmFunction = visual_xsharp::xmm::Function;
    using XmmInstruction = visual_xsharp::xmm::Instruction;
    using XmmOpcode = visual_xsharp::xmm::Opcode;
    using XmmTerminator = visual_xsharp::xmm::Terminator;
    using CaptureMode = visual_xsharp::core::CaptureMode;

    const auto objectType = Type::named({ U"Tests", U"Captured" });
    const auto callableType = Type::function({}, Type::unit());
    const auto unitReturn = XmmTerminator{
        XmmTerminator::Kind::Return,
        { visual_xsharp::xmm::Value::Kind::Immediate, Type::unit(), 0U, 0U, {} },
        0U,
        0U
    };
    XmmFunction lifted{
        { 610U, U"ClosureBody" },
        { 1U },
        { objectType },
        Type::unit(),
        0U,
        { XmmBlock{ 0U, {}, unitReturn } }
    };
    XmmFunction factory{
        { 611U, U"Factory" },
        { 1U },
        { objectType },
        Type::unit(),
        0U,
        { XmmBlock{
            0U,
            {
                XmmInstruction{
                    XmmOpcode::MakeClosure,
                    2U,
                    callableType,
                    { Register(1U, objectType) },
                    true,
                    610U,
                    { CaptureMode::Strong } },
                XmmInstruction{
                    XmmOpcode::ReleaseStrong,
                    0U,
                    Type::unit(),
                    { Register(2U, callableType) },
                    false,
                    0U,
                    {} },
            },
            unitReturn } }
    };
    const visual_xsharp::xmm::Module module{
        { U"Aarc", U"Closure" },
        { std::move(lifted), std::move(factory) }
    };
    Llvm::Options options;
    options.optimization = Llvm::OptimizationLevel::Debug;
    const auto result = Llvm::Lower(module, options);
    REQUIRE(result);
    CHECK(result.artifact->llvm_ir.find("vxs.aarc.closure.payload") != std::string::npos);
    CHECK(result.artifact->llvm_ir.find("vxs.aarc.closure.metadata") != std::string::npos);
    CHECK(result.artifact->llvm_ir.find("vxs.aarc.closure.destroy") != std::string::npos);
    CHECK(result.artifact->llvm_ir.find("call ptr @vxs_aarc_allocate") != std::string::npos);
    CHECK(result.artifact->llvm_ir.find("capture.strong") != std::string::npos);
    CHECK(result.artifact->llvm_ir.find("call void @vxs_aarc_release_strong") != std::string::npos);
}

TEST_CASE("Xmm verifier reports module and parameter shape independently")
{
    visual_xsharp::xmm::Module empty{ {}, {} };
    const auto emptyIssues = Llvm::Verify(empty);
    REQUIRE(HasIssue(emptyIssues, "VXL1001"));
    REQUIRE(HasIssue(emptyIssues, "VXL1002"));

    auto xmm = visual_xsharp::xmm::lower(visual_xsharp::xpp::lower(ArithmeticModule()));
    xmm.functions.front().parameter_types.pop_back();
    xmm.functions.front().parameter_registers.front() = 0;
    const auto parameterIssues = Llvm::Verify(xmm);
    REQUIRE(HasIssue(parameterIssues, "VXL1006"));
    REQUIRE(HasIssue(parameterIssues, "VXL1007"));
}

TEST_CASE("Xmm verifier rejects immediate payload and declared type disagreement")
{
    auto xmm = visual_xsharp::xmm::lower(visual_xsharp::xpp::lower(ArithmeticModule()));
    auto &immediate = xmm.functions.front().blocks.front().instructions.front().operands.back();
    REQUIRE(immediate.kind == visual_xsharp::xmm::Value::Kind::Register);
    immediate.kind = visual_xsharp::xmm::Value::Kind::Immediate;
    immediate.immediate = true;
    const auto issues = Llvm::Verify(xmm);
    REQUIRE(HasIssue(issues, "VXL1018"));
}

TEST_CASE("LLVM artifact records an explicit target triple without repository paths")
{
    const auto xmm = visual_xsharp::xmm::lower(visual_xsharp::xpp::lower(StringModule()));
    Llvm::Options options;
    options.optimization = Llvm::OptimizationLevel::Debug;
    options.target_triple = "x86_64-pc-windows-msvc";
    const auto result = Llvm::Lower(xmm, options);
    REQUIRE(result);
    REQUIRE(result.artifact->target_triple == options.target_triple);
    REQUIRE(result.artifact->llvm_ir.find("target triple = \"x86_64-pc-windows-msvc\"") != std::string::npos);
    REQUIRE(result.artifact->llvm_ir.find("C:/LLVM") == std::string::npos);
}

TEST_CASE("LLVM native target machine emits host object and assembly artifacts")
{
    const auto object = LowerMachineArtifact(ArithmeticModule(), Llvm::MachineCodeEmission::Object, true);
    REQUIRE(object);
#ifdef _WIN32
    REQUIRE(object.artifact->objectFormat == Llvm::ObjectFormat::Coff);
    // AMD64 COFF starts with IMAGE_FILE_MACHINE_AMD64 in little-endian order.
    // This catches accidental bitcode/text output hidden behind an `.o` name.
    REQUIRE(object.artifact->object.at(0) == 0x64U);
    REQUIRE(object.artifact->object.at(1) == 0x86U);
    REQUIRE(object.artifact->llvm_ir.find("mainCRTStartup") != std::string::npos);
#else
    REQUIRE(object.artifact->objectFormat == Llvm::ObjectFormat::MachO);
    // Official macOS hosts emit little-endian 64-bit Mach-O objects on both
    // Apple Silicon and Intel. Assert the complete magic, not the CPU subtype.
    REQUIRE(object.artifact->object.at(0) == 0xCFU);
    REQUIRE(object.artifact->object.at(1) == 0xFAU);
    REQUIRE(object.artifact->object.at(2) == 0xEDU);
    REQUIRE(object.artifact->object.at(3) == 0xFEU);
    REQUIRE(object.artifact->llvm_ir.find("define i32 @main()") != std::string::npos);
#endif
    REQUIRE(object.artifact->object.size() > 100U);

    const auto assembly = LowerMachineArtifact(ArithmeticModule(), Llvm::MachineCodeEmission::Assembly);
    REQUIRE(assembly);
    REQUIRE_FALSE(assembly.artifact->assembly.empty());
    REQUIRE(assembly.artifact->assembly.find("Backend.Contract.Calculate.10") != std::string::npos);
    REQUIRE(assembly.artifact->llvm_ir.find("mainCRTStartup") == std::string::npos);
    REQUIRE(assembly.artifact->llvm_ir.find("define i32 @main()") == std::string::npos);
}

TEST_CASE("native executable emission requires one valid Main function")
{
    // A library-like module remains legal until executable emission requests a
    // concrete process entry. The diagnostic belongs at that precise boundary.
    const auto result = LowerMachineArtifact(StringModule(), Llvm::MachineCodeEmission::Object, true);
    REQUIRE_FALSE(result);
    REQUIRE(result.error->kind == Llvm::ErrorKind::InvalidEntryPoint);
    REQUIRE(result.error->code == "VXL2007");
}

TEST_CASE("canonical Visual XSharp C++ namespace owns the renewed backend")
{
    static_assert(std::same_as<Llvm::Artifact, Visual::XSharp::Backend::LLVM::Artifact>);
    const auto xmm = visual_xsharp::xmm::lower(visual_xsharp::xpp::lower(ArithmeticModule()));
    const auto issues = Llvm::Verify(xmm);
    REQUIRE(issues.empty());
    const auto result = Llvm::Lower(xmm);
    REQUIRE(result);
}

TEST_CASE("LLVM artifacts write only through explicit extension-checked APIs")
{
    const auto result = LowerModule(ArithmeticModule());
    REQUIRE(result);
    TemporaryArtifacts paths;
    REQUIRE_FALSE(Llvm::WriteLlvmIr(paths.llvm_ir, result.artifact->llvm_ir));
    REQUIRE_FALSE(Llvm::WriteBitcode(paths.bitcode, result.artifact->bitcode));
    REQUIRE(std::filesystem::file_size(paths.llvm_ir) == result.artifact->llvm_ir.size());
    REQUIRE(std::filesystem::file_size(paths.bitcode) == result.artifact->bitcode.size());

    const auto object = LowerMachineArtifact(ArithmeticModule(), Llvm::MachineCodeEmission::Object);
    const auto assembly = LowerMachineArtifact(ArithmeticModule(), Llvm::MachineCodeEmission::Assembly);
    REQUIRE(object);
    REQUIRE(assembly);
    REQUIRE_FALSE(Llvm::WriteObject(paths.object, object.artifact->object));
    REQUIRE_FALSE(Llvm::WriteAssembly(paths.assembly, assembly.artifact->assembly));
    REQUIRE(std::filesystem::file_size(paths.object) == object.artifact->object.size());
    REQUIRE(std::filesystem::file_size(paths.assembly) == assembly.artifact->assembly.size());

    const auto wrongText = Llvm::WriteLlvmIr(paths.bitcode, result.artifact->llvm_ir);
    const auto wrongBinary = Llvm::WriteBitcode(paths.llvm_ir, result.artifact->bitcode);
    REQUIRE(wrongText);
    REQUIRE(wrongBinary);
    REQUIRE(wrongText->code == "VXL3002");
    REQUIRE(wrongBinary->code == "VXL3002");
}

TEST_CASE("RAM pipeline reaches LLVM only after wire and semantic verification")
{
    const auto encoded = visual_xsharp::core::wire::encode(ArithmeticModule());
    REQUIRE(encoded);
    const auto result = visual_xsharp::consume_coreprep(encoded.bytes);
    REQUIRE(result);
    REQUIRE(result.core_prep);
    REQUIRE(result.xpp);
    REQUIRE(result.xmm);
    REQUIRE(result.xmmVerificationIssues.empty());
    REQUIRE(result.llvm);
    REQUIRE_FALSE(result.llvm_error);
    REQUIRE(result.llvm->function_count == 2);
}

TEST_CASE("RAM pipeline stops unsupported semantic types before native lowering")
{
    auto module = ArithmeticModule();
    module.functions.front().return_type = Type::named({ U"Unsupported" });
    const auto encoded = visual_xsharp::core::wire::encode(module);
    REQUIRE(encoded);
    const auto result = visual_xsharp::consume_coreprep(encoded.bytes);
    REQUIRE_FALSE(result);
    REQUIRE(result.core_prep);
    REQUIRE_FALSE(result.verification_issues.empty());
    REQUIRE_FALSE(result.xpp);
    REQUIRE_FALSE(result.xmm);
    REQUIRE_FALSE(result.llvm);
    REQUIRE_FALSE(result.llvm_error);
}
