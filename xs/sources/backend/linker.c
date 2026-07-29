/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "xs/backend/linker.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <spawn.h>
#include <sys/wait.h>

extern char **environ;

static XsBackendStatus linker_error(XsBackendError *error, XsBackendStatus status, const char *message)
{
  if(error != nullptr)
  {
    error->status = status;
    snprintf(error->message, sizeof(error->message), "%s", message);
  }
  return status;
}

XsBackendStatus xs_linker_invoke(const XsLinkerInvocation *invocation, int *exit_code, XsBackendError *error)
{
  if(error != nullptr)
    *error = (XsBackendError){.status = XS_BACKEND_OK};
  if(invocation == nullptr || invocation->program == nullptr || invocation->program[0] == '\0' || exit_code == nullptr ||
     (invocation->argument_count != 0 && invocation->arguments == nullptr))
    return linker_error(error, XS_BACKEND_INVALID_ARGUMENT,
                        "valid linker invocation and exit-code output are required");

  char **arguments = calloc(invocation->argument_count + 2, sizeof(*arguments));
  if(arguments == nullptr)
    return linker_error(error, XS_BACKEND_SYSTEM_ERROR, "out of memory while preparing linker invocation");
  arguments[0] = (char *)invocation->program;
  for(size_t i = 0; i < invocation->argument_count; ++i)
    arguments[i + 1] = (char *)invocation->arguments[i];

  pid_t process = 0;
  int spawn_status = posix_spawnp(&process, invocation->program, nullptr, nullptr, arguments, environ);
  free(arguments);
  if(spawn_status != 0)
    return linker_error(error, XS_BACKEND_SYSTEM_ERROR, strerror(spawn_status));

  int status = 0;
  if(waitpid(process, &status, 0) < 0)
    return linker_error(error, XS_BACKEND_SYSTEM_ERROR, strerror(errno));
  if(WIFEXITED(status))
    *exit_code = WEXITSTATUS(status);
  else if(WIFSIGNALED(status))
    *exit_code = 128 + WTERMSIG(status);
  else
    *exit_code = 1;
  return XS_BACKEND_OK;
}
