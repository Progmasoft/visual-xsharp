/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "expression_check_internal.h"

#include <string.h>

static bool text_is(XsText text, const char *expected)
{
    size_t length = strlen(expected);
    return text.length == length && memcmp(text.data, expected, length) == 0;
}

static const XsSyntaxNode *first_child(const XsSyntaxNode *node, XsSyntaxKind kind)
{
    if(node == nullptr)
        return nullptr;
    for(size_t index = 0; index < node->child_count; ++index)
    {
        if(node->children[index]->kind == kind)
            return node->children[index];
    }
    return nullptr;
}

static bool path_is_single(const XsSyntaxNode *path, const char *name)
{
    return path != nullptr && path->kind == XS_SYNTAX_PATH && path->child_count == 1 &&
           path->children[0]->kind == XS_SYNTAX_IDENTIFIER && text_is(path->children[0]->text, name);
}

bool xs_hir_type_is_borrowed_str(const XsSyntaxNode *type)
{
    if(type == nullptr || type->kind != XS_SYNTAX_TYPE_REFERENCE || type->child_count != 1)
        return false;
    const XsSyntaxNode *referent = type->children[0];
    return referent->kind == XS_SYNTAX_TYPE_NAMED && path_is_single(first_child(referent, XS_SYNTAX_PATH), "Str");
}

static bool report_invalid_borrowed_str_value(const XsSyntaxNode *expression, XsDiagnostics *diagnostics)
{
    XsSpan span = {.start = expression->span.start_offset, .end = expression->span.end_offset};
    return xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, span, "value is not assignable to '&Str'");
}

bool xs_hir_check_borrowed_str_value(const XsSyntaxNode *expression, XsDiagnostics *diagnostics)
{
    if(expression == nullptr)
        return true;
    if(expression->kind == XS_SYNTAX_EXPR_LITERAL)
        return expression->token_kind == XS_TOKEN_STRING || report_invalid_borrowed_str_value(expression, diagnostics);
    return true;
}
