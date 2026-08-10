/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 * Shared C and C++ ABI surface.
 *
 */

#ifndef XS_AST_H
#define XS_AST_H

#include "Visual/C23/source.hh"

#include <stddef.h>

typedef enum
{
    XS_VISIBILITY_DEFAULT,
    XS_VISIBILITY_PUBLIC,
    XS_VISIBILITY_PRIVATE,
    XS_VISIBILITY_PROTECTED,
    XS_VISIBILITY_INTERNAL,
} XsVisibility;

typedef enum
{
    XS_AST_MODULE,
    XS_AST_NAMESPACE,
    XS_AST_IMPORT,
    XS_AST_FUNCTION,
    XS_AST_CLASS,
    XS_AST_INTERFACE,
    XS_AST_DATA,
    XS_AST_ENUM,
    XS_AST_MACRO,
} XsAstItemKind;

typedef struct
{
    XsSpan span;
    XsSpan name;
    XsSpan body;
    XsVisibility visibility;
    bool is_async;
    bool is_incomplete;
    bool is_data_enum;
} XsAstItem;

typedef struct
{
    XsAstItemKind kind;
    XsAstItem item;
} XsAstNode;

typedef struct
{
    XsAstNode *items;
    size_t count;
    size_t capacity;
    bool allocation_failed;
} XsAst;

#ifdef __cplusplus
extern "C" {
#endif

void xs_ast_init(XsAst *ast);
void xs_ast_free(XsAst *ast);
bool xs_ast_push(XsAst *ast, XsAstNode node);

#ifdef __cplusplus
}
#endif

#endif
