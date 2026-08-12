// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

#include "Visual/XSharp/Core/CorePrep/Artifact.hpp"
#include "xs/sources/driver/coreprep_driver.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

namespace
{
auto module() -> visual_xsharp::core::CorePrepModule
{
    using namespace visual_xsharp::core;
    Function main{{1, U"Main"}, {}, Type::unit(), 0,
                  {{0, {}, {Terminator::Kind::Return, Atom::constant({}, Type::unit()), 0, 0}}}};
    return CorePrepModule{{U"Driver"}, {std::move(main)}};
}

struct TemporaryCore final
{
    std::filesystem::path path = std::filesystem::temp_directory_path() / "visual-xsharp-driver-test.core";
    std::filesystem::path llvm_ir = std::filesystem::temp_directory_path() / "visual-xsharp-driver-test.ll";
    std::filesystem::path bitcode = std::filesystem::temp_directory_path() / "visual-xsharp-driver-test.bc";

    TemporaryCore()
    {
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
        std::filesystem::remove(llvm_ir, ignored);
        std::filesystem::remove(bitcode, ignored);
        REQUIRE_FALSE(visual_xsharp::core::write_coreprep_artifact(path, module()).has_value());
    }

    ~TemporaryCore()
    {
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
        std::filesystem::remove(llvm_ir, ignored);
        std::filesystem::remove(bitcode, ignored);
    }
};

auto options_for(const char *command, const char *path) -> XsCliOptions
{
    XsCliOptions options{};
    options.command = command;
    options.file_path = path;
    options.input = XS_BUILD_INPUT_CORE;
    options.compiler = xs_cli_default_compiler_settings();
    return options;
}
} // namespace

TEST_CASE("CorePrep CLI driver verifies and lowers explicit artifact input")
{
    TemporaryCore artifact;
    const auto path = artifact.path.string();

    SECTION("build")
    {
        auto options = options_for("build", path.c_str());
        REQUIRE(xs_driver_build_coreprep(&options) == 0);
    }
    SECTION("check")
    {
        auto options = options_for("check", path.c_str());
        options.compiler.xpp_optimization_passes = false;
        options.compiler.xmm_optimization_passes = false;
        REQUIRE(xs_driver_build_coreprep(&options) == 0);
    }
}

TEST_CASE("CorePrep CLI driver rejects unsupported command and emission combinations")
{
    TemporaryCore artifact;
    const auto path = artifact.path.string();

    SECTION("run is not an artifact-input command")
    {
        auto options = options_for("run", path.c_str());
        REQUIRE(xs_driver_build_coreprep(&options) == 2);
    }
    SECTION("artifact emission is not guessed")
    {
        auto options = options_for("build", path.c_str());
        options.output_override = true;
        options.output = XS_BUILD_OUTPUT_XMM;
        REQUIRE(xs_driver_build_coreprep(&options) == 1);
    }
    SECTION("missing path is a usage error")
    {
        auto options = options_for("build", nullptr);
        REQUIRE(xs_driver_build_coreprep(&options) == 2);
    }
}

TEST_CASE("CorePrep CLI driver reports malformed artifact input")
{
    const auto path = std::filesystem::temp_directory_path() / "visual-xsharp-driver-malformed.core";
    {
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        stream << "malformed";
    }
    const auto path_text = path.string();
    auto options = options_for("build", path_text.c_str());
    REQUIRE(xs_driver_build_coreprep(&options) == 1);
    REQUIRE(std::filesystem::remove(path));
}

TEST_CASE("CorePrep CLI driver explicitly emits LLVM text and bitcode")
{
    TemporaryCore artifact;
    const auto path = artifact.path.string();

    SECTION("llvmll")
    {
        auto options = options_for("build", path.c_str());
        options.output_override = true;
        options.output = XS_BUILD_OUTPUT_LLVM_LL;
        options.compiler.llvm_opt_level = XS_LLVM_OPT_G;
        REQUIRE(xs_driver_build_coreprep(&options) == 0);
        REQUIRE(std::filesystem::exists(artifact.llvm_ir));
        std::ifstream stream(artifact.llvm_ir, std::ios::binary);
        const std::string text{std::istreambuf_iterator<char>{stream}, {}};
        REQUIRE(text.find("Driver.Main.1") != std::string::npos);
        REQUIRE(text.find("define void") != std::string::npos);
    }
    SECTION("llvmbc")
    {
        auto options = options_for("build", path.c_str());
        options.output_override = true;
        options.output = XS_BUILD_OUTPUT_LLVM_BC;
        options.compiler.llvm_opt_level = XS_LLVM_OPT_3;
        REQUIRE(xs_driver_build_coreprep(&options) == 0);
        REQUIRE(std::filesystem::file_size(artifact.bitcode) > 100);
        std::ifstream stream(artifact.bitcode, std::ios::binary);
        char magic[2]{};
        stream.read(magic, 2);
        REQUIRE(magic[0] == 'B');
        REQUIRE(magic[1] == 'C');
    }
}

TEST_CASE("CorePrep check command cannot create LLVM artifacts")
{
    TemporaryCore artifact;
    const auto path = artifact.path.string();
    auto options = options_for("check", path.c_str());
    options.output_override = true;
    options.output = XS_BUILD_OUTPUT_LLVM_LL;
    REQUIRE(xs_driver_build_coreprep(&options) == 2);
    REQUIRE_FALSE(std::filesystem::exists(artifact.llvm_ir));
}
