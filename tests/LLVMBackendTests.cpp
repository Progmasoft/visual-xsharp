// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <concepts>
#include <filesystem>
#include <fstream>
#include <ranges>
#include <string_view>

#include "Visual/XSharp/Backend/LLVM.hpp"
#include "Visual/XSharp/Core/CorePrep/Wire.hpp"
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
        options.target_triple = "x86_64-pc-windows-msvc";
        options.machineCode = emission;
        options.executableEntry = executable;
        return Llvm::Lower(xmm, options);
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
    REQUIRE(result.artifact->bitcode[0] == 'B');
    REQUIRE(result.artifact->bitcode[1] == 'C');
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
    REQUIRE(result.artifact->llvm_ir.find("{ ptr, i64 }") != std::string::npos);
    REQUIRE(result.artifact->llvm_ir.find("c\"Merhaba") == std::string::npos);
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

TEST_CASE("Xmm verifier rejects unsupported named types before LLVM construction")
{
    auto xmm = visual_xsharp::xmm::lower(visual_xsharp::xpp::lower(ArithmeticModule()));
    xmm.functions.front().return_type = Type::named({ U"User", U"Record" });
    const auto issues = Llvm::Verify(xmm);
    REQUIRE(HasIssue(issues, "VXL1005"));
    const auto result = Llvm::Lower(xmm);
    REQUIRE_FALSE(result);
    REQUIRE(result.error->code == "VXL2000");
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

TEST_CASE("LLVM target machine emits COFF object and assembly artifacts")
{
    const auto object = LowerMachineArtifact(ArithmeticModule(), Llvm::MachineCodeEmission::Object, true);
    REQUIRE(object);
    REQUIRE(object.artifact->objectFormat == Llvm::ObjectFormat::Coff);
    REQUIRE(object.artifact->object.size() > 100U);
    // AMD64 COFF starts with IMAGE_FILE_MACHINE_AMD64 in little-endian order.
    // This catches accidental bitcode/text output hidden behind an `.o` name.
    REQUIRE(object.artifact->object.at(0) == 0x64U);
    REQUIRE(object.artifact->object.at(1) == 0x86U);
    REQUIRE(object.artifact->llvm_ir.find("mainCRTStartup") != std::string::npos);

    const auto assembly = LowerMachineArtifact(ArithmeticModule(), Llvm::MachineCodeEmission::Assembly);
    REQUIRE(assembly);
    REQUIRE_FALSE(assembly.artifact->assembly.empty());
    REQUIRE(assembly.artifact->assembly.find("Backend.Contract.Calculate.10") != std::string::npos);
    REQUIRE(assembly.artifact->llvm_ir.find("mainCRTStartup") == std::string::npos);
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
