/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 * Native C++ entry point for the unified Visual X# command-line driver.
 */

#pragma once

namespace Visual::XSharp::Cli
{
    // Run owns argument dispatch only. Frontend and backend stages remain
    // separate libraries so the CLI does not become the compiler architecture.
    [[nodiscard]] auto Run(int argc, char **argv) -> int;
}
