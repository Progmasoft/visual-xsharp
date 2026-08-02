/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef XS_HIR_MODULE_REGISTRY_H
#define XS_HIR_MODULE_REGISTRY_H

#include <stddef.h>

typedef struct
{
    char *module_name;
    char *source_path;
    size_t declaration_start;
    size_t declaration_end;
} XsDiscoveredModule;

typedef struct
{
    XsDiscoveredModule *modules;
    size_t count;
    size_t capacity;
} XsModuleRegistry;

typedef struct
{
    char *importer_path;
    char *module_name;
    char *imported_path;
    size_t import_start;
    size_t import_end;
} XsModuleDependency;

typedef struct
{
    XsModuleDependency *dependencies;
    size_t count;
    size_t capacity;
} XsModuleGraph;

typedef struct
{
    char *source_path;
    size_t start;
    size_t end;
    char *message;
} XsModuleIssue;

typedef struct
{
    XsModuleIssue *issues;
    size_t count;
    size_t capacity;
    bool allocation_failed;
} XsModuleIssues;

void xs_module_registry_init(XsModuleRegistry *registry);
void xs_module_registry_free(XsModuleRegistry *registry);
void xs_module_graph_init(XsModuleGraph *graph);
void xs_module_graph_free(XsModuleGraph *graph);
void xs_module_issues_init(XsModuleIssues *issues);
void xs_module_issues_free(XsModuleIssues *issues);
void xs_module_issues_print(const XsModuleIssues *issues);

bool xs_module_registry_discover(const char *project_root, XsModuleRegistry *registry, XsModuleIssues *issues);
const XsDiscoveredModule *xs_module_registry_find(const XsModuleRegistry *registry, const char *module_name);
bool xs_module_graph_resolve(const char *project_root, const char *const *direct_sources, size_t direct_source_count,
                             const XsModuleRegistry *registry, XsModuleGraph *graph, XsModuleIssues *issues);

#endif
