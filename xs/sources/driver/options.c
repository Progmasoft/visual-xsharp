/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <xs-lang.chess031@slmails.com>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "options.h"

#include <string.h>

XsCompilerSettings xs_cli_default_compiler_settings(void)
{
  return (XsCompilerSettings){.warning_level = XS_WARNING_MEDIUM, .verbose = true};
}

const char *xs_cli_warning_level_name(XsWarningLevel level)
{
  switch(level)
  {
  case XS_WARNING_ALL:
    return "all";
  case XS_WARNING_MEDIUM:
    return "medium";
  case XS_WARNING_LOW:
    return "low";
  case XS_WARNING_NONE:
    return "none";
  }
  return "medium";
}

static bool parse_warning_level(const char *text, XsWarningLevel *level)
{
  for(XsWarningLevel candidate = XS_WARNING_NONE; candidate <= XS_WARNING_ALL; ++candidate)
  {
    if(strcmp(text, xs_cli_warning_level_name(candidate)) == 0)
    {
      *level = candidate;
      return true;
    }
  }
  return false;
}

static bool parse_bool(const char *text, bool *value)
{
  if(strcmp(text, "true") == 0)
  {
    *value = true;
    return true;
  }
  if(strcmp(text, "false") == 0)
  {
    *value = false;
    return true;
  }
  return false;
}

void xs_cli_apply_compiler_overrides(const XsCliOptions *options, XsCompilerSettings *settings)
{
  if(options->warning_override)
    settings->warning_level = options->compiler.warning_level;
  if(options->werror_override)
    settings->warnings_as_errors = options->compiler.warnings_as_errors;
  if(options->verbose_override)
    settings->verbose = options->compiler.verbose;
  if(options->xgc_override)
    settings->xgc_enabled = options->compiler.xgc_enabled;
}

const char *xs_cli_output_extension(XsBuildOutput output)
{
  switch(output)
  {
  case XS_BUILD_OUTPUT_HIR:
    return ".xhir";
  case XS_BUILD_OUTPUT_MIR:
    return ".xmir";
  case XS_BUILD_OUTPUT_XLIL:
    return ".xlil";
  case XS_BUILD_OUTPUT_NONE:
    return "";
  }
  return "";
}

static bool parse_output_kind(const char *text, XsBuildOutput *output)
{
  if(strcmp(text, "hir") == 0)
  {
    *output = XS_BUILD_OUTPUT_HIR;
    return true;
  }
  if(strcmp(text, "mir") == 0)
  {
    *output = XS_BUILD_OUTPUT_MIR;
    return true;
  }
  if(strcmp(text, "xlil") == 0)
  {
    *output = XS_BUILD_OUTPUT_XLIL;
    return true;
  }
  return false;
}

static bool set_output(XsCliOptions *options, XsBuildOutput output)
{
  if(options->output != XS_BUILD_OUTPUT_NONE)
    return false;
  options->output = output;
  return true;
}

static bool parse_output_flag(const char *flag, XsCliOptions *options)
{
  if(strcmp(flag, "--hir") == 0)
    return set_output(options, XS_BUILD_OUTPUT_HIR);
  if(strcmp(flag, "--mir") == 0)
    return set_output(options, XS_BUILD_OUTPUT_MIR);
  if(strcmp(flag, "--xlil") == 0)
    return set_output(options, XS_BUILD_OUTPUT_XLIL);
  return false;
}

bool xs_cli_parse(int argc, char **argv, XsCliOptions *options)
{
  if(argc < 2)
    return false;
  if(strcmp(argv[1], "resolve") == 0)
  {
    if(argc != 2)
      return false;
    *options = (XsCliOptions){.command = argv[1], .compiler = xs_cli_default_compiler_settings()};
    return true;
  }
  if(strcmp(argv[1], "check") != 0 && strcmp(argv[1], "build") != 0 && strcmp(argv[1], "run") != 0 &&
     strcmp(argv[1], "test") != 0)
    return false;
  *options = (XsCliOptions){.command = argv[1], .compiler = xs_cli_default_compiler_settings()};
  if(argc == 2)
    return true;
  for(int i = 2; i < argc; ++i)
  {
    if(strcmp(argv[i], "-file") == 0)
    {
      if(++i >= argc || options->file_path != nullptr)
        return false;
      options->file_path = argv[i];
    }
    else if(strcmp(argv[i], "--module") == 0)
    {
      if(++i >= argc || options->module_path != nullptr)
        return false;
      options->module_path = argv[i];
    }
    else if(strcmp(argv[i], "--output") == 0)
    {
      if(++i >= argc || options->output != XS_BUILD_OUTPUT_NONE)
        return false;
      if(!parse_output_kind(argv[i], &options->output))
        return false;
    }
    else if(strcmp(argv[i], "--warning") == 0)
    {
      if(++i >= argc || options->warning_override || !parse_warning_level(argv[i], &options->compiler.warning_level))
        return false;
      options->warning_override = true;
    }
    else if(strcmp(argv[i], "--werror") == 0)
    {
      if(++i >= argc || options->werror_override || !parse_bool(argv[i], &options->compiler.warnings_as_errors))
        return false;
      options->werror_override = true;
    }
    else if(strcmp(argv[i], "--verbose") == 0)
    {
      if(++i >= argc || options->verbose_override || !parse_bool(argv[i], &options->compiler.verbose))
        return false;
      options->verbose_override = true;
    }
    else if(strcmp(argv[i], "--xgc-enabled") == 0)
    {
      if(++i >= argc || options->xgc_override || !parse_bool(argv[i], &options->compiler.xgc_enabled))
        return false;
      options->xgc_override = true;
    }
    else if(!parse_output_flag(argv[i], options))
    {
      return false;
    }
  }
  if(options->file_path == nullptr)
    return options->output == XS_BUILD_OUTPUT_NONE || strcmp(options->command, "build") == 0;
  if(options->file_path != nullptr)
  {
    if(options->module_path != nullptr)
      return false;
    if(strcmp(options->command, "check") == 0 || strcmp(options->command, "test") == 0)
      return options->output == XS_BUILD_OUTPUT_NONE;
    if(strcmp(options->command, "run") == 0)
      return options->output == XS_BUILD_OUTPUT_NONE;
    return strcmp(options->command, "build") == 0;
  }
  return false;
}

void xs_cli_print_usage(FILE *stream)
{
  fprintf(stream, "usage: xs --version\n");
  fprintf(stream, "usage: xs resolve\n");
  fprintf(stream, "usage: xs <check|build|run|test> [--output hir|mir|xlil] [--module <directory>]\n");
  fprintf(stream, "       [--warning all|medium|low|none] [--werror true|false] [--verbose true|false]\n");
  fprintf(stream, "       [--xgc-enabled true|false]\n");
  fprintf(stream, "usage: xs <build|run> -file <Main.xs>\n");
  fprintf(stream, "usage: xs test [-file <Test.xs>]\n");
  fprintf(stream, "usage: xs build [--output hir|mir|xlil] -file <input>\n");
  fprintf(stream, "usage: xs build [--hir|--mir|--xlil] -file <input>\n");
}
