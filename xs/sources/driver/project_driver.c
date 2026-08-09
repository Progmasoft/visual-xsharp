/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "project_driver.h"

#include <errno.h>
#include <limits.h>
#ifdef _WIN32
#    include <io.h>
#    include <process.h>
#    include <windows.h>
#    define access _access
#    define X_OK 0
#else
#    include <spawn.h>
#    include <sys/wait.h>
#    include <unistd.h>
#endif
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
extern char **environ;
#endif

static const char *const REGISTRY_VERSION = "xs-project-sources-v5";

#ifndef XS_PROJECT_RUNTIME_DEFAULT
#    define XS_PROJECT_RUNTIME_DEFAULT "xs-project-runtime"
#endif

static const char *installed_project_runtime(void)
{
#ifdef _WIN32
    static char path[MAX_PATH];
    DWORD length = GetModuleFileNameA(nullptr, path, MAX_PATH);
    if(length == 0 || length >= MAX_PATH)
        return nullptr;
    char *separator = strrchr(path, '\\');
    if(separator == nullptr)
        return nullptr;
    *separator = '\0';
    const char *suffix = "\\..\\libexec\\xs\\project-runtime\\bin\\xs-project-runtime.bat";
    size_t directory_length = strlen(path);
    size_t suffix_length = strlen(suffix);
    if(directory_length > sizeof(path) - suffix_length - 1U)
        return nullptr;
    memcpy(path + directory_length, suffix, suffix_length + 1U);
    return access(path, 0) == 0 ? path : nullptr;
#else
    static char path[PATH_MAX];
    ssize_t length = readlink("/proc/self/exe", path, sizeof(path) - 1U);
    if(length <= 0 || (size_t)length >= sizeof(path))
        return nullptr;
    path[length] = '\0';
    char *separator = strrchr(path, '/');
    if(separator == nullptr)
        return nullptr;
    *separator = '\0';
    const char *suffix = "/../libexec/xs/project-runtime/bin/xs-project-runtime";
    size_t directory_length = strlen(path);
    size_t suffix_length = strlen(suffix);
    if(directory_length > sizeof(path) - suffix_length - 1U)
        return nullptr;
    memcpy(path + directory_length, suffix, suffix_length + 1U);
    return access(path, X_OK) == 0 ? path : nullptr;
#endif
}

static int run_program(const char *program, const char *const *arguments)
{
#ifdef _WIN32
    intptr_t result = _spawnvp(_P_WAIT, program, arguments);
    return result == -1 ? -1 : (int)result;
#else
    pid_t process = 0;
    int status = posix_spawnp(&process, program, nullptr, nullptr, (char *const *)arguments, environ);
    if(status != 0)
        return -1;
    int result = 0;
    if(waitpid(process, &result, 0) < 0 || !WIFEXITED(result))
        return -1;
    return WEXITSTATUS(result);
#endif
}

static const char *project_runtime_program(void)
{
    const char *program = getenv("XS_PROJECT_RUNTIME");
    if(program != nullptr && program[0] != '\0')
        return program;
#ifdef XS_PROJECT_RUNTIME_BUILD
    if(access(XS_PROJECT_RUNTIME_BUILD, X_OK) == 0)
        return XS_PROJECT_RUNTIME_BUILD;
#endif
    program = installed_project_runtime();
    if(program != nullptr)
        return program;
    return XS_PROJECT_RUNTIME_DEFAULT;
}

static bool run_resolver(const char *mode, const char *project_root, const char *output_path, const char *module_path)
{
    const char *program = project_runtime_program();
    const char *arguments[] = {program, mode, project_root, output_path, module_path, nullptr};
    int result = run_program(program, arguments);
    if(result < 0)
    {
        fprintf(stderr, "vxs: could not start the bundled project runtime '%s': %s\n", program, strerror(errno));
        return false;
    }
    return result == 0;
}

bool xs_driver_refresh_lock(void)
{
    const char *program = project_runtime_program();
    const char *arguments[] = {program, "resolve", ".", nullptr};
    int result = run_program(program, arguments);
    if(result < 0)
    {
        fprintf(stderr, "vxs: could not start the bundled project runtime '%s': %s\n", program, strerror(errno));
        return false;
    }
    if(result != 0)
        return false;
    fprintf(stderr, "vxs: refreshed binary lock file 'xs.lock.sqlite3'\n");
    return true;
}

static char *read_registry(const char *path, size_t *length)
{
    FILE *file = fopen(path, "rb");
    if(file == nullptr || fseek(file, 0, SEEK_END) != 0)
        goto failure;
    long size = ftell(file);
    if(size <= 0 || fseek(file, 0, SEEK_SET) != 0)
        goto failure;
    char *data = malloc((size_t)size);
    if(data == nullptr || fread(data, 1, (size_t)size, file) != (size_t)size)
    {
        free(data);
        goto failure;
    }
    fclose(file);
    *length = (size_t)size;
    return data;

failure:
    if(file != nullptr)
        fclose(file);
    return nullptr;
}

void xs_driver_free_project(XsResolvedProject *project)
{
    if(project == nullptr)
        return;
    for(size_t i = 0; i < project->path_count; ++i)
    {
        free(project->paths[i]);
        free(project->module_names[i]);
    }
    free(project->paths);
    free(project->module_names);
    for(size_t i = 0; i < project->test_path_count; ++i)
        free(project->test_paths[i]);
    free(project->test_paths);
    *project = (XsResolvedProject){0};
}

static bool parse_bool_record(const char *text, bool *value)
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

static bool parse_size_record(const char *text, size_t *value)
{
    char *end = nullptr;
    unsigned long long parsed = strtoull(text, &end, 10);
    if(text[0] == '\0' || end == nullptr || *end != '\0' || parsed > SIZE_MAX)
        return false;
    *value = (size_t)parsed;
    return true;
}

static char *copy_record(const char *text)
{
    size_t length = strlen(text);
    char *result = malloc(length + 1U);
    if(result != nullptr)
        memcpy(result, text, length + 1U);
    return result;
}

static char *next_record(char *record)
{
    return record + strlen(record) + 1U;
}

static bool parse_header(char *data, size_t record_count, size_t *source_count, size_t *module_count,
                         size_t *test_count, XsCompilerSettings *settings)
{
    *settings = xs_cli_default_compiler_settings();
    if(strcmp(data, REGISTRY_VERSION) != 0 || record_count < 7U)
        return false;
    char *warning = next_record(data);
    char *werror = next_record(warning);
    char *verbose = next_record(werror);
    char *sources = next_record(verbose);
    char *modules = next_record(sources);
    char *tests = next_record(modules);
    bool parsed_warning = false;
    for(XsWarningLevel level = XS_WARNING_NONE; level <= XS_WARNING_ALL; ++level)
    {
        if(strcmp(warning, xs_cli_warning_level_name(level)) == 0)
        {
            settings->warning_level = level;
            parsed_warning = true;
            break;
        }
    }
    if(!parsed_warning || !parse_bool_record(werror, &settings->warnings_as_errors) ||
       !parse_bool_record(verbose, &settings->verbose) || !parse_size_record(sources, source_count) ||
       !parse_size_record(modules, module_count) || !parse_size_record(tests, test_count))
        return false;
    if(*source_count > SIZE_MAX - 7U || *test_count > SIZE_MAX - 7U - *source_count ||
       *module_count > (SIZE_MAX - 7U - *source_count - *test_count) / 2U)
        return false;
    return record_count == 7U + *source_count + (*module_count * 2U) + *test_count;
}

static bool resolve_project_registry(const char *mode, const char *project_root, const char *module_path,
                                     bool require_sources, XsResolvedProject *project)
{
    if(project == nullptr)
        return false;
    *project = (XsResolvedProject){0};
#ifdef _WIN32
    char temporary_directory[MAX_PATH] = {0};
    char registry_path[MAX_PATH] = {0};
    bool registry_created = GetTempPathA(MAX_PATH, temporary_directory) != 0 &&
                            GetTempFileNameA(temporary_directory, "xsr", 0, registry_path) != 0;
#else
    char registry_path[] = "/tmp/xs-project-sources-XXXXXX";
    int registry = mkstemp(registry_path);
    bool registry_created = registry >= 0;
#endif
    if(!registry_created)
    {
        fprintf(stderr, "vxs: could not create the project source registry\n");
        return false;
    }
#ifndef _WIN32
    close(registry);
#endif
    bool success = run_resolver(mode, project_root, registry_path, module_path);
    size_t length = 0;
    char *data = success ? read_registry(registry_path, &length) : nullptr;
    (void)remove(registry_path);
    if(data == nullptr || data[length - 1U] != '\0')
    {
        free(data);
        if(success)
            fprintf(stderr, "vxs: bundled project runtime produced an invalid source registry\n");
        return false;
    }
    size_t count = 0;
    for(size_t i = 0; i < length; ++i)
        count += data[i] == '\0';
    size_t source_count = 0;
    size_t module_count = 0;
    size_t test_count = 0;
    if(count == 0U || !parse_header(data, count, &source_count, &module_count, &test_count, &project->settings) ||
       (require_sources && source_count == 0U))
    {
        free(data);
        fprintf(stderr, "vxs: bundled project runtime returned an invalid compiler/source registry\n");
        return false;
    }
    size_t path_count = source_count + module_count;
    project->paths = calloc(path_count, sizeof(*project->paths));
    project->module_names = calloc(path_count, sizeof(*project->module_names));
    project->test_paths = test_count == 0U ? nullptr : calloc(test_count, sizeof(*project->test_paths));
    project->test_path_count = test_count;
    if((path_count != 0U && (project->paths == nullptr || project->module_names == nullptr)) ||
       (test_count != 0U && project->test_paths == nullptr))
    {
        free(data);
        xs_driver_free_project(project);
        return false;
    }
    project->path_count = path_count;
    char *record = data;
    for(size_t i = 0; i < 7U; ++i)
        record = next_record(record);
    for(size_t i = 0; i < source_count; ++i)
    {
        project->paths[i] = copy_record(record);
        if(project->paths[i] == nullptr)
        {
            free(data);
            xs_driver_free_project(project);
            return false;
        }
        record = next_record(record);
    }
    for(size_t i = 0; i < module_count; ++i)
    {
        size_t index = source_count + i;
        project->module_names[index] = copy_record(record);
        record = next_record(record);
        project->paths[index] = copy_record(record);
        record = next_record(record);
        if(project->module_names[index] == nullptr || project->paths[index] == nullptr)
        {
            free(data);
            xs_driver_free_project(project);
            return false;
        }
    }
    for(size_t i = 0; i < test_count; ++i)
    {
        project->test_paths[i] = copy_record(record);
        record = next_record(record);
        if(project->test_paths[i] == nullptr)
        {
            free(data);
            xs_driver_free_project(project);
            return false;
        }
    }
    free(data);
    return true;
}

bool xs_driver_resolve_project(const char *module_path, XsResolvedProject *project)
{
    return resolve_project_registry("sources0", ".", module_path, true, project);
}

bool xs_driver_resolve_project_tests(const char *module_path, XsResolvedProject *project)
{
    return resolve_project_registry("sources0", ".", module_path, false, project);
}
