/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "type_resolution_internal.h"

static bool declaration_has_child_macro_call(const XsSyntaxNode *node)
{
    if(node == nullptr)
        return false;
    for(size_t i = 0; i < node->child_count; ++i)
    {
        if(node->children[i]->kind == XS_SYNTAX_DECL_MACRO_CALL)
            return true;
    }
    return false;
}

bool xs_hir_node_has_statement_macro_child(const XsSyntaxNode *node)
{
    if(node == nullptr)
        return false;
    for(size_t i = 0; i < node->child_count; ++i)
    {
        if(node->children[i]->kind == XS_SYNTAX_STMT_MACRO_CALL)
            return true;
    }
    return false;
}

bool xs_hir_declaration_uses_expanded_member_view(const XsSyntaxNode *node,
                                                  const XsMacroDeclarationExpansionSet *macro_declarations)
{
    if(macro_declarations == nullptr || node == nullptr)
        return false;
    return (node->kind == XS_SYNTAX_DECL_CLASS || node->kind == XS_SYNTAX_DECL_INTERFACE) &&
           declaration_has_child_macro_call(node);
}
