// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

#include <cstdint>
#include <fmt/format.h>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>

#include "Source.hpp"
#include "Value.hpp"
#include "Visual/XSharp/Core/Scalar.hpp"
#include "Visual/XSharp/Interactive/Session.hpp"

namespace Visual::XSharp::Interactive
{
    namespace
    {
        [[nodiscard]] auto
        Error(std::string message) -> CellResult
        {
            return { CellStatus::Error, std::move(message), std::nullopt };
        }

        [[nodiscard]] auto
        EvaluationFunction(const visual_xsharp::xmm::Module &module)
            -> const visual_xsharp::xmm::Function *
        {
            const visual_xsharp::xmm::Function *found{};
            for (const auto &function : module.functions)
            {
                if (function.symbol.spelling != U"Evaluate")
                    continue;
                if (found != nullptr)
                    return nullptr;
                found = &function;
            }
            return found;
        }

        [[nodiscard]] auto
        BackendError(const Backend::LLVM::JitError &error) -> std::string
        {
            return fmt::format("{}: {}", error.code, error.message);
        }
    } // namespace

    auto
    Session::Evaluate(std::string_view expression) -> CellResult
    {
        auto result = Compile(expression, true);
        if (result.status == CellStatus::Value || result.status == CellStatus::Void)
            history_.Append(expression);
        return result;
    }

    auto
    Session::TypeOf(std::string_view expression) -> CellResult
    {
        return Compile(expression, false);
    }

    auto
    Session::Reset() -> std::optional<std::string>
    {
        if (const auto issue = jit_.Reset())
            return BackendError(*issue);
        nextCell_ = 0U;
        previous_.reset();
        history_.Clear();
        return std::nullopt;
    }

    auto
    Session::History() const noexcept -> const std::deque<std::string> &
    {
        return history_.Entries();
    }

    auto
    Session::Compile(std::string_view expression, bool execute) -> CellResult
    {
        if (expression.empty())
            return Error("enter a Visual X# expression, or use :help for REPL commands");
        if (expression.size() > 1024U * 1024U)
            return Error("one Visual X# expression cannot exceed 1 MiB");

        Runtime::ScratchCell cell;
        if (const auto issue = Runtime::WriteCellSource(cell, nextCell_, expression, previous_))
            return Error(*issue);
        if (Runtime::RunFrontend(cell.SourcePath(), cell.CorePath()) != 0)
            return Error("frontend rejected this input; the source diagnostic is shown above");
        auto bytes = Runtime::ReadCore(cell.CorePath());
        if (!bytes)
            return Error("frontend did not produce a readable Core artifact");

        visual_xsharp::PipelineOptions options;
        if (!execute)
            options.stop_after = visual_xsharp::PipelineStop::Xmm;
        auto pipeline = Visual::XSharp::Pipeline::ConsumeCore(*bytes, options);
        if (!pipeline)
        {
            if (pipeline.coreWireError)
                return Error(fmt::format("Core artifact error at byte {}: {}",
                                         pipeline.coreWireError->offset,
                                         pipeline.coreWireError->message));
            if (pipeline.llvm_error)
                return Error(fmt::format("{}: {}", pipeline.llvm_error->code, pipeline.llvm_error->message));
            if (!pipeline.coreVerificationIssues.empty())
                return Error(fmt::format("Core verifier rejected the expression: {} issue(s)",
                                         pipeline.coreVerificationIssues.size()));
            if (!pipeline.verification_issues.empty())
                return Error(fmt::format("CorePrep verifier rejected the expression: {} issue(s)",
                                         pipeline.verification_issues.size()));
            if (!pipeline.xppVerificationIssues.empty())
                return Error(fmt::format("Xpp verifier rejected the expression: {} issue(s)",
                                         pipeline.xppVerificationIssues.size()));
            if (!pipeline.xmmVerificationIssues.empty())
                return Error(fmt::format("Xmm verifier rejected the expression: {} issue(s)",
                                         pipeline.xmmVerificationIssues.size()));
            return Error("compiler pipeline stopped before producing an executable expression");
        }
        if (!pipeline.xmm)
            return Error("compiler pipeline did not retain its verified Xmm module");

        const auto symbol = Runtime::EvaluationSymbol(*pipeline.xmm, nextCell_);
        if (!symbol)
            return Error("compiler did not emit the unique zero-argument Evaluate function for this cell");
        const auto *function = EvaluationFunction(*pipeline.xmm);
        if (function == nullptr)
            return Error("compiler emitted an ambiguous Evaluate function for this cell");

        if (!execute)
        {
            const auto typeName = FormatType(function->return_type);
            return { CellStatus::Type, typeName, std::nullopt };
        }
        if (!pipeline.llvm)
            return Error("LLVM lowering did not produce bitcode for this cell");
        // A module is never re-used after insertion, even if lookup or native
        // invocation fails. Advancing before insertion prevents a later input
        // from colliding with residual JIT symbols from a partially failed cell.
        ++nextCell_;
        if (const auto issue = jit_.AddModule(pipeline.llvm->bitcode, *symbol, *symbol, function->return_type))
            return Error(BackendError(*issue));

        auto invocation = jit_.InvokeScalar(*symbol, function->return_type);
        if (!invocation)
            return Error(BackendError(*invocation.error));

        if (std::holds_alternative<std::monostate>(invocation.value->payload))
        {
            previous_.reset();
            return { CellStatus::Void, "void", std::move(invocation.value) };
        }
        // Keep the result only when it has a lossless source spelling. Wider
        // values remain executable and printable by the backend, but never
        // silently narrow to host int64 merely to implement the `_` binding.
        if (Runtime::SourceBinding(*invocation.value))
            previous_ = *invocation.value;
        else
            previous_.reset();
        return { CellStatus::Value,
                 fmt::format("{} : {}", Runtime::DisplayValue(*invocation.value), FormatType(function->return_type)),
                 std::move(invocation.value) };
    }

    auto
    FormatValue(const Backend::LLVM::JitValue &value) -> std::string
    {
        return Runtime::DisplayValue(value);
    }

    auto
    FormatType(const visual_xsharp::core::Type &type) -> std::string
    {
        return Runtime::DisplayType(type);
    }
} // namespace Visual::XSharp::Interactive
