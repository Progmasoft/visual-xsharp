// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1
#pragma once

#include <string>

namespace Visual::XSharp::Interactive
{
    enum class RequestKind
    {
        Repl,
        Evaluate,
        Help,
        Error
    };

    struct Request final
    {
        RequestKind kind{ RequestKind::Repl };
        std::string expression;
        std::string diagnostic;
    };

    /** Parse the deliberately small public vxsi command line. */
    [[nodiscard]] auto
    ParseArguments(int argc, char **argv) -> Request;

    void
    PrintHelp();
} // namespace Visual::XSharp::Interactive
