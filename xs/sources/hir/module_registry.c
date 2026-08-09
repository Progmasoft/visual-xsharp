/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "xs/hir/module_registry.h"

#include "module_internal.h"

#ifdef _WIN32
#    include <windows.h>
#else
#    include <dirent.h>
#endif
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

char *copy_text(const char *text)
{
    size_t length = strlen(text);
    char *copy = malloc(length + 1);
    if(copy != nullptr)
        memcpy(copy, text, length + 1);
    return copy;
}

static char *copy_span(const XsSource *source, XsSpan span)
{
    size_t length = span.end - span.start;
    char *copy = malloc(length + 1);
    if(copy != nullptr)
    {
        memcpy(copy, source->text + span.start, length);
        copy[length] = '\0';
    }
    return copy;
}

char *join_path(const char *left, const char *right)
{
    size_t left_length = strlen(left);
    size_t right_length = strlen(right);
    bool separator = left_length != 0 && left[left_length - 1] != '/';
    char *result = malloc(left_length + (separator ? 1 : 0) + right_length + 1);
    if(result == nullptr)
        return nullptr;
    memcpy(result, left, left_length);
    size_t offset = left_length;
    if(separator)
        result[offset++] = '/';
    memcpy(result + offset, right, right_length + 1);
    return result;
}

static bool has_suffix(const char *text, const char *suffix)
{
    size_t length = strlen(text);
    size_t suffix_length = strlen(suffix);
    return length >= suffix_length && strcmp(text + length - suffix_length, suffix) == 0;
}

bool append_issue(XsModuleIssues *issues, const char *path, size_t start, size_t end, const char *message)
{
    if(issues->count == issues->capacity)
    {
        size_t capacity = issues->capacity == 0 ? 8 : issues->capacity * 2;
        XsModuleIssue *items = realloc(issues->issues, capacity * sizeof(*items));
        if(items == nullptr)
        {
            issues->allocation_failed = true;
            return false;
        }
        issues->issues = items;
        issues->capacity = capacity;
    }
    XsModuleIssue issue = {
        .source_path = copy_text(path),
        .start = start,
        .end = end,
        .message = copy_text(message),
    };
    if(issue.source_path == nullptr || issue.message == nullptr)
    {
        free(issue.source_path);
        free(issue.message);
        issues->allocation_failed = true;
        return false;
    }
    issues->issues[issues->count++] = issue;
    return true;
}

char *read_file(const char *path, size_t *length)
{
    FILE *file = fopen(path, "rb");
    if(file == nullptr)
        return nullptr;
    if(fseek(file, 0, SEEK_END) != 0)
    {
        fclose(file);
        return nullptr;
    }
    long size = ftell(file);
    if(size < 0 || fseek(file, 0, SEEK_SET) != 0)
    {
        fclose(file);
        return nullptr;
    }
    char *text = malloc((size_t)size + 1);
    if(text == nullptr)
    {
        fclose(file);
        return nullptr;
    }
    size_t count = fread(text, 1, (size_t)size, file);
    fclose(file);
    if(count != (size_t)size)
    {
        free(text);
        return nullptr;
    }
    text[count] = '\0';
    *length = count;
    return text;
}

static XsToken scanner_token(ModuleScanner *scanner)
{
    XsToken token;
    do
    {
        token = xs_lexer_next(&scanner->lexer);
    } while(token.kind == XS_TOKEN_DOC_COMMENT || token.kind == XS_TOKEN_MODULE_COMMENT);
    return token;
}

void scanner_advance(ModuleScanner *scanner)
{
    scanner->current = scanner_token(scanner);
}

char *scan_path(ModuleScanner *scanner, size_t *start, size_t *end)
{
    if(scanner->current.kind != XS_TOKEN_IDENTIFIER)
        return nullptr;
    *start = scanner->current.span.start;
    *end = scanner->current.span.end;
    size_t length = *end - *start;
    char *result = copy_span(scanner->source, scanner->current.span);
    scanner_advance(scanner);
    while(scanner->current.kind == XS_TOKEN_DOT || scanner->current.kind == XS_TOKEN_DOUBLE_COLON)
    {
        scanner_advance(scanner);
        if(scanner->current.kind != XS_TOKEN_IDENTIFIER)
        {
            free(result);
            return nullptr;
        }
        size_t segment_length = scanner->current.span.end - scanner->current.span.start;
        char *grown = realloc(result, length + segment_length + 2);
        if(grown == nullptr)
        {
            free(result);
            return nullptr;
        }
        result = grown;
        result[length++] = '.';
        memcpy(result + length, scanner->source->text + scanner->current.span.start, segment_length);
        length += segment_length;
        result[length] = '\0';
        *end = scanner->current.span.end;
        scanner_advance(scanner);
    }
    return result;
}

static bool scan_module_declaration(const char *path, char **module_name, size_t *start, size_t *end,
                                    XsModuleIssues *issues)
{
    *module_name = nullptr;
    size_t length = 0;
    char *text = read_file(path, &length);
    if(text == nullptr)
    {
        append_issue(issues, path, 0, 0, "source file could not be read during module discovery");
        return false;
    }
    XsSource source = {.path = path, .text = text, .length = length};
    XsDiagnostics diagnostics;
    xs_diagnostics_init(&diagnostics);
    ModuleScanner scanner = {.source = &source};
    xs_lexer_init(&scanner.lexer, &source, &diagnostics);
    scanner_advance(&scanner);
    if(scanner.current.kind == XS_TOKEN_KW_MODULE)
    {
        scanner_advance(&scanner);
        *module_name = scan_path(&scanner, start, end);
        if(*module_name == nullptr || scanner.current.kind != XS_TOKEN_SEMICOLON)
        {
            append_issue(issues, path, *start, *end, "invalid module path or missing ';' in module declaration");
            free(*module_name);
            *module_name = nullptr;
        }
    }
    bool success = !issues->allocation_failed;
    xs_diagnostics_free(&diagnostics);
    free(text);
    return success;
}

static bool append_module(XsModuleRegistry *registry, XsDiscoveredModule module)
{
    if(registry->count == registry->capacity)
    {
        size_t capacity = registry->capacity == 0 ? 8 : registry->capacity * 2;
        XsDiscoveredModule *items = realloc(registry->modules, capacity * sizeof(*items));
        if(items == nullptr)
            return false;
        registry->modules = items;
        registry->capacity = capacity;
    }
    registry->modules[registry->count++] = module;
    return true;
}

const XsDiscoveredModule *xs_module_registry_find(const XsModuleRegistry *registry, const char *module_name)
{
    for(size_t i = 0; i < registry->count; ++i)
    {
        if(strcmp(registry->modules[i].module_name, module_name) == 0)
            return &registry->modules[i];
    }
    return nullptr;
}

static bool discover_path(const char *path, XsModuleRegistry *registry, XsModuleIssues *issues)
{
#ifdef _WIN32
    char *pattern = join_path(path, "*");
    if(pattern == nullptr)
    {
        issues->allocation_failed = true;
        return false;
    }
    WIN32_FIND_DATAA entry;
    HANDLE search = FindFirstFileA(pattern, &entry);
    free(pattern);
    if(search == INVALID_HANDLE_VALUE)
    {
        append_issue(issues, path, 0, 0, "directory could not be opened");
        return false;
    }
    bool success = true;
    do
    {
        if(strcmp(entry.cFileName, ".") == 0 || strcmp(entry.cFileName, "..") == 0)
            continue;
        char *child = join_path(path, entry.cFileName);
        if(child == nullptr)
        {
            issues->allocation_failed = true;
            success = false;
            break;
        }
        if((entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
        {
            success = discover_path(child, registry, issues) && success;
        }
        else if(has_suffix(child, ".vxs"))
        {
            char *module_name = nullptr;
            size_t start = 0;
            size_t end = 0;
            if(!scan_module_declaration(child, &module_name, &start, &end, issues))
                success = false;
            if(module_name != nullptr)
            {
                const XsDiscoveredModule *duplicate = xs_module_registry_find(registry, module_name);
                if(duplicate != nullptr)
                {
                    char message[512];
                    snprintf(message, sizeof(message), "duplicate module '%s'; first declared in %s", module_name,
                             duplicate->source_path);
                    append_issue(issues, child, start, end, message);
                    free(module_name);
                    success = false;
                }
                else
                {
                    XsDiscoveredModule module = {
                        .module_name = module_name,
                        .source_path = copy_text(child),
                        .declaration_start = start,
                        .declaration_end = end,
                    };
                    if(module.source_path == nullptr || !append_module(registry, module))
                    {
                        free(module.module_name);
                        free(module.source_path);
                        issues->allocation_failed = true;
                        success = false;
                    }
                }
            }
        }
        free(child);
    } while(FindNextFileA(search, &entry) != 0);
    (void)FindClose(search);
    return success;
#else
    DIR *directory = opendir(path);
    if(directory == nullptr)
    {
        append_issue(issues, path, 0, 0, strerror(errno));
        return false;
    }
    bool success = true;
    struct dirent *entry;
    while((entry = readdir(directory)) != nullptr)
    {
        if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        char *child = join_path(path, entry->d_name);
        if(child == nullptr)
        {
            issues->allocation_failed = true;
            success = false;
            break;
        }
        struct stat info;
        if(stat(child, &info) != 0)
        {
            append_issue(issues, child, 0, 0, strerror(errno));
            success = false;
        }
        else if(S_ISDIR(info.st_mode))
        {
            success = discover_path(child, registry, issues) && success;
        }
        else if(S_ISREG(info.st_mode) && has_suffix(child, ".vxs"))
        {
            char *module_name = nullptr;
            size_t start = 0;
            size_t end = 0;
            if(!scan_module_declaration(child, &module_name, &start, &end, issues))
                success = false;
            if(module_name != nullptr)
            {
                const XsDiscoveredModule *duplicate = xs_module_registry_find(registry, module_name);
                if(duplicate != nullptr)
                {
                    char message[512];
                    snprintf(message, sizeof(message), "duplicate module '%s'; first declared in %s", module_name,
                             duplicate->source_path);
                    append_issue(issues, child, start, end, message);
                    free(module_name);
                    success = false;
                }
                else
                {
                    XsDiscoveredModule module = {
                        .module_name = module_name,
                        .source_path = copy_text(child),
                        .declaration_start = start,
                        .declaration_end = end,
                    };
                    if(module.source_path == nullptr || !append_module(registry, module))
                    {
                        free(module.module_name);
                        free(module.source_path);
                        issues->allocation_failed = true;
                        success = false;
                    }
                }
            }
        }
        free(child);
    }
    closedir(directory);
    return success;
#endif
}

bool xs_module_registry_discover(const char *project_root, XsModuleRegistry *registry, XsModuleIssues *issues)
{
    if(project_root == nullptr || registry == nullptr || issues == nullptr)
        return false;
    bool success = discover_path(project_root, registry, issues);
    return success && issues->count == 0 && !issues->allocation_failed;
}
