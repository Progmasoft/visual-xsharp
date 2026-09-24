// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

#include <fmt/format.h>

#include "Visual/XSharp/Interactive/Arguments.hpp"

namespace Visual::XSharp::Interactive
{
    auto
    ParseArguments(int argc, char **argv) -> Request
    {
        if (argc == 1)
            return {};
        if (argc == 2 && std::string_view(argv[1]) == "-Help")
            return Request{ RequestKind::Help, {}, {} };
        if (argc == 3 && std::string_view(argv[1]) == "-Eval")
        {
            if (argv[2][0] == '\0')
                return Request{ RequestKind::Error, {}, "-Eval requires a non-empty Visual X# expression" };
            return Request{ RequestKind::Evaluate, argv[2], {} };
        }
        if (argc >= 2 && std::string_view(argv[1]) == "-Eval")
            return Request{ RequestKind::Error, {}, "-Eval accepts exactly one expression argument" };
        return Request{ RequestKind::Error,
                        {},
                        fmt::format("unknown or misplaced argument '{}'; use -Help for usage", argc > 1 ? argv[1] : "") };
    }

    void
    PrintHelp()
    {
        fmt::print("Visual X# Interactive\n\n"
                   "Usage:\n"
                   "  vxsi\n"
                   "  vxsi -Eval <expression>\n"
                   "  vxsi -Help\n\n"
                   "With no arguments, vxsi opens a persistent ORC LLJIT session. Each complete input line is\n"
                   "compiled by the Visual X# Haskell frontend and lowered through Core, CorePrep, Xpp, Xmm,\n"
                   "and LLVM before execution. The most recent supported scalar result is available as `vxsiPrevious`.\n\n"
                   "REPL commands:\n"
                   "  :help                 show this help\n"
                   "  :type <expression>   compile and report the expression type without running it\n"
                   "  :history              list successful input expressions\n"
                   "  :reset                clear results, history, and loaded JIT modules\n"
                   "  :quit                 leave the REPL (EOF also exits)\n\n"
                   "The current host invocation ABI prints void, booleans, characters, integers up to 64 bits,\n"
                   "and 32/64-bit floating values. Other Visual X# values are type-checked but not yet callable\n"
                   "through the scalar REPL ABI.\n");
    }
} // namespace Visual::XSharp::Interactive
