/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "xs/syntax_ast.h"

#include <stdalign.h>
#include <stdlib.h>
#include <string.h>

struct XsSyntaxArenaBlock
{
    XsSyntaxArenaBlock *next;
    size_t used;
    size_t capacity;
    max_align_t alignment;
    unsigned char data[];
};

static void *arena_allocate(XsSyntaxTree *tree, size_t size)
{
    const size_t alignment = alignof(max_align_t);
    size = (size + alignment - 1) & ~(alignment - 1);
    XsSyntaxArenaBlock *block = tree->arena;
    if(block == nullptr || block->capacity - block->used < size)
    {
        size_t capacity = size > 4096 ? size : 4096;
        block = malloc(sizeof(*block) + capacity);
        if(block == nullptr)
        {
            tree->allocation_failed = true;
            return nullptr;
        }
        block->next = tree->arena;
        block->used = 0;
        block->capacity = capacity;
        tree->arena = block;
    }
    void *result = block->data + block->used;
    block->used += size;
    memset(result, 0, size);
    return result;
}

void xs_syntax_tree_init(XsSyntaxTree *tree, const XsSource *source, uint64_t file_id)
{
    *tree = (XsSyntaxTree){.source = source, .file_id = file_id};
}

void xs_syntax_tree_free(XsSyntaxTree *tree)
{
    XsSyntaxArenaBlock *block = tree->arena;
    while(block != nullptr)
    {
        XsSyntaxArenaBlock *next = block->next;
        free(block);
        block = next;
    }
    *tree = (XsSyntaxTree){0};
}

static void line_column(const XsSource *source, size_t offset, size_t *line, size_t *column)
{
    *line = 1;
    *column = 1;
    if(offset > source->length)
        offset = source->length;
    for(size_t i = 0; i < offset; ++i)
    {
        if(source->text[i] == '\n')
        {
            ++*line;
            *column = 1;
        }
        else
        {
            ++*column;
        }
    }
}

XsSourceSpan xs_source_span(const XsSyntaxTree *tree, XsSpan span)
{
    XsSourceSpan result = {
        .file_id = tree->file_id,
        .start_offset = span.start,
        .end_offset = span.end,
    };
    line_column(tree->source, span.start, &result.start_line, &result.start_column);
    line_column(tree->source, span.end, &result.end_line, &result.end_column);
    return result;
}

XsSyntaxNode *xs_syntax_node_new(XsSyntaxTree *tree, XsSyntaxKind kind, XsSpan span)
{
    XsSyntaxNode *node = arena_allocate(tree, sizeof(*node));
    if(node == nullptr)
        return nullptr;
    node->kind = kind;
    node->span = xs_source_span(tree, span);
    node->text = xs_source_text(tree->source, span);
    node->visibility = XS_SYNTAX_VISIBILITY_DEFAULT;
    return node;
}

bool xs_syntax_node_add(XsSyntaxTree *tree, XsSyntaxNode *parent, XsSyntaxNode *child)
{
    if(parent == nullptr || child == nullptr)
        return false;
    if(parent->child_count == parent->child_capacity)
    {
        size_t capacity = parent->child_capacity == 0 ? 4 : parent->child_capacity * 2;
        XsSyntaxNode **children = arena_allocate(tree, capacity * sizeof(*children));
        if(children == nullptr)
            return false;
        if(parent->children != nullptr)
            memcpy(children, parent->children, parent->child_count * sizeof(*children));
        parent->children = children;
        parent->child_capacity = capacity;
    }
    parent->children[parent->child_count++] = child;
    return true;
}

XsSyntaxNode *xs_syntax_node_clone_shallow(XsSyntaxTree *tree, const XsSyntaxNode *node)
{
    if(tree == nullptr || node == nullptr)
        return nullptr;
    XsSyntaxNode *clone = arena_allocate(tree, sizeof(*clone));
    if(clone == nullptr)
        return nullptr;
    *clone = *node;
    clone->children = nullptr;
    clone->child_count = 0;
    clone->child_capacity = 0;
    return clone;
}

XsSyntaxNode *xs_syntax_clone_subtree(XsSyntaxTree *tree, const XsSyntaxNode *node)
{
    XsSyntaxNode *clone = xs_syntax_node_clone_shallow(tree, node);
    if(clone == nullptr)
        return nullptr;
    for(size_t i = 0; i < node->child_count; ++i)
    {
        XsSyntaxNode *child = xs_syntax_clone_subtree(tree, node->children[i]);
        if(!xs_syntax_node_add(tree, clone, child))
            return nullptr;
    }
    return clone;
}

const XsSyntaxNode *xs_syntax_find_first(const XsSyntaxNode *node, XsSyntaxKind kind)
{
    if(node == nullptr)
        return nullptr;
    if(node->kind == kind)
        return node;
    for(size_t i = 0; i < node->child_count; ++i)
    {
        const XsSyntaxNode *found = xs_syntax_find_first(node->children[i], kind);
        if(found != nullptr)
            return found;
    }
    return nullptr;
}

const char *xs_syntax_kind_name(XsSyntaxKind kind)
{
    static const char *const names[] = {
        "file",
        "module declaration",
        "import declaration",
        "namespace declaration",
        "function declaration",
        "class declaration",
        "interface declaration",
        "enum declaration",
        "data declaration",
        "variable declaration",
        "macro declaration",
        "macro-call declaration",
        "extern block",
        "attribute list",
        "attribute",
        "class field",
        "property accessor",
        "constructor",
        "destructor",
        "enum variant",
        "data field",
        "parameter",
        "generic parameter",
        "import name",
        "identifier",
        "path",
        "visibility",
        "named type",
        "generic type",
        "array type",
        "fixed array type",
        "pointer type",
        "reference type",
        "mutable reference type",
        "tuple type",
        "function type",
        "unit type",
        "lifetime",
        "block statement",
        "expression statement",
        "variable statement",
        "return statement",
        "if statement",
        "else-if branch",
        "for statement",
        "for-each statement",
        "while statement",
        "match statement",
        "match arm",
        "break statement",
        "continue statement",
        "reserved syntax 51",
        "reserved syntax 52",
        "reserved syntax 53",
        "macro-call statement",
        "discard statement",
        "identifier expression",
        "literal expression",
        "binary expression",
        "unary expression",
        "assignment expression",
        "call expression",
        "method call",
        "member access",
        "optional method call",
        "optional member access",
        "optional-forgiving expression",
        "result propagation expression",
        "index expression",
        "new expression",
        "function expression",
        "await expression",
        "move expression",
        "borrow expression",
        "mutable borrow expression",
        "dereference expression",
        "array literal",
        "object literal",
        "object field",
        "field set expression",
        "tuple expression",
        "if expression",
        "match expression",
        "macro-call expression",
        "identifier pattern",
        "literal pattern",
        "enum-variant pattern",
        "tuple pattern",
        "else pattern",
        "macro rule",
        "macro matcher",
        "macro token matcher",
        "macro fragment matcher",
        "macro repetition matcher",
        "macro expansion",
        "macro expansion token",
        "macro expansion variable",
        "macro expansion repetition",
        "macro argument",
        "token",
        "base specifier",
        "generic qualifier expression",
        "typed object literal",
        "typed pattern",
        "pattern variable declaration",
        "loop statement",
        "map type",
        "map literal",
        "map entry",
        "set literal",
        "tuple field",
    };
    if((size_t)kind >= sizeof(names) / sizeof(names[0]))
        return "unknown syntax node";
    return names[kind];
}
