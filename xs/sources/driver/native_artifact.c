/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "native_artifact.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <spawn.h>
#include <sys/wait.h>

extern char **environ;

static size_t source_stem_length(const char *base)
{
  size_t length = strlen(base);
  static const char *const extensions[] = {".xlil", ".xmir", ".xhir", ".xs"};
  for(size_t index = 0; index < sizeof(extensions) / sizeof(*extensions); ++index)
  {
    size_t extension_length = strlen(extensions[index]);
    if(length >= extension_length && strcmp(base + length - extension_length, extensions[index]) == 0)
      return length - extension_length;
  }
  return length;
}

char *xs_driver_native_artifact_path(const char *input_path, const char *extension)
{
  if(input_path == nullptr || extension == nullptr)
    return nullptr;
  const char *slash = strrchr(input_path, '/');
  const char *base = slash == nullptr ? input_path : slash + 1;
  size_t directory_length = slash == nullptr ? 0U : (size_t)(base - input_path);
  size_t base_length = source_stem_length(base);
  size_t extension_length = strlen(extension);
  char *path = malloc(directory_length + base_length + extension_length + 1U);
  if(path == nullptr)
    return nullptr;
  if(directory_length != 0U)
    memcpy(path, input_path, directory_length);
  memcpy(path + directory_length, base, base_length);
  memcpy(path + directory_length + base_length, extension, extension_length);
  path[directory_length + base_length + extension_length] = '\0';
  return path;
}

bool xs_driver_execute_native_artifact(const char *input_path, int *exit_code)
{
  if(exit_code == nullptr)
    return false;
  *exit_code = 1;
  char *path = xs_driver_native_artifact_path(input_path, ".xse");
  if(path == nullptr)
  {
    fprintf(stderr, "xs: out of memory while preparing the native executable path\n");
    return false;
  }
  char *const arguments[] = {path, nullptr};
  pid_t process = 0;
  int spawn_status = posix_spawn(&process, path, nullptr, nullptr, arguments, environ);
  if(spawn_status != 0)
  {
    fprintf(stderr, "xs: could not execute '%s': %s\n", path, strerror(spawn_status));
    free(path);
    return false;
  }
  int status = 0;
  if(waitpid(process, &status, 0) < 0)
  {
    fprintf(stderr, "xs: could not wait for '%s': %s\n", path, strerror(errno));
    free(path);
    return false;
  }
  free(path);
  if(WIFEXITED(status))
  {
    *exit_code = WEXITSTATUS(status);
    return true;
  }
  if(WIFSIGNALED(status))
  {
    int signal_number = WTERMSIG(status);
    fprintf(stderr, "xs: native executable terminated by signal %d\n", signal_number);
    *exit_code = 128 + signal_number;
    return true;
  }
  fprintf(stderr, "xs: native executable ended without an exit status\n");
  return false;
}

int xs_driver_run_native_artifact(const char *input_path)
{
  int exit_code = 1;
  return xs_driver_execute_native_artifact(input_path, &exit_code) ? exit_code : 1;
}
