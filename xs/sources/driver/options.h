/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef XS_DRIVER_OPTIONS_H
#define XS_DRIVER_OPTIONS_H

#include "xs/diagnostic.h"

#include <stdio.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef enum
  {
    XS_BUILD_OUTPUT_NONE,
    XS_BUILD_OUTPUT_HIR,
    XS_BUILD_OUTPUT_MIR,
    XS_BUILD_OUTPUT_XLIL,
  } XsBuildOutput;

  typedef struct
  {
    XsWarningLevel warning_level;
    bool warnings_as_errors;
    bool verbose;
  } XsCompilerSettings;

  typedef struct
  {
    const char *command;
    const char *file_path;
    const char *module_path;
    XsBuildOutput output;
    XsCompilerSettings compiler;
    bool warning_override;
    bool werror_override;
    bool verbose_override;
  } XsCliOptions;

  typedef enum
  {
    XS_CLI_PARSE_ERROR,
    XS_CLI_PARSE_READY,
    XS_CLI_PARSE_EXIT,
  } XsCliParseResult;

  XsCompilerSettings xs_cli_default_compiler_settings(void);
  void xs_cli_apply_compiler_overrides(const XsCliOptions *options, XsCompilerSettings *settings);
  const char *xs_cli_warning_level_name(XsWarningLevel level);
  const char *xs_cli_output_extension(XsBuildOutput output);
  XsCliParseResult xs_cli_parse(int argc, char **argv, XsCliOptions *options);
  void xs_cli_options_free(XsCliOptions *options);

#ifdef __cplusplus
}
#endif

#endif
