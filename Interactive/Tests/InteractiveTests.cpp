// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

#include <Progmasoft/Catch3/Assertions.hpp>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "Interactive/Runtime/Source.hpp"
#include "Interactive/Runtime/Value.hpp"
#include "Visual/XSharp/Interactive/Arguments.hpp"
#include "Visual/XSharp/Interactive/Session.hpp"

namespace
{
    namespace Interactive = Visual::XSharp::Interactive;
    namespace Llvm = Visual::XSharp::Backend::LLVM;
    namespace Core = Llvm::Core;
    namespace Runtime = Visual::XSharp::Interactive::Runtime;

    class Invocation final
    {
    public:
        explicit Invocation(std::initializer_list<std::string> arguments)
            : storage_(arguments)
        {
            argv_.reserve(storage_.size() + 1U);
            argv_.push_back(const_cast<char *>("vxsi"));
            for (auto &argument : storage_)
                argv_.push_back(argument.data());
            request_
                = Interactive::ParseArguments(static_cast<int>(argv_.size()),
                                              argv_.data());
        }

        [[nodiscard]] auto
        Request() const noexcept -> const Interactive::Request &
        {
            return request_;
        }

    private:
        std::vector<std::string> storage_;
        std::vector<char *> argv_;
        Interactive::Request request_;
    };

} // namespace

TEST_CASE(
    "vxsi defaults to its interactive mode and keeps one-shot syntax narrow",
    "[vxsi][arguments]")
{
    const Invocation repl{};
    REQUIRE(repl.Request().kind == Interactive::RequestKind::Repl);

    const Invocation evaluate{ "-Eval", "5 + 5" };
    REQUIRE(evaluate.Request().kind == Interactive::RequestKind::Evaluate);
    REQUIRE(evaluate.Request().expression == "5 + 5");

    const Invocation help{ "-Help" };
    REQUIRE(help.Request().kind == Interactive::RequestKind::Help);
}

TEST_CASE("vxsi rejects truncated and ambiguous public invocations",
          "[vxsi][arguments][errors]")
{
    const Invocation missing{ "-Eval" };
    REQUIRE(missing.Request().kind == Interactive::RequestKind::Error);
    REQUIRE(missing.Request().diagnostic.find("exactly one")
            != std::string::npos);

    const Invocation empty{ "-Eval", "" };
    REQUIRE(empty.Request().kind == Interactive::RequestKind::Error);
    REQUIRE(empty.Request().diagnostic.find("non-empty") != std::string::npos);

    const Invocation extra{ "-Eval", "1 + 2", "ignored" };
    REQUIRE(extra.Request().kind == Interactive::RequestKind::Error);

    const Invocation misplaced{ "-Bogus" };
    REQUIRE(misplaced.Request().kind == Interactive::RequestKind::Error);
    REQUIRE(misplaced.Request().diagnostic.find("-Help") != std::string::npos);
}

TEST_CASE("generated REPL cells use a unique namespace and preserve a typed "
          "previous result",
          "[vxsi][source]")
{
    const Llvm::JitValue previous{ Core::Type::int64(), std::int64_t{ 10 } };
    const auto first
        = Runtime::BuildCellSource(7U, "vxsiPrevious * 3", previous);
    const auto second = Runtime::BuildCellSource(8U, "5 + 5", std::nullopt);
    REQUIRE(first.has_value());
    REQUIRE(second.has_value());
    REQUIRE(first->find("namespace VisualXSharp.Interactive.Cell7;")
            != std::string::npos);
    REQUIRE(second->find("namespace VisualXSharp.Interactive.Cell8;")
            != std::string::npos);
    REQUIRE(first->find("class Session") != std::string::npos);
    REQUIRE(first->find("public static auto Evaluate()") != std::string::npos);
    REQUIRE(first->find("int vxsiPrevious = 10;") != std::string::npos);
    REQUIRE(first->find("vxsiPrevious * 3") != std::string::npos);
    REQUIRE(first->find(".vxs") == std::string::npos);
}

TEST_CASE(
    "cell source omits uninitialized session state and enforces input limits",
    "[vxsi][source][limits]")
{
    const auto source = Runtime::BuildCellSource(0U, "5 + 5", std::nullopt);
    REQUIRE(source.has_value());
    REQUIRE(source->find("_ =") == std::string::npos);
    REQUIRE(source->find("5 + 5") != std::string::npos);
    REQUIRE_FALSE(Runtime::BuildCellSource(1U, "", std::nullopt));
    REQUIRE_FALSE(Runtime::BuildCellSource(1U,
                                           std::string(1024U * 1024U + 1U, '1'),
                                           std::nullopt));
}

TEST_CASE("REPL scalar values have lossless Visual X# bindings",
          "[vxsi][state]")
{
    using Llvm::JitValue;
    REQUIRE(Runtime::SourceBinding(
                JitValue{ Core::Type::int8(), std::int64_t{ -8 } })
            == "byte vxsiPrevious = -8;");
    REQUIRE(Runtime::SourceBinding(JitValue{ Core::Type::uint64(),
                                             std::uint64_t{ 0xFFFFFFFFULL } })
            == "uint vxsiPrevious = 4294967295;");
    REQUIRE(Runtime::SourceBinding(JitValue{ Core::Type::boolean(), true })
            == "bool vxsiPrevious = true;");
    REQUIRE(Runtime::SourceBinding(JitValue{ Core::Type::character(), U'\n' })
            == "char vxsiPrevious = '\\u000A';");
    REQUIRE(Runtime::SourceBinding(JitValue{ Core::Type::float32(), 1.25 })
            == "lfloat vxsiPrevious = 1.25;");
    REQUIRE_FALSE(Runtime::SourceBinding(
        JitValue{ Core::Type::int128(), std::int64_t{ 10 } }));
    REQUIRE_FALSE(
        Runtime::SourceBinding(JitValue{ Core::Type::float128(), 1.25 }));
}

TEST_CASE("REPL bindings reject mismatched payloads and values outside their "
          "declared type",
          "[vxsi][state][safety]")
{
    using Llvm::JitValue;
    REQUIRE(Runtime::SourceBinding(
                JitValue{ Core::Type::boolean(), std::int64_t{ 1 } })
            == std::nullopt);
    REQUIRE(Runtime::SourceBinding(JitValue{ Core::Type::int8(), true })
            == std::nullopt);
    REQUIRE(Runtime::SourceBinding(
                JitValue{ Core::Type::character(), char32_t{ 0xd800U } })
            == std::nullopt);
    REQUIRE(Runtime::SourceBinding(
                JitValue{ Core::Type::character(), char32_t{ 0x110000U } })
            == std::nullopt);

    REQUIRE(Runtime::SourceBinding(
                JitValue{ Core::Type::int8(), std::int64_t{ -128 } })
            == "byte vxsiPrevious = -128;");
    REQUIRE(Runtime::SourceBinding(
                JitValue{ Core::Type::int8(), std::int64_t{ 127 } })
            == "byte vxsiPrevious = 127;");
    REQUIRE(Runtime::SourceBinding(
                JitValue{ Core::Type::int8(), std::int64_t{ -129 } })
            == std::nullopt);
    REQUIRE(Runtime::SourceBinding(
                JitValue{ Core::Type::int8(), std::int64_t{ 128 } })
            == std::nullopt);
    REQUIRE(Runtime::SourceBinding(
                JitValue{ Core::Type::uint8(), std::uint64_t{ 255 } })
            == "ubyte vxsiPrevious = 255;");
    REQUIRE(Runtime::SourceBinding(
                JitValue{ Core::Type::uint8(), std::uint64_t{ 256 } })
            == std::nullopt);
    REQUIRE(Runtime::SourceBinding(
                JitValue{ Core::Type::uint64(),
                          std::uint64_t{ 18446744073709551615ULL } })
            == "uint vxsiPrevious = 18446744073709551615;");

    REQUIRE(Runtime::SourceBinding(JitValue{ Core::Type::float32(), 0.1 })
            == "lfloat vxsiPrevious = 0.100000001;");
    REQUIRE(Runtime::SourceBinding(JitValue{ Core::Type::float64(), -0.0 })
            == "float vxsiPrevious = -0;");
}

TEST_CASE("REPL display retains source signedness, width, and scalar category",
          "[vxsi][presentation]")
{
    using Llvm::JitValue;
    REQUIRE(Interactive::FormatValue(
                JitValue{ Core::Type::int64(),
                          std::int64_t{ -9223372036854775807LL } })
            == "-9223372036854775807");
    REQUIRE(Interactive::FormatValue(
                JitValue{ Core::Type::uint64(),
                          std::uint64_t{ 18446744073709551615ULL } })
            == "18446744073709551615");
    REQUIRE(Interactive::FormatValue(JitValue{ Core::Type::boolean(), false })
            == "false");
    REQUIRE(Interactive::FormatValue(JitValue{ Core::Type::character(), U'A' })
            == "U+0041");
    REQUIRE(Interactive::FormatType(Core::Type::int32()) == "long");
    REQUIRE(Interactive::FormatType(Core::Type::unit()) == "void");
}

TEST_CASE("REPL symbol discovery honors cell identity and unique signatures",
          "[vxsi][symbols]")
{
    visual_xsharp::xmm::Module module;
    module.name = { U"VisualXSharp", U"Interactive", U"Cell12" };
    visual_xsharp::xmm::Function evaluate;
    evaluate.symbol = { 24U, U"Evaluate" };
    evaluate.return_type = Core::Type::int64();
    module.functions.push_back(evaluate);

    REQUIRE(Runtime::EvaluationSymbol(module, 12U)
            == "VisualXSharp.Interactive.Cell12.Evaluate.24");
    REQUIRE_FALSE(Runtime::EvaluationSymbol(module, 11U));

    module.functions.push_back(evaluate);
    REQUIRE_FALSE(Runtime::EvaluationSymbol(module, 12U));
}
