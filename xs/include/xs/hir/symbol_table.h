/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef XS_HIR_SYMBOL_TABLE_H
#define XS_HIR_SYMBOL_TABLE_H

#include "xs/diagnostic.h"
#include "xs/macro.h"
#include "xs/syntax_ast.h"

#include <stddef.h>

typedef enum
{
    XS_HIR_SYMBOL_FUNCTION,
    XS_HIR_SYMBOL_CLASS,
    XS_HIR_SYMBOL_INTERFACE,
    XS_HIR_SYMBOL_ENUM,
    XS_HIR_SYMBOL_DATA,
    XS_HIR_SYMBOL_MACRO,
    XS_HIR_SYMBOL_EXTERN_GLOBAL,
} XsHirSymbolKind;

typedef enum
{
    XS_HIR_MEMBER_FIELD,
    XS_HIR_MEMBER_METHOD,
    XS_HIR_MEMBER_CONSTRUCTOR,
    XS_HIR_MEMBER_DESTRUCTOR,
    XS_HIR_MEMBER_NESTED_TYPE,
    XS_HIR_MEMBER_VARIANT,
} XsHirMemberKind;

typedef struct
{
    XsHirSymbolKind kind;
    char *name;
    char *module_name;
    char *namespace_name;
    char *qualified_name;
    XsSyntaxVisibility visibility;
    XsSourceSpan span;
    const XsSyntaxNode *syntax;
} XsHirSymbol;

typedef struct
{
    char *local_name;
    const XsHirSymbol *symbol;
    XsSourceSpan span;
} XsHirImportBinding;

typedef struct
{
    XsHirSymbol *symbols;
    size_t count;
    size_t capacity;
    bool allocation_failed;
} XsHirSymbolTable;

typedef struct
{
    XsHirImportBinding *bindings;
    char **module_names;
    size_t count;
    size_t capacity;
    size_t module_count;
    size_t module_capacity;
    bool allocation_failed;
} XsHirImportScope;

typedef struct
{
    XsHirMemberKind kind;
    char *name;
    char *owner_qualified_name;
    char *qualified_name;
    XsSyntaxVisibility visibility;
    XsSourceSpan span;
    const XsSyntaxNode *syntax;
} XsHirMemberSymbol;

typedef struct
{
    XsHirMemberSymbol *members;
    size_t count;
    size_t capacity;
    bool allocation_failed;
} XsHirMemberSymbolTable;

void xs_hir_symbol_table_init(XsHirSymbolTable *table);
void xs_hir_symbol_table_free(XsHirSymbolTable *table);
const XsHirSymbol *xs_hir_symbol_table_find(const XsHirSymbolTable *table, const char *qualified_name);
bool xs_hir_collect_symbols(const XsSyntaxTree *tree, XsHirSymbolTable *table, XsDiagnostics *diagnostics);
bool xs_hir_collect_symbols_expanded(const XsSyntaxTree *tree, const XsMacroDeclarationExpansionSet *macro_declarations,
                                     XsHirSymbolTable *table, XsDiagnostics *diagnostics);
bool xs_hir_collect_symbols_in_module_expanded(const XsSyntaxTree *tree,
                                               const XsMacroDeclarationExpansionSet *macro_declarations,
                                               const char *module_name, XsHirSymbolTable *table,
                                               XsDiagnostics *diagnostics);
const char *xs_hir_symbol_kind_name(XsHirSymbolKind kind);
const char *xs_hir_member_kind_name(XsHirMemberKind kind);

void xs_hir_member_symbol_table_init(XsHirMemberSymbolTable *table);
void xs_hir_member_symbol_table_free(XsHirMemberSymbolTable *table);
const XsHirMemberSymbol *xs_hir_member_symbol_table_find(const XsHirMemberSymbolTable *table,
                                                         const char *owner_qualified_name, const char *name);
bool xs_hir_collect_member_symbols(const XsHirSymbol *owner, const XsMacroDeclarationExpansionSet *macro_declarations,
                                   XsHirMemberSymbolTable *table, XsDiagnostics *diagnostics);

void xs_hir_import_scope_init(XsHirImportScope *scope);
void xs_hir_import_scope_free(XsHirImportScope *scope);
const XsHirImportBinding *xs_hir_import_scope_find(const XsHirImportScope *scope, const char *local_name);
bool xs_hir_import_scope_has_module(const XsHirImportScope *scope, const char *namespace_name);
bool xs_hir_resolve_imports(const XsSyntaxTree *tree, const XsHirSymbolTable *project_symbols, XsHirImportScope *scope,
                            XsDiagnostics *diagnostics);
bool xs_hir_validate_name_uses(const XsSyntaxTree *tree, const XsHirSymbolTable *project_symbols,
                               const XsHirImportScope *import, XsDiagnostics *diagnostics);
bool xs_hir_validate_name_uses_expanded(const XsSyntaxTree *tree, const XsMacroStatementExpansionSet *macro_statements,
                                        const XsHirSymbolTable *project_symbols, const XsHirImportScope *import,
                                        XsDiagnostics *diagnostics);
bool xs_hir_validate_name_uses_with_macros(const XsSyntaxTree *tree,
                                           const XsMacroDeclarationExpansionSet *macro_declarations,
                                           const XsMacroStatementExpansionSet *macro_statements,
                                           const XsHirSymbolTable *project_symbols, const XsHirImportScope *import,
                                           XsDiagnostics *diagnostics);

#endif
