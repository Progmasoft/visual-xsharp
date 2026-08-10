/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 *
 */

#ifndef XS_MACRO_H
#define XS_MACRO_H

#include "Visual/C23/diagnostic.hh"
#include "Visual/XSharp/syntax_ast.hh"

#include <stdint.h>

typedef struct
{
    size_t calls_seen;
    size_t calls_resolved;
    size_t calls_expandable;
    size_t calls_deferred;
    size_t output_tokens_planned;
    size_t substitutions_planned;
} XsMacroExpansionReport;

typedef struct
{
    XsTokenKind kind;
    XsText text;
    XsSpan source_span;
    bool from_substitution;
} XsMacroExpansionToken;

typedef struct
{
    XsSpan call_span;
    XsMacroExpansionToken *tokens;
    size_t token_count;
    size_t token_capacity;
} XsMacroExpansion;

typedef struct
{
    XsMacroExpansion *items;
    size_t count;
    size_t capacity;
    bool allocation_failed;
} XsMacroExpansionSet;

typedef struct
{
    char *text;
    XsSource source;
    XsSyntaxTree tree;
} XsMacroReparseResult;

typedef struct
{
    XsSpan call_span;
    XsMacroReparseResult reparse;
    const XsSyntaxNode *statement;
} XsMacroStatementExpansion;

typedef struct
{
    XsSpan call_span;
    XsMacroReparseResult reparse;
    size_t declaration_count;
} XsMacroDeclarationExpansion;

typedef struct
{
    XsMacroStatementExpansion *items;
    size_t count;
    size_t capacity;
    bool allocation_failed;
} XsMacroStatementExpansionSet;

typedef struct
{
    XsMacroDeclarationExpansion *items;
    size_t count;
    size_t capacity;
    bool allocation_failed;
} XsMacroDeclarationExpansionSet;

typedef struct
{
    const XsSyntaxNode *declaration;
    XsSpan call_span;
    bool from_macro_expansion;
} XsMacroExpandedDeclaration;

typedef struct
{
    const XsSyntaxNode *statement;
    XsSpan call_span;
    bool from_macro_expansion;
} XsMacroExpandedStatement;

typedef struct
{
    XsMacroExpandedDeclaration *items;
    size_t count;
    size_t capacity;
    bool allocation_failed;
} XsMacroExpandedDeclarationSet;

typedef struct
{
    XsMacroExpandedStatement *items;
    size_t count;
    size_t capacity;
    bool allocation_failed;
} XsMacroExpandedStatementSet;

bool xs_macro_validate(const XsSyntaxTree *tree, XsDiagnostics *diagnostics);
bool xs_macro_prepare_expansion(const XsSyntaxTree *tree, XsDiagnostics *diagnostics, XsMacroExpansionReport *report);
void xs_macro_expansion_set_free(XsMacroExpansionSet *set);
bool xs_macro_expand_tokens(const XsSyntaxTree *tree, XsDiagnostics *diagnostics, XsMacroExpansionSet *set);
bool xs_macro_reparse_expansion_as_statement(const XsMacroExpansion *expansion, uint64_t file_id,
                                             XsDiagnostics *diagnostics, XsMacroReparseResult *result);
bool xs_macro_reparse_expansion_as_declarations(const XsMacroExpansion *expansion, uint64_t file_id,
                                                XsDiagnostics *diagnostics, XsMacroReparseResult *result);
const XsSyntaxNode *xs_macro_reparse_result_statement(const XsMacroReparseResult *result);
void xs_macro_reparse_result_free(XsMacroReparseResult *result);
void xs_macro_statement_expansion_set_free(XsMacroStatementExpansionSet *set);
void xs_macro_declaration_expansion_set_free(XsMacroDeclarationExpansionSet *set);
bool xs_macro_expand_statements(const XsSyntaxTree *tree, XsDiagnostics *diagnostics,
                                XsMacroStatementExpansionSet *set);
bool xs_macro_expand_declarations(const XsSyntaxTree *tree, XsDiagnostics *diagnostics,
                                  XsMacroDeclarationExpansionSet *set);
const XsSyntaxNode *xs_macro_statement_expansion_find(const XsMacroStatementExpansionSet *set,
                                                      const XsSyntaxNode *statement);
bool xs_macro_statement_expansion_matches(const XsMacroStatementExpansion *expansion, const XsSyntaxNode *statement);
const XsMacroDeclarationExpansion *xs_macro_declaration_expansion_find(const XsMacroDeclarationExpansionSet *set,
                                                                       const XsSyntaxNode *declaration);
bool xs_macro_expand_top_level_declarations(const XsSyntaxTree *tree,
                                            const XsMacroDeclarationExpansionSet *declarations,
                                            XsDiagnostics *diagnostics, XsMacroExpandedDeclarationSet *expanded);
bool xs_macro_expand_child_declarations(const XsSyntaxNode *parent, const XsMacroDeclarationExpansionSet *declarations,
                                        XsDiagnostics *diagnostics, XsMacroExpandedDeclarationSet *expanded);
bool xs_macro_expand_child_statements(const XsSyntaxNode *parent, const XsMacroStatementExpansionSet *statements,
                                      XsDiagnostics *diagnostics, XsMacroExpandedStatementSet *expanded);
bool xs_macro_materialize_expanded_tree(const XsSyntaxTree *tree, const XsMacroDeclarationExpansionSet *declarations,
                                        const XsMacroStatementExpansionSet *statements, XsDiagnostics *diagnostics,
                                        XsSyntaxTree *expanded);
void xs_macro_expanded_declaration_set_free(XsMacroExpandedDeclarationSet *set);
void xs_macro_expanded_statement_set_free(XsMacroExpandedStatementSet *set);

#endif
