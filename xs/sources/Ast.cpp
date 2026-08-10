// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

#include "Visual/XSharp/ast.hh"

#include <cstdlib>

void xs_ast_init(XsAst *ast)
{
    *ast = XsAst{};
}

void xs_ast_free(XsAst *ast)
{
    std::free(ast->items);
    *ast = XsAst{};
}

bool xs_ast_push(XsAst *ast, XsAstNode node)
{
    if(ast->count == ast->capacity)
    {
        const std::size_t capacity = ast->capacity == 0 ? 16 : ast->capacity * 2;
        auto *items = static_cast<XsAstNode *>(std::realloc(ast->items, capacity * sizeof(XsAstNode)));
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
