/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "Visual/XSharp/macro.h"

static bool spans_equal(XsSpan span, const XsSyntaxNode *call)
{
    return call != nullptr && span.start == call->span.start_offset && span.end == call->span.end_offset;
}

static const XsSyntaxNode *macro_call_expr(const XsSyntaxNode *node)
{
    return xs_syntax_find_first(node, XS_SYNTAX_EXPR_MACRO_CALL);
}

static bool clone_expanded_node(XsSyntaxTree *target, const XsSyntaxNode *source,
                                const XsMacroDeclarationExpansionSet *declarations,
                                const XsMacroStatementExpansionSet *statements, XsSyntaxNode **result);

static bool add_cloned_subtree(XsSyntaxTree *target, XsSyntaxNode *parent, const XsSyntaxNode *source)
{
    XsSyntaxNode *clone = xs_syntax_clone_subtree(target, source);
    return clone != nullptr && xs_syntax_node_add(target, parent, clone);
}

static bool add_declaration_replacements(XsSyntaxTree *target, XsSyntaxNode *parent, const XsSyntaxNode *macro_call,
                                         const XsMacroDeclarationExpansionSet *declarations)
{
    const XsSyntaxNode *call = macro_call_expr(macro_call);
    for(size_t i = 0; declarations != nullptr && i < declarations->count; ++i)
    {
        const XsMacroDeclarationExpansion *expansion = &declarations->items[i];
        if(!spans_equal(expansion->call_span, call) || expansion->reparse.tree.root == nullptr)
            continue;
        for(size_t j = 0; j < expansion->reparse.tree.root->child_count; ++j)
        {
            if(!add_cloned_subtree(target, parent, expansion->reparse.tree.root->children[j]))
                return false;
        }
    }
    return true;
}

static bool add_statement_replacements(XsSyntaxTree *target, XsSyntaxNode *parent, const XsSyntaxNode *macro_call,
                                       const XsMacroStatementExpansionSet *statements, bool *replaced)
{
    *replaced = false;
    const XsSyntaxNode *call = macro_call_expr(macro_call);
    for(size_t i = 0; statements != nullptr && i < statements->count; ++i)
    {
        const XsMacroStatementExpansion *expansion = &statements->items[i];
        if(!spans_equal(expansion->call_span, call) || expansion->statement == nullptr)
            continue;
        *replaced = true;
        if(!add_cloned_subtree(target, parent, expansion->statement))
            return false;
    }
    return true;
}

static bool clone_expanded_children(XsSyntaxTree *target, XsSyntaxNode *parent, const XsSyntaxNode *source,
                                    const XsMacroDeclarationExpansionSet *declarations,
                                    const XsMacroStatementExpansionSet *statements)
{
    for(size_t i = 0; i < source->child_count; ++i)
    {
        const XsSyntaxNode *child = source->children[i];
        if(child->kind == XS_SYNTAX_DECL_MACRO_CALL)
        {
            if(!add_declaration_replacements(target, parent, child, declarations))
                return false;
            continue;
        }
        if(child->kind == XS_SYNTAX_STMT_MACRO_CALL)
        {
            bool replaced = false;
            if(!add_statement_replacements(target, parent, child, statements, &replaced) ||
               (!replaced && !add_cloned_subtree(target, parent, child)))
                return false;
            continue;
        }
        XsSyntaxNode *clone = nullptr;
        if(!clone_expanded_node(target, child, declarations, statements, &clone) ||
           !xs_syntax_node_add(target, parent, clone))
            return false;
    }
    return true;
}

static bool clone_expanded_node(XsSyntaxTree *target, const XsSyntaxNode *source,
                                const XsMacroDeclarationExpansionSet *declarations,
                                const XsMacroStatementExpansionSet *statements, XsSyntaxNode **result)
{
    XsSyntaxNode *clone = xs_syntax_node_clone_shallow(target, source);
    if(clone == nullptr)
        return false;
    if(!clone_expanded_children(target, clone, source, declarations, statements))
        return false;
    *result = clone;
    return true;
}

bool xs_macro_materialize_expanded_tree(const XsSyntaxTree *tree, const XsMacroDeclarationExpansionSet *declarations,
                                        const XsMacroStatementExpansionSet *statements, XsDiagnostics *diagnostics,
                                        XsSyntaxTree *expanded)
{
    if(tree == nullptr || tree->root == nullptr || diagnostics == nullptr || expanded == nullptr)
        return false;
    xs_syntax_tree_init(expanded, tree->source, tree->file_id);
    if(!clone_expanded_node(expanded, tree->root, declarations, statements, &expanded->root))
    {
        xs_syntax_tree_free(expanded);
        xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, (XsSpan){0, 0},
                           "compiler ran out of memory while materializing expanded macro tree");
        return false;
    }
    return !expanded->allocation_failed && !xs_diagnostics_has_error(diagnostics);
}
