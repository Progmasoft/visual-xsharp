/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>

extern char **environ;

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
    pid_t process = 0;
    int spawn_status = posix_spawn(&process, argv[1], nullptr, nullptr, arguments, environ);
    if(spawn_status != 0)
    {
        fprintf(stderr, "could not start xs: %d\n", spawn_status);
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
    if(actual != (int)expected)
    {
        fprintf(stderr, "xs run returned %d, expected %ld\n", actual, expected);
        return 1;
    }
    return 0;
}
