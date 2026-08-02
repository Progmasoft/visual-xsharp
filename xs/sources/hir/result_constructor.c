/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "expression_check_internal.h"

#include <string.h>

static bool text_matches(XsText text, const char *value)
{
    size_t length = strlen(value);
    return text.length == length && memcmp(text.data, value, length) == 0;
}

static bool path_matches(const XsSyntaxNode *path, const char *const *segments, size_t count)
{
    if(path == nullptr || path->kind != XS_SYNTAX_PATH || path->child_count != count)
        return false;
    for(size_t i = 0; i < count; ++i)
    {
        if(path->children[i]->kind != XS_SYNTAX_IDENTIFIER || !text_matches(path->children[i]->text, segments[i]))
            return false;
    }
    return true;
}

static bool named_type_matches(const XsSyntaxNode *type, const char *const *segments, size_t count)
{
    if(type == nullptr || type->kind != XS_SYNTAX_TYPE_NAMED)
        return false;
    for(size_t i = 0; i < type->child_count; ++i)
    {
        if(type->children[i]->kind == XS_SYNTAX_PATH)
            return path_matches(type->children[i], segments, count);
    }
    return false;
}

bool xs_hir_type_returns_result(const XsSyntaxNode *type)
{
    if(type == nullptr || type->kind != XS_SYNTAX_TYPE_GENERIC || type->child_count < 2)
        return false;
    static const char *const result[] = {"Result"};
    static const char *const legacy_result[] = {"Result", "Result"};
    static const char *const canonical_result[] = {"std", "result", "Result"};
    const XsSyntaxNode *base = type->children[0];
    if(named_type_matches(base, result, 1) || named_type_matches(base, legacy_result, 2) ||
       named_type_matches(base, canonical_result, 3))
        return true;
    static const char *const task[] = {"Task"};
    return type->child_count == 2 && named_type_matches(base, task, 1) && xs_hir_type_returns_result(type->children[1]);
}

bool xs_hir_check_result_constructor_call(const XsSyntaxNode *node, bool enclosing_returns_result,
                                          XsDiagnostics *diagnostics)
{
    if(node == nullptr || node->kind != XS_SYNTAX_EXPR_CALL || node->child_count == 0)
        return true;
    const XsSyntaxNode *callee = node->children[0];
    if(callee->kind != XS_SYNTAX_EXPR_IDENTIFIER || callee->child_count != 1 ||
       callee->children[0]->kind != XS_SYNTAX_IDENTIFIER)
        return true;
    XsText name = callee->children[0]->text;
    if(!text_matches(name, "Ok") && !text_matches(name, "Error"))
        return true;
    if(enclosing_returns_result)
        return true;
    XsSpan span = {.start = node->span.start_offset, .end = node->span.end_offset};
    return xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, span,
                              "Ok and Error constructors require an enclosing function returning Result<T, E>");
}
