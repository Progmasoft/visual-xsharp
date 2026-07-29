/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef XS_HIR_SYNTAX_HELPERS_H
#define XS_HIR_SYNTAX_HELPERS_H

#include "xs/hir/symbol_table.h"

char *xs_hir_copy_text(XsText text);
char *xs_hir_copy_cstr(const char *text);
const XsSyntaxNode *xs_hir_first_child_kind(const XsSyntaxNode *node, XsSyntaxKind kind);
const XsSyntaxNode *xs_hir_second_child_kind(const XsSyntaxNode *node, XsSyntaxKind kind);
char *xs_hir_path_to_string(const XsSyntaxNode *path);
char *xs_hir_join_qualified(const char *namespace_name, const char *name);
XsSpan xs_hir_node_span(const XsSyntaxNode *node);
bool xs_hir_symbol_is_public_child_of_namespace(const XsHirSymbol *symbol, const char *namespace_name);

#endif
