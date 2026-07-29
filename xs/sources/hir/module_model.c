/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "xs/hir/module_registry.h"

#include <stdio.h>
#include <stdlib.h>

void xs_module_registry_init(XsModuleRegistry *registry)
{
  *registry = (XsModuleRegistry){0};
}

void xs_module_registry_free(XsModuleRegistry *registry)
{
  for(size_t i = 0; i < registry->count; ++i)
  {
    free(registry->modules[i].module_name);
    free(registry->modules[i].source_path);
  }
  free(registry->modules);
  *registry = (XsModuleRegistry){0};
}

void xs_module_graph_init(XsModuleGraph *graph)
{
  *graph = (XsModuleGraph){0};
}

void xs_module_graph_free(XsModuleGraph *graph)
{
  for(size_t i = 0; i < graph->count; ++i)
  {
    free(graph->dependencies[i].importer_path);
    free(graph->dependencies[i].module_name);
    free(graph->dependencies[i].imported_path);
  }
  free(graph->dependencies);
  *graph = (XsModuleGraph){0};
}

void xs_module_issues_init(XsModuleIssues *issues)
{
  *issues = (XsModuleIssues){0};
}

void xs_module_issues_free(XsModuleIssues *issues)
{
  for(size_t i = 0; i < issues->count; ++i)
  {
    free(issues->issues[i].source_path);
    free(issues->issues[i].message);
  }
  free(issues->issues);
  *issues = (XsModuleIssues){0};
}

void xs_module_issues_print(const XsModuleIssues *issues)
{
  for(size_t i = 0; i < issues->count; ++i)
    fprintf(stderr, "%s:%zu: error: %s\n", issues->issues[i].source_path, issues->issues[i].start,
            issues->issues[i].message);
  if(issues->allocation_failed)
    fprintf(stderr, "xs: error: out of memory while resolving modules\n");
}
