/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef XS_HIR_TYPE_RESOLUTION_INTERNAL_H
#define XS_HIR_TYPE_RESOLUTION_INTERNAL_H

#include "Visual/XSharp/macro.h"
#include "Visual/XSharp/syntax_ast.hh"

bool xs_hir_declaration_uses_expanded_member_view(const XsSyntaxNode *node,
                                                  const XsMacroDeclarationExpansionSet *macro_declarations);
bool xs_hir_node_has_statement_macro_child(const XsSyntaxNode *node);

#endif
