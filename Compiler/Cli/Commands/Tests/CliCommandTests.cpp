// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

#include <Progmasoft/Catch3/Assertions.hpp>
#include <array>

#include "Compiler/Cli/Commands/Commands.hpp"

TEST_CASE("reserved VXCI header generation fails before project discovery")
{
    std::array<char, 4> executable{ 'v', 'x', 's', '\0' };
    std::array<char, 6> build{ 'b', 'u', 'i', 'l', 'd', '\0' };
    std::array<char, 6> fileOption{ '-', 'F', 'i', 'l', 'e', '\0' };
    std::array<char, 9> source{ 'M', 'a', 'i', 'n', '.', 'v', 'x', 's', '\0' };
    std::array<char, 8> header{ '-', 'H', 'e', 'a', 'd', 'e', 'r', '\0' };
    std::array<char *, 6> arguments{ executable.data(), build.data(),
                                     fileOption.data(), source.data(),
                                     header.data(),     nullptr };

    CHECK(Visual::XSharp::Cli::Run(5, arguments.data()) == 2);
}
