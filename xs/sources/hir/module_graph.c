/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "module_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool append_import(ImportList *import, char *name, size_t start, size_t end)
{
    if(import->count == import->capacity)
    {
        size_t capacity = import->capacity == 0 ? 4 : import->capacity * 2;
        ImportView *items = realloc(import->items, capacity * sizeof(*items));
        if(items == nullptr)
        {
            free(name);
            return false;
        }
        import->items = items;
        import->capacity = capacity;
    }
    import->items[import->count++] = (ImportView){.name = name, .start = start, .end = end};
    return true;
}

static void free_imports(ImportList *import)
{
    for(size_t i = 0; i < import->count; ++i)
        free(import->items[i].name);
    free(import->items);
    *import = (ImportList){0};
}

static void trim_last_path_segment(char *path)
{
    char *dot = strrchr(path, '.');
    if(dot != nullptr)
        *dot = '\0';
}

static bool scan_imports(const char *path, ImportList *import, XsModuleIssues *issues)
{
    size_t length = 0;
    char *text = read_file(path, &length);
    if(text == nullptr)
    {
        append_issue(issues, path, 0, 0, "source file could not be read during import resolution");
        return false;
    }
    XsSource source = {.path = path, .text = text, .length = length};
    XsDiagnostics diagnostics;
    xs_diagnostics_init(&diagnostics);
    ModuleScanner scanner = {.source = &source};
    xs_lexer_init(&scanner.lexer, &source, &diagnostics);
    scanner_advance(&scanner);
    size_t brace_depth = 0;
    bool success = true;
    while(scanner.current.kind != XS_TOKEN_EOF)
    {
        if(scanner.current.kind == XS_TOKEN_LEFT_BRACE)
        {
            ++brace_depth;
            scanner_advance(&scanner);
            continue;
        }
        if(scanner.current.kind == XS_TOKEN_RIGHT_BRACE)
        {
            if(brace_depth != 0)
                --brace_depth;
            scanner_advance(&scanner);
            continue;
        }
        if(brace_depth == 0 && scanner.current.kind == XS_TOKEN_KW_IMPORT)
        {
            scanner_advance(&scanner);
            do
            {
                size_t start = 0;
                size_t end = 0;
                char *name = scan_path(&scanner, &start, &end);
                if(name == nullptr || !append_import(import, name, start, end))
                {
                    success = false;
                    break;
                }
                if(scanner.current.kind == XS_TOKEN_KW_AS)
                {
                    scanner_advance(&scanner);
                    if(scanner.current.kind == XS_TOKEN_IDENTIFIER)
                        scanner_advance(&scanner);
                }
                if(scanner.current.kind != XS_TOKEN_COMMA)
                    break;
                scanner_advance(&scanner);
            } while(true);
            continue;
        }
        if(brace_depth == 0 && scanner.current.kind == XS_TOKEN_KW_USING)
        {
            scanner_advance(&scanner);
            bool namespace_using = false;
            if(scanner.current.kind == XS_TOKEN_KW_NAMESPACE)
            {
                namespace_using = true;
                scanner_advance(&scanner);
            }
            size_t start = 0;
            size_t end = 0;
            char *name = scan_path(&scanner, &start, &end);
            if(name != nullptr && scanner.current.kind == XS_TOKEN_ASSIGN)
            {
                free(name);
                scanner_advance(&scanner);
                name = scan_path(&scanner, &start, &end);
            }
            if(name != nullptr && !namespace_using)
                trim_last_path_segment(name);
            if(name == nullptr || !append_import(import, name, start, end))
                success = false;
            continue;
        }
        scanner_advance(&scanner);
    }
    xs_diagnostics_free(&diagnostics);
    free(text);
    return success;
}

static bool string_list_contains(const StringList *list, const char *text)
{
    for(size_t i = 0; i < list->count; ++i)
    {
        if(strcmp(list->items[i], text) == 0)
            return true;
    }
    return false;
}

static bool string_list_append(StringList *list, char *text)
{
    if(list->count == list->capacity)
    {
        size_t capacity = list->capacity == 0 ? 8 : list->capacity * 2;
        char **items = realloc(list->items, capacity * sizeof(*items));
        if(items == nullptr)
            return false;
        list->items = items;
        list->capacity = capacity;
    }
    list->items[list->count++] = text;
    return true;
}

static bool append_dependency(XsModuleGraph *graph, const char *importer, const ImportView *import,
                              const char *imported_path)
{
    if(graph->count == graph->capacity)
    {
        size_t capacity = graph->capacity == 0 ? 8 : graph->capacity * 2;
        XsModuleDependency *items = realloc(graph->dependencies, capacity * sizeof(*items));
        if(items == nullptr)
            return false;
        graph->dependencies = items;
        graph->capacity = capacity;
    }
    XsModuleDependency dependency = {
        .importer_path = copy_text(importer),
        .module_name = copy_text(import->name),
        .imported_path = copy_text(imported_path),
        .import_start = import->start,
        .import_end = import->end,
    };
    if(dependency.importer_path == nullptr || dependency.module_name == nullptr || dependency.imported_path == nullptr)
    {
        free(dependency.importer_path);
        free(dependency.module_name);
        free(dependency.imported_path);
        return false;
    }
    graph->dependencies[graph->count++] = dependency;
    return true;
}

bool xs_module_graph_resolve(const char *project_root, const char *const *direct_sources, size_t direct_source_count,
                             const XsModuleRegistry *registry, XsModuleGraph *graph, XsModuleIssues *issues)
{
    if(project_root == nullptr || registry == nullptr || graph == nullptr || issues == nullptr ||
       (direct_source_count != 0 && direct_sources == nullptr))
        return false;
    StringList queue = {0};
    StringList visited = {0};
    bool success = true;
    for(size_t i = 0; i < direct_source_count; ++i)
    {
        char *path =
            direct_sources[i][0] == '/' ? copy_text(direct_sources[i]) : join_path(project_root, direct_sources[i]);
        if(path == nullptr || !string_list_append(&queue, path))
        {
            free(path);
            issues->allocation_failed = true;
            success = false;
            break;
        }
    }
    for(size_t index = 0; index < queue.count; ++index)
    {
        char *path = queue.items[index];
        if(string_list_contains(&visited, path))
            continue;
        char *visited_path = copy_text(path);
        if(visited_path == nullptr || !string_list_append(&visited, visited_path))
        {
            free(visited_path);
            issues->allocation_failed = true;
            success = false;
            break;
        }
        ImportList import = {0};
        if(!scan_imports(path, &import, issues))
            success = false;
        for(size_t i = 0; i < import.count; ++i)
        {
            const XsDiscoveredModule *module = xs_module_registry_find(registry, import.items[i].name);
            if(module == nullptr)
            {
                char message[512];
                snprintf(message, sizeof(message), "imported module '%s' was not found", import.items[i].name);
                append_issue(issues, path, import.items[i].start, import.items[i].end, message);
                success = false;
                continue;
            }
            if(!append_dependency(graph, path, &import.items[i], module->source_path))
            {
                issues->allocation_failed = true;
                success = false;
                continue;
            }
            if(!string_list_contains(&visited, module->source_path))
            {
                char *imported_path = copy_text(module->source_path);
                if(imported_path == nullptr || !string_list_append(&queue, imported_path))
                {
                    free(imported_path);
                    issues->allocation_failed = true;
                    success = false;
                }
            }
        }
        free_imports(&import);
    }
    for(size_t i = 0; i < queue.count; ++i)
        free(queue.items[i]);
    for(size_t i = 0; i < visited.count; ++i)
        free(visited.items[i]);
    free(queue.items);
    free(visited.items);
    return success && issues->count == 0 && !issues->allocation_failed;
}
