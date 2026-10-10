/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 *
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

 */

#include "Compiler/Cli/Commands/Commands.hpp"
#include "Visual/XSharp/Support/CompilerStack.hpp"

int
main(int argc, char **argv)
{
    // Every command runs on the compiler stack: the nesting limits of the
    // frontend are stated against its size, not against the stack the
    // operating system gives the process.
    return Visual::XSharp::Support::RunOnCompilerStack([argc, argv] {
        return Visual::XSharp::Cli::Run(argc, argv);
    });
}
