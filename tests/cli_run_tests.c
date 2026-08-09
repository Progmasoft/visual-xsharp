/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#ifdef _WIN32
#    include <process.h>
#else
#    include <spawn.h>
#    include <sys/wait.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#ifndef _WIN32
extern char **environ;
#endif

int main(int argc, char **argv)
{
    if(argc != 4)
    {
        fprintf(stderr, "CLI run test requires xs, source, and expected exit code\n");
        return 2;
    }
    char *end = nullptr;
    long expected = strtol(argv[3], &end, 10);
    if(end == argv[3] || *end != '\0' || expected < 0 || expected > 255)
    {
        fprintf(stderr, "invalid expected exit code '%s'\n", argv[3]);
        return 2;
    }
    char *const arguments[] = {argv[1], "run", "-file", argv[2], nullptr};
#ifdef _WIN32
    intptr_t result = _spawnv(_P_WAIT, argv[1], (const char *const *)arguments);
    if(result == -1)
    {
        perror("could not start vxs");
        return 1;
    }
    int actual = (int)result;
#else
    pid_t process = 0;
    int spawn_status = posix_spawn(&process, argv[1], nullptr, nullptr, arguments, environ);
    if(spawn_status != 0)
    {
        fprintf(stderr, "could not start vxs: %d\n", spawn_status);
        return 1;
    }
    int status = 0;
    if(waitpid(process, &status, 0) < 0)
    {
        perror("could not wait for xs");
        return 1;
    }
    if(!WIFEXITED(status))
    {
        fprintf(stderr, "xs did not exit normally\n");
        return 1;
    }
    int actual = WEXITSTATUS(status);
#endif
    if(actual != (int)expected)
    {
        fprintf(stderr, "xs run returned %d, expected %ld\n", actual, expected);
        return 1;
    }
    return 0;
}
