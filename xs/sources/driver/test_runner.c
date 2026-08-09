/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "test_runner.h"

#include "compiler_core_native.h"
#include "native_artifact.h"

#include "xs/diagnostic.h"
#include "xs/source.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#    include <direct.h>
#    include <windows.h>
#else
#    include <unistd.h>
#endif

static char *test_name(const XsCompilerCoreSession *session, uint64_t index)
{
    uint64_t raw_length = 0;
    const uint8_t *raw = xslang_compiler_core_session_test_name(session, index, &raw_length);
    if(raw == nullptr || raw_length > SIZE_MAX - 1U)
        return nullptr;
    size_t length = (size_t)raw_length;
    char *name = malloc(length + 1U);
    if(name == nullptr)
        return nullptr;
    memcpy(name, raw, length);
    name[length] = '\0';
    return name;
}

static void remove_artifact(const char *input_path, const char *extension)
{
    char *path = xs_driver_native_artifact_path(input_path, extension);
    if(path != nullptr)
    {
        (void)remove(path);
        free(path);
    }
}

static void remove_test_artifacts(const char *input_path)
{
    remove_artifact(input_path, ".ll");
    remove_artifact(input_path, ".obj");
    remove_artifact(input_path, ".vxse");
}

static bool build_harness(const char *input_path, const char *name, const XsCompilerCoreSession *harness)
{
    XsDiagnostics diagnostics;
    xs_diagnostics_init(&diagnostics);
    XsSpan span = {0};
    bool success = xs_driver_compiler_core_native_available(harness);
    if(success)
        success = xs_driver_build_compiler_core_native(input_path, harness, &diagnostics, span);
    else if(!xs_driver_append_compiler_core_diagnostics(harness, &diagnostics, span))
        (void)xs_diagnostics_add(&diagnostics, XS_DIAGNOSTIC_ERROR, span, "test harness could not reach verified XLIL");
    if(!success)
    {
        XsSource source = {.path = name, .text = "", .length = 0};
        xs_diagnostics_print(&diagnostics, &source, stderr);
    }
    xs_diagnostics_free(&diagnostics);
    return success;
}

static bool run_one_test(const XsCompilerCoreSession *session, uint64_t index, const char *input_path, const char *name,
                         uint32_t flags)
{
    XsCompilerCoreSession *harness = nullptr;
    if(xslang_compiler_core_test_harness_create(session, index, &harness) != XS_COMPILER_CORE_FFI_OK ||
       harness == nullptr)
    {
        fprintf(stderr, "vxs: test %s ... FAILED (harness creation)\n", name);
        return false;
    }
    bool built = build_harness(input_path, name, harness);
    xslang_compiler_core_session_free(harness);
    if(!built)
    {
        fprintf(stderr, "vxs: test %s ... FAILED (compilation)\n", name);
        remove_test_artifacts(input_path);
        return false;
    }
    int exit_code = 1;
    bool executed = xs_driver_execute_native_artifact(input_path, &exit_code);
    bool should_panic = (flags & XS_COMPILER_CORE_TEST_SHOULD_PANIC) != 0U;
    bool passed = executed && (should_panic ? exit_code != 0 : exit_code == 0);
    fprintf(stderr, "vxs: test %s ... %s\n", name, passed ? "ok" : "FAILED");
    remove_test_artifacts(input_path);
    return passed;
}

int xs_driver_run_compiler_core_tests(const XsCompilerCoreSession *session)
{
    uint64_t count = xslang_compiler_core_session_test_count(session);
#ifdef _WIN32
    char directory[MAX_PATH] = {0};
    char temporary[MAX_PATH] = {0};
    bool directory_created = count == 0U;
    if(count != 0U && GetTempPathA(MAX_PATH, directory) != 0 &&
       GetTempFileNameA(directory, "xst", 0, temporary) != 0)
    {
        (void)DeleteFileA(temporary);
        directory_created = CreateDirectoryA(temporary, nullptr) != 0;
        if(directory_created)
            (void)snprintf(directory, sizeof(directory), "%s", temporary);
    }
#else
    char directory[] = "/tmp/xs-tests-XXXXXX";
    bool directory_created = count == 0U || mkdtemp(directory) != nullptr;
#endif
    if(!directory_created)
    {
        fprintf(stderr, "vxs: could not create the native test artifact directory\n");
        return 1;
    }
    size_t passed = 0;
    size_t failed = 0;
    size_t ignored = 0;
    for(uint64_t index = 0; index < count; ++index)
    {
        char *name = test_name(session, index);
        if(name == nullptr)
        {
            ++failed;
            continue;
        }
        uint32_t flags = xslang_compiler_core_session_test_flags(session, index);
        if((flags & XS_COMPILER_CORE_TEST_IGNORED) != 0U)
        {
            fprintf(stderr, "vxs: test %s ... ignored\n", name);
            ++ignored;
            free(name);
            continue;
        }
        char input_path[sizeof(directory) + 32U];
#ifdef _WIN32
        int written = snprintf(input_path, sizeof(input_path), "%s\\case-%llu.xs", directory, (unsigned long long)index);
#else
        int written = snprintf(input_path, sizeof(input_path), "%s/case-%llu.xs", directory, (unsigned long long)index);
#endif
        bool success = written > 0 && (size_t)written < sizeof(input_path) &&
                       run_one_test(session, index, input_path, name, flags);
        passed += success ? 1U : 0U;
        failed += success ? 0U : 1U;
        free(name);
    }
    if(count != 0U)
#ifdef _WIN32
        (void)_rmdir(directory);
#else
        (void)rmdir(directory);
#endif
    fprintf(stderr, "vxs: test result: %s. %zu passed; %zu failed; %zu ignored\n", failed == 0U ? "ok" : "FAILED",
            passed, failed, ignored);
    return failed == 0U ? 0 : 1;
}
