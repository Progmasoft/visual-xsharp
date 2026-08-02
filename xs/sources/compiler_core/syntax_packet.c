/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "xs/compiler_core.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static_assert(sizeof(XsCompilerCoreSyntaxNode) == 112);
static_assert(offsetof(XsCompilerCoreSyntaxNode, parent_index) == 16);
static_assert(offsetof(XsCompilerCoreSyntaxNode, end_column) == 104);
static_assert(sizeof(XsCompilerCoreSyntaxPacket) == 64);
static_assert(offsetof(XsCompilerCoreSyntaxPacket, nodes) == 16);
static_assert(offsetof(XsCompilerCoreSyntaxPacket, text_byte_count) == 56);
static_assert(XS_SYNTAX_FILE == 0 && XS_SYNTAX_DECL_MODULE == 1 && XS_SYNTAX_DECL_FUNCTION == 4);
static_assert(XS_SYNTAX_PARAMETER == 21 && XS_SYNTAX_IDENTIFIER == 24 && XS_SYNTAX_PATH == 25);
static_assert(XS_SYNTAX_TYPE_NAMED == 27 && XS_SYNTAX_TYPE_UNIT == 36);
static_assert(XS_SYNTAX_DECL_VARIABLE == 9 && XS_SYNTAX_STMT_BLOCK == 38 && XS_SYNTAX_STMT_VARIABLE == 40);
static_assert(XS_SYNTAX_STMT_RETURN == 41 && XS_SYNTAX_EXPR_IDENTIFIER == 56 && XS_SYNTAX_EXPR_LITERAL == 57);
static_assert(XS_SYNTAX_STMT_IF == 42 && XS_SYNTAX_STMT_ELSE_IF == 43 && XS_SYNTAX_EXPR_IF == 81 &&
              XS_SYNTAX_EXPR_MATCH == 82);
static_assert(XS_SYNTAX_STMT_FOR == 44 && XS_SYNTAX_STMT_FOR_EACH == 45 && XS_SYNTAX_EXPR_UNARY == 59);
static_assert(XS_SYNTAX_STMT_WHILE == 46 && XS_SYNTAX_STMT_BREAK == 49 && XS_SYNTAX_STMT_CONTINUE == 50);
static_assert(XS_SYNTAX_STMT_MATCH == 47 && XS_SYNTAX_MATCH_ARM == 48);
static_assert(XS_SYNTAX_PATTERN_LITERAL == 85 && XS_SYNTAX_PATTERN_ELSE == 88);
static_assert(XS_SYNTAX_EXPR_GENERIC_QUALIFIER == 101 && XS_SYNTAX_EXPR_TYPED_OBJECT_LITERAL == 102);
static_assert(XS_SYNTAX_PATTERN_TYPED == 103 && XS_SYNTAX_DECL_PATTERN_VARIABLE == 104 && XS_SYNTAX_STMT_LOOP == 105);
static_assert(XS_SYNTAX_TYPE_MAP == 106);
static_assert(XS_SYNTAX_EXPR_MAP_LITERAL == 107 && XS_SYNTAX_MAP_ENTRY == 108);
static_assert(XS_SYNTAX_EXPR_SET_LITERAL == 109);
static_assert(XS_SYNTAX_TUPLE_FIELD == 110);
static_assert(XS_SYNTAX_EXPR_BINARY == 58 && XS_SYNTAX_EXPR_ASSIGNMENT == 60 && XS_SYNTAX_EXPR_CALL == 61);
static_assert(XS_TOKEN_INTEGER == 3 && XS_TOKEN_ASSIGN == 24 && XS_TOKEN_PLUS == 38 && XS_TOKEN_STAR == 44);
static_assert(XS_TOKEN_BANG == 26 && XS_TOKEN_NOT_EQUAL == 27 && XS_TOKEN_MINUS == 41);
static_assert(XS_TOKEN_PLUS_PLUS == 39 && XS_TOKEN_PLUS_ASSIGN == 40 && XS_TOKEN_MINUS_MINUS == 42);
static_assert(XS_TOKEN_SHIFT_RIGHT == 34 && XS_TOKEN_SHIFT_LEFT == 37 && XS_TOKEN_SLASH == 46 &&
              XS_TOKEN_PERCENT == 48);
static_assert(XS_TOKEN_AMPERSAND == 50 && XS_TOKEN_LOGICAL_AND == 51 && XS_TOKEN_PIPE == 53 &&
              XS_TOKEN_LOGICAL_OR == 54 && XS_TOKEN_CARET == 56);

struct XsCompilerCoreSyntaxStorage
{
    XsCompilerCoreSyntaxPacket packet;
    XsCompilerCoreSyntaxNode *nodes;
    uint64_t *child_indices;
    uint8_t *text_bytes;
};

typedef struct
{
    uint64_t nodes;
    uint64_t children;
    uint64_t text_bytes;
} PacketSize;

typedef struct
{
    XsCompilerCoreSyntaxStorage *storage;
    uint64_t node_cursor;
    uint64_t child_cursor;
    uint64_t text_cursor;
} PacketWriter;

static bool add_size(uint64_t *value, size_t addition)
{
    if(addition > UINT64_MAX || *value > UINT64_MAX - (uint64_t)addition)
        return false;
    *value += (uint64_t)addition;
    return true;
}

static bool measure_node(const XsSyntaxNode *node, size_t depth, PacketSize *size)
{
    if(node == nullptr || depth > 4096 || !add_size(&size->nodes, 1) || !add_size(&size->children, node->child_count) ||
       !add_size(&size->text_bytes, node->text.length))
        return false;
    for(size_t index = 0; index < node->child_count; ++index)
    {
        if(!measure_node(node->children[index], depth + 1, size))
            return false;
    }
    return true;
}

static bool allocation_size(uint64_t count, size_t item_size, size_t *bytes)
{
    if(count > SIZE_MAX || (size_t)count > SIZE_MAX / item_size)
        return false;
    *bytes = (size_t)count * item_size;
    return true;
}

static uint64_t write_node(PacketWriter *writer, const XsSyntaxNode *node, uint64_t parent)
{
    uint64_t node_index = writer->node_cursor++;
    XsCompilerCoreSyntaxNode *record = &writer->storage->nodes[node_index];
    *record = (XsCompilerCoreSyntaxNode){
        .kind = (uint32_t)node->kind,
        .token_kind = (uint32_t)node->token_kind,
        .visibility = (uint32_t)node->visibility,
        .flags = node->flags,
        .parent_index = parent,
        .first_child = writer->child_cursor,
        .child_count = (uint64_t)node->child_count,
        .text_offset = writer->text_cursor,
        .text_length = (uint64_t)node->text.length,
        .file_id = node->span.file_id,
        .start_offset = (uint64_t)node->span.start_offset,
        .end_offset = (uint64_t)node->span.end_offset,
        .start_line = (uint64_t)node->span.start_line,
        .start_column = (uint64_t)node->span.start_column,
        .end_line = (uint64_t)node->span.end_line,
        .end_column = (uint64_t)node->span.end_column,
    };
    uint64_t child_start = writer->child_cursor;
    writer->child_cursor += (uint64_t)node->child_count;
    if(node->text.length != 0)
    {
        memcpy(writer->storage->text_bytes + writer->text_cursor, node->text.data, node->text.length);
        writer->text_cursor += (uint64_t)node->text.length;
    }
    for(size_t index = 0; index < node->child_count; ++index)
        writer->storage->child_indices[child_start + (uint64_t)index] =
            write_node(writer, node->children[index], node_index);
    return node_index;
}

XsCompilerCoreStatus xs_compiler_core_syntax_packet_create(const XsSyntaxTree *tree,
                                                           XsCompilerCoreSyntaxStorage **storage)
{
    if(storage == nullptr || tree == nullptr || tree->root == nullptr)
        return XS_COMPILER_CORE_INVALID_ARGUMENT;
    *storage = nullptr;
    PacketSize size = {0};
    if(!measure_node(tree->root, 0, &size))
        return XS_COMPILER_CORE_LIMIT_EXCEEDED;
    size_t node_bytes = 0;
    size_t child_bytes = 0;
    size_t text_bytes = 0;
    if(!allocation_size(size.nodes, sizeof(XsCompilerCoreSyntaxNode), &node_bytes) ||
       !allocation_size(size.children, sizeof(uint64_t), &child_bytes) ||
       !allocation_size(size.text_bytes, sizeof(uint8_t), &text_bytes))
        return XS_COMPILER_CORE_LIMIT_EXCEEDED;
    XsCompilerCoreSyntaxStorage *result = calloc(1, sizeof(*result));
    if(result == nullptr)
        return XS_COMPILER_CORE_ALLOCATION_FAILED;
    result->nodes = malloc(node_bytes);
    result->child_indices = child_bytes == 0 ? nullptr : malloc(child_bytes);
    result->text_bytes = text_bytes == 0 ? nullptr : malloc(text_bytes);
    if(result->nodes == nullptr || (child_bytes != 0 && result->child_indices == nullptr) ||
       (text_bytes != 0 && result->text_bytes == nullptr))
    {
        xs_compiler_core_syntax_packet_free(result);
        return XS_COMPILER_CORE_ALLOCATION_FAILED;
    }
    PacketWriter writer = {.storage = result};
    uint64_t root_index = write_node(&writer, tree->root, XS_COMPILER_CORE_NO_NODE);
    result->packet = (XsCompilerCoreSyntaxPacket){
        .abi_version = XS_COMPILER_CORE_SYNTAX_ABI_VERSION,
        .root_index = root_index,
        .nodes = result->nodes,
        .node_count = size.nodes,
        .child_indices = result->child_indices,
        .child_index_count = size.children,
        .text_bytes = result->text_bytes,
        .text_byte_count = size.text_bytes,
    };
    *storage = result;
    return XS_COMPILER_CORE_OK;
}

const XsCompilerCoreSyntaxPacket *xs_compiler_core_syntax_packet(const XsCompilerCoreSyntaxStorage *storage)
{
    return storage == nullptr ? nullptr : &storage->packet;
}

void xs_compiler_core_syntax_packet_free(XsCompilerCoreSyntaxStorage *storage)
{
    if(storage == nullptr)
        return;
    free(storage->nodes);
    free(storage->child_indices);
    free(storage->text_bytes);
    free(storage);
}
