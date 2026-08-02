/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "xs/ast.h"

#include <stdlib.h>

void xs_ast_init(XsAst *ast)
{
    *ast = (XsAst){0};
}

void xs_ast_free(XsAst *ast)
{
    free(ast->items);
    *ast = (XsAst){0};
}

bool xs_ast_push(XsAst *ast, XsAstNode node)
{
    if(ast->count == ast->capacity)
    {
        size_t capacity = ast->capacity == 0 ? 16 : ast->capacity * 2;
        XsAstNode *items = realloc(ast->items, capacity * sizeof(*items));
        if(items == nullptr)
        {
            ast->allocation_failed = true;
            return false;
        }
        ast->items = items;
        ast->capacity = capacity;
    }
    ast->items[ast->count++] = node;
    return true;
}
