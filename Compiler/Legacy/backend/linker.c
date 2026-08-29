/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

#include "Visual/XSharp/backend/linker.h"

#include <errno.h>
#ifdef _WIN32
#    include <process.h>
#else
#    include <spawn.h>
#    include <sys/wait.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
extern char **environ;
#endif

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
    if(invocation == nullptr || invocation->program == nullptr || invocation->program[0] == '\0' ||
       exit_code == nullptr || (invocation->argument_count != 0 && invocation->arguments == nullptr))
        return linker_error(error, XS_BACKEND_INVALID_ARGUMENT,
                            "valid linker invocation and exit-code output are required");

    char **arguments = calloc(invocation->argument_count + 2, sizeof(*arguments));
    if(arguments == nullptr)
        return linker_error(error, XS_BACKEND_SYSTEM_ERROR, "out of memory while preparing linker invocation");
    arguments[0] = (char *)invocation->program;
    for(size_t i = 0; i < invocation->argument_count; ++i)
        arguments[i + 1] = (char *)invocation->arguments[i];

#ifdef _WIN32
    intptr_t result = _spawnvp(_P_WAIT, invocation->program, (const char *const *)arguments);
    free(arguments);
    if(result == -1)
        return linker_error(error, XS_BACKEND_SYSTEM_ERROR, strerror(errno));
    *exit_code = (int)result;
#else
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
#endif
    return XS_BACKEND_OK;
}
