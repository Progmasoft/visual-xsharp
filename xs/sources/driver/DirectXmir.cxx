/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "direct_xmir.h"

extern "C"
{
#include "direct_xlil.h"
#include "xs/compiler_core.h"
}

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>

namespace
{
void print_diagnostics(const char *input_path, const XsCompilerCoreDirectIrSession *session)
{
    const std::uint64_t count = xslang_compiler_core_direct_ir_diagnostic_count(session);
    for(std::uint64_t index = 0; index < count; ++index)
    {
        std::uint64_t length = 0;
        const std::uint8_t *text = xslang_compiler_core_direct_ir_diagnostic_text(session, index, &length);
        if(text == nullptr)
            continue;
        std::fprintf(stderr, "xs: %s: ", input_path);
        static_cast<void>(std::fwrite(text, 1, static_cast<std::size_t>(length), stderr));
        std::fputc('\n', stderr);
    }
}
} // namespace

bool xs_driver_build_direct_xmir(const char *input_path, const char *text, std::size_t length)
{
    XsCompilerCoreDirectIrSession *session = nullptr;
    const XsCompilerCoreFfiStatus status = xslang_compiler_core_direct_xmir_create(
        reinterpret_cast<const std::uint8_t *>(text), static_cast<std::uint64_t>(length), &session);
    if(status != XS_COMPILER_CORE_FFI_OK || session == nullptr)
    {
        std::fprintf(stderr, "xs: XMIR compiler core session could not be created (status %u)\n",
                     static_cast<unsigned int>(status));
        return false;
    }
    print_diagnostics(input_path, session);
    std::uint64_t xlil_length = 0;
    const std::uint8_t *xlil = xslang_compiler_core_direct_ir_xlil_text(session, &xlil_length);
    bool success = xlil != nullptr && xlil_length != 0 && xlil_length <= std::numeric_limits<std::size_t>::max();
    if(success)
        success = xs_driver_build_direct_xlil(input_path, reinterpret_cast<const char *>(xlil),
                                              static_cast<std::size_t>(xlil_length));
    else if(xslang_compiler_core_direct_ir_diagnostic_count(session) == 0)
        std::fputs("xs: XMIR compiler core did not produce a verified XLIL module\n", stderr);
    xslang_compiler_core_direct_ir_free(session);
    return success;
}
