/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "syntax_helpers.h"

#include <stdlib.h>
#include <string.h>

char *xs_hir_copy_text(XsText text)
{
  char *copy = malloc(text.length + 1);
  if(copy == nullptr)
    return nullptr;
  memcpy(copy, text.data, text.length);
  copy[text.length] = '\0';
  return copy;
}

char *xs_hir_copy_cstr(const char *text)
{
  size_t length = strlen(text);
  char *copy = malloc(length + 1);
  if(copy != nullptr)
    memcpy(copy, text, length + 1);
  return copy;
}

const XsSyntaxNode *xs_hir_first_child_kind(const XsSyntaxNode *node, XsSyntaxKind kind)
{
  if(node == nullptr)
    return nullptr;
  for(size_t i = 0; i < node->child_count; ++i)
  {
    if(node->children[i]->kind == kind)
      return node->children[i];
  }
  return nullptr;
}

const XsSyntaxNode *xs_hir_second_child_kind(const XsSyntaxNode *node, XsSyntaxKind kind)
{
  bool found_first = false;
  if(node == nullptr)
    return nullptr;
  for(size_t i = 0; i < node->child_count; ++i)
  {
    if(node->children[i]->kind != kind)
      continue;
    if(found_first)
      return node->children[i];
    found_first = true;
  }
  return nullptr;
}

char *xs_hir_path_to_string(const XsSyntaxNode *path)
{
  if(path == nullptr || path->kind != XS_SYNTAX_PATH)
    return nullptr;
  size_t length = 0;
  size_t segments = 0;
  for(size_t i = 0; i < path->child_count; ++i)
  {
    if(path->children[i]->kind != XS_SYNTAX_IDENTIFIER)
      continue;
    length += path->children[i]->text.length;
    ++segments;
  }
  if(segments == 0)
    return xs_hir_copy_cstr("");
  char *result = malloc(length + segments);
  if(result == nullptr)
    return nullptr;
  size_t offset = 0;
  for(size_t i = 0; i < path->child_count; ++i)
  {
    if(path->children[i]->kind != XS_SYNTAX_IDENTIFIER)
      continue;
    if(offset != 0)
      result[offset++] = '.';
    memcpy(result + offset, path->children[i]->text.data, path->children[i]->text.length);
    offset += path->children[i]->text.length;
  }
  result[offset] = '\0';
  return result;
}

char *xs_hir_join_qualified(const char *namespace_name, const char *name)
{
  size_t namespace_length = strlen(namespace_name);
  size_t name_length = strlen(name);
  bool has_namespace = namespace_length != 0;
  char *result = malloc(namespace_length + (has_namespace ? 1 : 0) + name_length + 1);
  if(result == nullptr)
    return nullptr;
  size_t offset = 0;
  if(has_namespace)
  {
    memcpy(result, namespace_name, namespace_length + 1);
    offset = namespace_length;
    result[offset++] = '.';
  }
  memcpy(result + offset, name, name_length + 1);
  return result;
}

XsSpan xs_hir_node_span(const XsSyntaxNode *node)
{
  return (XsSpan){.start = node->span.start_offset, .end = node->span.end_offset};
}

bool xs_hir_symbol_is_public_child_of_namespace(const XsHirSymbol *symbol, const char *namespace_name)
{
  return strcmp(symbol->namespace_name, namespace_name) == 0 && symbol->visibility == XS_SYNTAX_VISIBILITY_PUBLIC;
}
