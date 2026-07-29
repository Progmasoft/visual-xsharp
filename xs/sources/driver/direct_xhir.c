/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "direct_xhir.h"

#include "direct_xlil.h"

#include "xs/compiler_core.h"

#include <stdint.h>
#include <stdio.h>

static void print_diagnostics(const char *input_path, const XsCompilerCoreDirectIrSession *session)
{
  uint64_t count = xslang_compiler_core_direct_ir_diagnostic_count(session);
  for(uint64_t index = 0; index < count; ++index)
  {
    uint64_t length = 0;
    const uint8_t *text = xslang_compiler_core_direct_ir_diagnostic_text(session, index, &length);
    if(text == nullptr)
      continue;
    fprintf(stderr, "xs: %s: ", input_path);
    (void)fwrite(text, 1, (size_t)length, stderr);
    fputc('\n', stderr);
  }
}

bool xs_driver_build_direct_xhir(const char *input_path, const char *text, size_t length)
{
  XsCompilerCoreDirectIrSession *session = nullptr;
  XsCompilerCoreFfiStatus status =
      xslang_compiler_core_direct_xhir_create((const uint8_t *)text, (uint64_t)length, &session);
  if(status != XS_COMPILER_CORE_FFI_OK || session == nullptr)
  {
    fprintf(stderr, "xs: XHIR compiler core session could not be created (status %u)\n", (unsigned int)status);
    return false;
  }
  print_diagnostics(input_path, session);
  uint64_t xlil_length = 0;
  const uint8_t *xlil = xslang_compiler_core_direct_ir_xlil_text(session, &xlil_length);
  bool success = xlil != nullptr && xlil_length != 0 && xlil_length <= SIZE_MAX;
  if(success)
    success = xs_driver_build_direct_xlil(input_path, (const char *)xlil, (size_t)xlil_length);
  else if(xslang_compiler_core_direct_ir_diagnostic_count(session) == 0)
    fprintf(stderr, "xs: XHIR compiler core did not produce a verified XLIL module\n");
  xslang_compiler_core_direct_ir_free(session);
  return success;
}
