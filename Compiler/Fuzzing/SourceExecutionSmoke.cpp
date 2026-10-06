// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

#include <llvm/Support/raw_ostream.h>

#include "BranchingExecutionCases.hpp"
#include "ExpressionExecutionCases.hpp"
#include "LeavingExecutionCases.hpp"
#include "Visual/XSharp/Support/CompilerStack.hpp"

// The executable regressions of the three large tables: every program of the
// expression, branching and leaving tables runs through CorePrep, Xpp, Xmm,
// LLVM and the ORC JIT, unoptimized and optimized, and must return its expected
// value from both. They are a program of their own, beside `source_fuzz_smoke`
// and `source_feature_smoke`, because each is a deterministic check under one
// process watchdog, and a watchdog is meant to end a run that never
// finishes, not to bound the size of a test table.

namespace
{
    int
    Smoke()
    {
        // Hand-written results for assignments and increments used as values
        // and for loops used as expressions.
        Visual::XSharp::Fuzzing::ExerciseExpressionCases();
        // Hand-written results for match, if expressions and guard.
        Visual::XSharp::Fuzzing::ExerciseBranchingCases();
        // Hand-written results for expressions that leave instead of
        // yielding a value.
        Visual::XSharp::Fuzzing::ExerciseLeavingCases();
        return 0;
    }
} // namespace

int
main()
{
    // The programs include ones nested up to the frontend's limits, which
    // only compile on the stack the compiler runs on in `vxs`.
    return Visual::XSharp::Support::RunOnCompilerStack([] {
        const auto result = Smoke();
        // What the compiler stack held for all of the above, the programs
        // at the nesting limits among them. The line is how the figure is
        // read on platforms where nobody measures by hand; zero means the
        // platform does not report it.
        llvm::errs() << "compiler stack committed: "
                     << Visual::XSharp::Support::CommittedStackBytes() / 1024U
                     << " KiB\n";
        return result;
    });
}
