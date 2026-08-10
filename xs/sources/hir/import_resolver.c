/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "Visual/XSharp/hir/symbol_table.h"

#include "standard_library.h"
#include "syntax_helpers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool append_import_binding(XsHirImportScope *scope, XsHirImportBinding binding)
{
    if(scope->count == scope->capacity)
    {
        size_t capacity = scope->capacity == 0 ? 16 : scope->capacity * 2;
        XsHirImportBinding *bindings = realloc(scope->bindings, capacity * sizeof(*bindings));
        if(bindings == nullptr)
        {
            scope->allocation_failed = true;
            return false;
        }
        scope->bindings = bindings;
        scope->capacity = capacity;
    }
    scope->bindings[scope->count++] = binding;
    return true;
}

static bool append_module_import(XsHirImportScope *scope, char *module_name)
{
    if(scope->module_count == scope->module_capacity)
    {
        size_t capacity = scope->module_capacity == 0 ? 8 : scope->module_capacity * 2;
        char **module_names = realloc(scope->module_names, capacity * sizeof(*module_names));
        if(module_names == nullptr)
        {
            scope->allocation_failed = true;
            return false;
        }
        scope->module_names = module_names;
        scope->module_capacity = capacity;
    }
    scope->module_names[scope->module_count++] = module_name;
    return true;
}

static bool import_module_name(XsHirImportScope *scope, const char *module_name)
{
    if(xs_hir_import_scope_has_module(scope, module_name))
        return true;
    char *copy = xs_hir_copy_cstr(module_name);
    if(copy == nullptr)
    {
        scope->allocation_failed = true;
        return false;
    }
    if(!append_module_import(scope, copy))
    {
        free(copy);
        return false;
    }
    return true;
}

static bool report_import_conflict(XsDiagnostics *diagnostics, XsSpan span, const char *local_name)
{
    char message[512];
    snprintf(message, sizeof(message), "imported name '%s' conflicts with a previous import", local_name);
    return xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, span, message);
}

static bool import_symbol_as(XsHirImportScope *scope, const XsHirSymbol *symbol, const char *local_name,
                             XsSourceSpan source_span, XsDiagnostics *diagnostics)
{
    char *copy = xs_hir_copy_cstr(local_name);
    if(copy == nullptr)
    {
        scope->allocation_failed = true;
        return false;
    }
    if(xs_hir_import_scope_find(scope, local_name) != nullptr)
    {
        XsSpan span = {.start = source_span.start_offset, .end = source_span.end_offset};
        report_import_conflict(diagnostics, span, local_name);
        free(copy);
        return true;
    }
    XsHirImportBinding binding = {
        .local_name = copy,
        .symbol = symbol,
        .span = source_span,
    };
    if(!append_import_binding(scope, binding))
    {
        free(copy);
        return false;
    }
    return true;
}

static bool import_module_namespace(const XsSyntaxNode *import_node, const XsSyntaxNode *path,
                                    const XsHirSymbolTable *project_symbols, XsHirImportScope *scope,
                                    XsDiagnostics *diagnostics)
{
    char *module_name = xs_hir_path_to_string(path);
    if(module_name == nullptr)
    {
        scope->allocation_failed = true;
        return false;
    }
    bool exists = xs_hir_standard_module_name(module_name);
    for(size_t i = 0; !exists && i < project_symbols->count; ++i)
    {
        const XsHirSymbol *symbol = &project_symbols->symbols[i];
        exists = strcmp(symbol->module_name, module_name) == 0 || strcmp(symbol->namespace_name, module_name) == 0;
    }
    if(!exists)
    {
        char message[512];
        snprintf(message, sizeof(message), "module '%s' does not exist with this exact case", module_name);
        xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, xs_hir_node_span(import_node), message);
        free(module_name);
        return true;
    }
    if(xs_hir_import_scope_has_module(scope, module_name))
    {
        free(module_name);
        return true;
    }
    return append_module_import(scope, module_name);
}

static const XsHirSymbol *find_symbol_in_namespace(const XsHirSymbolTable *table, const char *namespace_name,
                                                   const char *name)
{
    for(size_t i = 0; i < table->count; ++i)
    {
        if(xs_hir_symbol_is_public_child_of_namespace(&table->symbols[i], namespace_name) &&
           strcmp(table->symbols[i].name, name) == 0)
            return &table->symbols[i];
    }
    return nullptr;
}

static bool split_qualified_name(char *qualified, const char **namespace_name, const char **name)
{
    char *dot = strrchr(qualified, '.');
    if(dot == nullptr || dot == qualified || dot[1] == '\0')
        return false;
    *dot = '\0';
    *namespace_name = qualified;
    *name = dot + 1;
    return true;
}

static bool import_selected_name(const XsSyntaxNode *name_node, const char *module_name,
                                 const XsHirSymbolTable *project_symbols, XsHirImportScope *scope,
                                 XsDiagnostics *diagnostics)
{
    const XsSyntaxNode *imported = xs_hir_first_child_kind(name_node, XS_SYNTAX_IDENTIFIER);
    if(imported == nullptr)
        return true;
    char *imported_name = xs_hir_copy_text(imported->text);
    const XsSyntaxNode *alias = xs_hir_second_child_kind(name_node, XS_SYNTAX_IDENTIFIER);
    char *local_name = alias == nullptr ? xs_hir_copy_text(imported->text) : xs_hir_copy_text(alias->text);
    if(imported_name == nullptr || local_name == nullptr)
    {
        free(imported_name);
        free(local_name);
        scope->allocation_failed = true;
        return false;
    }
    const XsHirSymbol *symbol = find_symbol_in_namespace(project_symbols, module_name, imported_name);
    if(symbol == nullptr)
    {
        xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, xs_hir_node_span(imported),
                           "using declaration does not name a public symbol in the target namespace");
        free(imported_name);
        free(local_name);
        return true;
    }
    bool ok = import_symbol_as(scope, symbol, local_name, name_node->span, diagnostics);
    free(imported_name);
    free(local_name);
    return ok;
}

static bool import_selected(const XsSyntaxNode *import_node, const XsHirSymbolTable *project_symbols,
                            XsHirImportScope *scope, XsDiagnostics *diagnostics)
{
    const XsSyntaxNode *path = xs_hir_first_child_kind(import_node, XS_SYNTAX_PATH);
    char *module_name = xs_hir_path_to_string(path);
    if(module_name == nullptr)
    {
        scope->allocation_failed = true;
        return false;
    }
    if((import_node->flags & XS_SYNTAX_FLAG_USING) != 0 && strcmp(module_name, "panic") == 0)
    {
        xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, xs_hir_node_span(import_node),
                           "using namespace panic is invalid because panic exports only macros");
        free(module_name);
        return true;
    }
    bool ok = true;
    if((import_node->flags & XS_SYNTAX_FLAG_WILDCARD) != 0)
    {
        if((import_node->flags & XS_SYNTAX_FLAG_USING) != 0)
            ok = import_module_name(scope, module_name) && ok;
        for(size_t i = 0; i < project_symbols->count; ++i)
        {
            if(!xs_hir_symbol_is_public_child_of_namespace(&project_symbols->symbols[i], module_name))
                continue;
            ok = import_symbol_as(scope, &project_symbols->symbols[i], project_symbols->symbols[i].name,
                                  import_node->span, diagnostics) &&
                 ok;
        }
    }
    else
    {
        for(size_t i = 0; i < import_node->child_count; ++i)
        {
            if(import_node->children[i]->kind == XS_SYNTAX_IMPORT_NAME)
                ok = import_selected_name(import_node->children[i], module_name, project_symbols, scope, diagnostics) &&
                     ok;
        }
    }
    free(module_name);
    return ok;
}

static bool import_using_declaration(const XsSyntaxNode *import_node, const XsHirSymbolTable *project_symbols,
                                     XsHirImportScope *scope, XsDiagnostics *diagnostics)
{
    const XsSyntaxNode *path = xs_hir_first_child_kind(import_node, XS_SYNTAX_PATH);
    char *qualified = xs_hir_path_to_string(path);
    if(qualified == nullptr)
    {
        scope->allocation_failed = true;
        return false;
    }
    const char *namespace_name = nullptr;
    const char *symbol_name = nullptr;
    if(!split_qualified_name(qualified, &namespace_name, &symbol_name))
    {
        xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, xs_hir_node_span(import_node),
                           "using declaration requires a qualified name");
        free(qualified);
        return true;
    }
    const XsHirSymbol *symbol = find_symbol_in_namespace(project_symbols, namespace_name, symbol_name);
    if(symbol == nullptr)
    {
        xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, xs_hir_node_span(import_node),
                           "using declaration does not name a public symbol");
        free(qualified);
        return true;
    }
    const XsSyntaxNode *alias = xs_hir_first_child_kind(import_node, XS_SYNTAX_IDENTIFIER);
    char *local_name = alias == nullptr ? xs_hir_copy_cstr(symbol_name) : xs_hir_copy_text(alias->text);
    if(local_name == nullptr)
    {
        scope->allocation_failed = true;
        free(qualified);
        return false;
    }
    bool ok = import_symbol_as(scope, symbol, local_name, import_node->span, diagnostics);
    free(local_name);
    free(qualified);
    return ok;
}

void xs_hir_import_scope_init(XsHirImportScope *scope)
{
    *scope = (XsHirImportScope){0};
}

void xs_hir_import_scope_free(XsHirImportScope *scope)
{
    for(size_t i = 0; i < scope->count; ++i)
        free(scope->bindings[i].local_name);
    for(size_t i = 0; i < scope->module_count; ++i)
        free(scope->module_names[i]);
    free(scope->bindings);
    free(scope->module_names);
    *scope = (XsHirImportScope){0};
}

const XsHirImportBinding *xs_hir_import_scope_find(const XsHirImportScope *scope, const char *local_name)
{
    if(scope == nullptr || local_name == nullptr)
        return nullptr;
    for(size_t i = 0; i < scope->count; ++i)
    {
        if(strcmp(scope->bindings[i].local_name, local_name) == 0)
            return &scope->bindings[i];
    }
    return nullptr;
}

bool xs_hir_import_scope_has_module(const XsHirImportScope *scope, const char *namespace_name)
{
    if(scope == nullptr || namespace_name == nullptr)
        return false;
    for(size_t i = 0; i < scope->module_count; ++i)
    {
        const char *module = scope->module_names[i];
        size_t length = strlen(module);
        if(strncmp(namespace_name, module, length) == 0 &&
           (namespace_name[length] == '\0' || namespace_name[length] == '.'))
            return true;
    }
    return false;
}

bool xs_hir_resolve_imports(const XsSyntaxTree *tree, const XsHirSymbolTable *project_symbols, XsHirImportScope *scope,
                            XsDiagnostics *diagnostics)
{
    if(tree == nullptr || tree->root == nullptr || project_symbols == nullptr || scope == nullptr ||
       diagnostics == nullptr)
        return false;
    bool success = true;
    success = import_module_name(scope, "panic") && success;
    success = import_module_name(scope, "optional") && success;
    success = import_module_name(scope, "std.optional") && success;
    success = import_module_name(scope, "result") && success;
    success = import_module_name(scope, "std.result") && success;
    for(size_t i = 0; i < tree->root->child_count; ++i)
    {
        const XsSyntaxNode *child = tree->root->children[i];
        if(child->kind != XS_SYNTAX_DECL_IMPORT)
            continue;
        if((child->flags & XS_SYNTAX_FLAG_USING) != 0 && (child->flags & XS_SYNTAX_FLAG_WILDCARD) == 0)
            success = import_using_declaration(child, project_symbols, scope, diagnostics) && success;
        else if(xs_hir_first_child_kind(child, XS_SYNTAX_IMPORT_NAME) != nullptr ||
                (child->flags & XS_SYNTAX_FLAG_WILDCARD) != 0)
            success = import_selected(child, project_symbols, scope, diagnostics) && success;
        else
        {
            for(size_t j = 0; j < child->child_count; ++j)
            {
                if(child->children[j]->kind == XS_SYNTAX_PATH)
                    success = import_module_namespace(child, child->children[j], project_symbols, scope, diagnostics) &&
                              success;
            }
        }
    }
    if(scope->allocation_failed)
        xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, (XsSpan){0, 0},
                           "compiler ran out of memory while resolving HIR import");
    return success && !xs_diagnostics_has_error(diagnostics);
}
