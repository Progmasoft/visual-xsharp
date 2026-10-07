// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

#include <Progmasoft/Catch3/Assertions.hpp>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include "Compiler/Cli/Commands/Commands.hpp"

// Programs built into native executables and run as processes.
//
// Every other execution test runs generated code inside the process that
// compiled it, where the runtime library of the compiler is at hand. A native
// executable has only what was linked into it. These cases build a program
// the way a user does, start the executable, and read its exit status: zero
// for a program that ran to the end of `Main`, and the status of a stopped
// process for a program that needed a value that cannot be computed.
//
// A program checks its own results with `Check`, which divides by zero when
// two numbers differ. A wrong result therefore stops the program, and one
// case confirms that it does, so that the others cannot pass by checking
// nothing.

namespace
{
    constexpr std::string_view kHeader
        = "namespace Demo;\n"
          "public class Program {\n"
          "    public static int Check(_ int got, _ int expected) {\n"
          "        return 1 / (got == expected ? 1 : 0);\n"
          "    }\n"
          "    public static int Zero(_ int v) { return v - v; }\n"
          "    public static int Pick(_ int flag, _ int value) {\n"
          "        if (flag > 0) { return value; }\n"
          "        return 7;\n"
          "    }\n";

    [[nodiscard]] auto
    Directory() -> std::filesystem::path
    {
        return std::filesystem::temp_directory_path()
               / "visual-xsharp-executable-run";
    }

    /// Writes the program, and gives the status of `vxs <command> -File` on
    /// it.
    [[nodiscard]] auto
    Drive(std::string_view command,
          std::string_view name,
          std::string_view members) -> int
    {
        std::filesystem::create_directories(Directory());
        const auto source = Directory() / (std::string(name) + ".vxs");
        {
            std::ofstream stream(source, std::ios::binary | std::ios::trunc);
            stream << kHeader << members << "}\n";
            REQUIRE(stream.good());
        }
        std::vector<std::string> storage{ "vxs",
                                          std::string(command),
                                          "-File",
                                          source.string() };
        std::vector<char *> arguments;
        arguments.reserve(storage.size() + 1U);
        for (auto &argument : storage)
            arguments.push_back(argument.data());
        arguments.push_back(nullptr);
        return Visual::XSharp::Cli::Run(static_cast<int>(storage.size()),
                                        arguments.data());
    }

#ifdef _WIN32
    /// The status of a process that executed an instruction the processor
    /// refuses, which is how generated code stops.
    constexpr int kStopped = static_cast<int>(0xC000001DU);

    [[nodiscard]] auto
    Run(std::string_view name, std::string_view members) -> int
    {
        return Drive("run", name, members);
    }
#endif
} // namespace

TEST_CASE("every program of this file passes the checks before code "
          "generation")
{
    CHECK(Drive("check",
                "Checked",
                "    public static void Main() { Check(Pick(0, 1), 7); }\n")
          == 0);
    std::filesystem::remove_all(Directory());
}

// The native linker is the Windows one; elsewhere `vxs` does not produce an
// executable yet.
#ifdef _WIN32

TEST_CASE("a program whose Main ends without a return runs to its end")
{
    CHECK(Run("Empty", "    public static void Main() { }\n") == 0);
    CHECK(Run("Falling",
              "    public static void Touch(_ int v) { int w = v + 1; }\n"
              "    public static void Twice(_ int v) {\n"
              "        if (v > 0) { Touch(v); return; }\n"
              "        Touch(v + 1);\n"
              "    }\n"
              "    public static void Main() {\n"
              "        Twice(1);\n"
              "        Twice(0);\n"
              "        for (int i = 0; i < 3; i += 1) { Touch(i); }\n"
              "    }\n")
          == 0);
    std::filesystem::remove_all(Directory());
}

TEST_CASE("a program that checks a wrong result stops")
{
    CHECK(Run("Right", "    public static void Main() { Check(1 + 1, 2); }\n")
          == 0);
    CHECK(Run("Wrong", "    public static void Main() { Check(1 + 1, 3); }\n")
          == kStopped);
    CHECK(Run("WrongLater",
              "    public static void Main() {\n"
              "        Check(Pick(1, 4), 4);\n"
              "        Check(Pick(0, 4), 4);\n"
              "    }\n")
          == kStopped);
    std::filesystem::remove_all(Directory());
}

TEST_CASE("an executable passes arguments by need")
{
    // A division by zero in an argument the method does not read is never
    // carried out; the same argument, read, is the value it should be.
    CHECK(Run("ByNeed",
              "    public static int Run(_ int a, _ int b) {\n"
              "        return Pick(a, b / a);\n"
              "    }\n"
              "    public static int Pass(_ int flag, _ int value) {\n"
              "        return Pick(flag, value);\n"
              "    }\n"
              "    public static void Main() {\n"
              "        Check(Run(0, 8), 7);\n"
              "        Check(Run(2, 8), 4);\n"
              "        Check(Pass(0, 8 / Zero(3)), 7);\n"
              "        Check(Pass(1, 8 / (Zero(3) + 2)), 4);\n"
              "        int shared = 12 / (Zero(1) + 3);\n"
              "        Check(Pick(1, shared) + shared, 8);\n"
              "    }\n")
          == 0);
    std::filesystem::remove_all(Directory());
}

TEST_CASE("an executable that needs a quotient by zero stops")
{
    CHECK(Run("Needed",
              "    public static void Main() {\n"
              "        Check(Pick(1, 8 / Zero(3)), 0);\n"
              "    }\n")
          == kStopped);
    CHECK(Run("NeededRemainder",
              "    public static void Main() {\n"
              "        int left = 8 % Zero(3);\n"
              "        Check(left, left);\n"
              "    }\n")
          == kStopped);
    CHECK(Run("NotNeeded",
              "    public static void Main() {\n"
              "        int never = 8 / Zero(3);\n"
              "        Check(Pick(0, never), 7);\n"
              "    }\n")
          == 0);
    std::filesystem::remove_all(Directory());
}

TEST_CASE("an executable wraps the least integer divided by minus one")
{
    CHECK(Run("Wraps",
              "    public static void Main() {\n"
              "        int least = Zero(1) - 9223372036854775807 - 1;\n"
              "        int minusOne = Zero(1) - 1;\n"
              "        Check(least / minusOne, least);\n"
              "        Check(least % minusOne, 0);\n"
              "    }\n")
          == 0);
    std::filesystem::remove_all(Directory());
}

TEST_CASE("an executable creates and calls closures")
{
    CHECK(Run("Closures",
              "    public static int Run(_ int a, _ int b) {\n"
              "        auto f = \\(int v) -> v + a;\n"
              "        auto g = \\(int w) -> f(w) * 2;\n"
              "        return g(b);\n"
              "    }\n"
              "    public static void Main() { Check(Run(1, 8), 18); }\n")
          == 0);
    std::filesystem::remove_all(Directory());
}

TEST_CASE("an executable reuses the memory of the objects it releases")
{
    // Each pass creates objects and releases them. Two million passes hold
    // far more than the executable's memory if nothing is reused, and no
    // more than one pass does if everything is.
    CHECK(Run("ReuseClosures",
              "    public static int Many(_ int rounds) {\n"
              "        int total = 0;\n"
              "        for (int i = 0; i < rounds; i += 1) {\n"
              "            auto add = \\(int v) -> v + i;\n"
              "            total += add(1) - i;\n"
              "        }\n"
              "        return total;\n"
              "    }\n"
              "    public static void Main() {\n"
              "        Check(Many(2000000), 2000000);\n"
              "    }\n")
          == 0);
    CHECK(Run("ReuseArguments",
              "    public static int Many(_ int rounds) {\n"
              "        int total = 0;\n"
              "        for (int i = 0; i < rounds; i += 1) {\n"
              "            total += Pick(i % 2, 10 / (i % 2));\n"
              "        }\n"
              "        return total;\n"
              "    }\n"
              "    public static void Main() {\n"
              "        Check(Many(2000000), 17000000);\n"
              "    }\n")
          == 0);
    std::filesystem::remove_all(Directory());
}

TEST_CASE("an executable holds many objects at one time")
{
    // Every level of the recursion holds a closure until the levels below
    // it have returned.
    CHECK(Run("Held",
              "    public static int Deep(_ int n) {\n"
              "        auto here = \\(int v) -> v + n;\n"
              "        if (n <= 0) { return here(0); }\n"
              "        return Deep(n - 1) + here(1) - n;\n"
              "    }\n"
              "    public static void Main() { Check(Deep(4000), 4000); }\n")
          == 0);
    std::filesystem::remove_all(Directory());
}

#endif
