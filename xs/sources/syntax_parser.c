/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "xs/syntax_parser.h"

#include "syntax/parser_internal.h"

#include <stddef.h>
#include <string.h>

static XsToken read_token(SyntaxParser *parser)
{
  XsToken token;
  do
  {
    token = xs_lexer_next(&parser->lexer);
  } while(token.kind == XS_TOKEN_DOC_COMMENT || token.kind == XS_TOKEN_MODULE_COMMENT);
  return token;
}

void advance(SyntaxParser *parser)
{
  parser->previous = parser->current;
  parser->current = parser->next;
  parser->next = read_token(parser);
}

bool accept(SyntaxParser *parser, XsTokenKind kind)
{
  if(kind == XS_TOKEN_GREATER && parser->current.kind == XS_TOKEN_SHIFT_RIGHT)
  {
    parser->previous =
        (XsToken){.kind = XS_TOKEN_GREATER, .span = {parser->current.span.start, parser->current.span.start + 1}};
    parser->current =
        (XsToken){.kind = XS_TOKEN_GREATER, .span = {parser->current.span.start + 1, parser->current.span.end}};
    return true;
  }
  if(parser->current.kind != kind)
    return false;
  advance(parser);
  return true;
}

bool expect(SyntaxParser *parser, XsTokenKind kind, const char *message)
{
  if(accept(parser, kind))
    return true;
  xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR, parser->current.span, message);
  return false;
}

XsSyntaxNode *node(SyntaxParser *parser, XsSyntaxKind kind, XsSpan span)
{
  return xs_syntax_node_new(parser->tree, kind, span);
}

void finish_node(SyntaxParser *parser, XsSyntaxNode *value, size_t end)
{
  if(value == nullptr)
    return;
  XsSpan span = {.start = value->span.start_offset, .end = end};
  value->span = xs_source_span(parser->tree, span);
  value->text = xs_source_text(parser->tree->source, span);
}

XsSyntaxNode *identifier(SyntaxParser *parser)
{
  if(!expect(parser, XS_TOKEN_IDENTIFIER, "expected identifier"))
    return nullptr;
  return node(parser, XS_SYNTAX_IDENTIFIER, parser->previous.span);
}

XsSyntaxNode *parse_path(SyntaxParser *parser)
{
  size_t start = parser->current.span.start;
  XsSyntaxNode *path = node(parser, XS_SYNTAX_PATH, (XsSpan){start, start});
  XsSyntaxNode *segment = nullptr;
  if(parser->current.kind == XS_TOKEN_IDENTIFIER || parser->current.kind == XS_TOKEN_KW_ATOMIC)
  {
    XsToken token = parser->current;
    advance(parser);
    segment = node(parser, XS_SYNTAX_IDENTIFIER, token.span);
  }
  else
  {
    xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR, parser->current.span, "expected identifier");
  }
  if(segment == nullptr)
    return path;
  xs_syntax_node_add(parser->tree, path, segment);
  while(parser->current.kind == XS_TOKEN_DOUBLE_COLON &&
        (parser->next.kind == XS_TOKEN_IDENTIFIER || parser->next.kind == XS_TOKEN_KW_ATOMIC))
  {
    advance(parser);
    XsToken token = parser->current;
    advance(parser);
    segment = node(parser, XS_SYNTAX_IDENTIFIER, token.span);
    xs_syntax_node_add(parser->tree, path, segment);
  }
  finish_node(parser, path, parser->previous.span.end);
  return path;
}

static XsSyntaxNode *parse_attribute(SyntaxParser *parser, bool inner)
{
  size_t start = parser->current.span.start;
  expect(parser, XS_TOKEN_HASH, "expected '#'");
  if(inner)
    expect(parser, XS_TOKEN_BANG, "expected '!' in inner attribute");
  expect(parser, XS_TOKEN_LEFT_BRACKET, "expected '[' before attribute");
  XsSyntaxNode *attribute = node(parser, XS_SYNTAX_ATTRIBUTE, (XsSpan){start, start});
  if(inner)
    attribute->flags |= XS_SYNTAX_FLAG_INNER_ATTRIBUTE;
  xs_syntax_node_add(parser->tree, attribute, parse_path(parser));
  if(accept(parser, XS_TOKEN_ASSIGN))
  {
    xs_syntax_node_add(parser->tree, attribute, parse_expression(parser, 1));
  }
  else if(accept(parser, XS_TOKEN_LEFT_PAREN))
  {
    if(parser->current.kind != XS_TOKEN_RIGHT_PAREN)
    {
      do
      {
        if(parser->current.kind == XS_TOKEN_RIGHT_PAREN)
          break;
        xs_syntax_node_add(parser->tree, attribute, parse_expression(parser, 1));
      } while(accept(parser, XS_TOKEN_COMMA));
    }
    expect(parser, XS_TOKEN_RIGHT_PAREN, "expected ')' after attribute arguments");
  }
  expect(parser, XS_TOKEN_RIGHT_BRACKET, "expected ']' after attribute");
  finish_node(parser, attribute, parser->previous.span.end);
  return attribute;
}

static XsSyntaxNode *parse_attribute_list(SyntaxParser *parser)
{
  if(parser->current.kind != XS_TOKEN_HASH || parser->next.kind != XS_TOKEN_LEFT_BRACKET)
    return nullptr;
  size_t start = parser->current.span.start;
  XsSyntaxNode *attributes = node(parser, XS_SYNTAX_ATTRIBUTE_LIST, (XsSpan){start, start});
  while(parser->current.kind == XS_TOKEN_HASH && parser->next.kind == XS_TOKEN_LEFT_BRACKET)
    xs_syntax_node_add(parser->tree, attributes, parse_attribute(parser, false));
  finish_node(parser, attributes, parser->previous.span.end);
  return attributes;
}

XsSyntaxNode *parse_inner_attribute(SyntaxParser *parser)
{
  return parse_attribute(parser, true);
}

Modifiers parse_modifiers(SyntaxParser *parser)
{
  Modifiers result = {.visibility = XS_SYNTAX_VISIBILITY_INTERNAL,
                      .span = {parser->current.span.start, parser->current.span.start}};
  bool progress = true;
  while(progress)
  {
    progress = false;
    XsToken token = parser->current;
    XsSyntaxNode *attributes = parse_attribute_list(parser);
    if(attributes != nullptr)
    {
      if(result.attributes == nullptr)
        result.attributes = attributes;
      else
      {
        for(size_t i = 0; i < attributes->child_count; ++i)
          xs_syntax_node_add(parser->tree, result.attributes, attributes->children[i]);
        finish_node(parser, result.attributes, parser->previous.span.end);
      }
      result.span.end = parser->previous.span.end;
      progress = true;
    }
    else if(accept(parser, XS_TOKEN_KW_PUBLIC))
    {
      result.visibility = XS_SYNTAX_VISIBILITY_PUBLIC;
      progress = true;
    }
    else if(accept(parser, XS_TOKEN_KW_PRIVATE))
    {
      result.visibility = XS_SYNTAX_VISIBILITY_PRIVATE;
      progress = true;
    }
    else if(accept(parser, XS_TOKEN_KW_PROTECTED))
    {
      result.visibility = XS_SYNTAX_VISIBILITY_PROTECTED;
      progress = true;
    }
    else if(accept(parser, XS_TOKEN_KW_INTERNAL))
    {
      result.visibility = XS_SYNTAX_VISIBILITY_INTERNAL;
      progress = true;
    }
    else if(accept(parser, XS_TOKEN_KW_ASYNC))
    {
      result.flags |= XS_SYNTAX_FLAG_ASYNC;
      progress = true;
    }
    else if(accept(parser, XS_TOKEN_KW_STATIC))
    {
      result.flags |= XS_SYNTAX_FLAG_STATIC;
      progress = true;
    }
    else if(accept(parser, XS_TOKEN_KW_INCOMPLETE))
    {
      result.flags |= XS_SYNTAX_FLAG_INCOMPLETE;
      progress = true;
    }
    else if(accept(parser, XS_TOKEN_KW_VIRTUAL))
    {
      result.flags |= XS_SYNTAX_FLAG_VIRTUAL;
      progress = true;
    }
    else if(accept(parser, XS_TOKEN_KW_OVERRIDE))
    {
      result.flags |= XS_SYNTAX_FLAG_OVERRIDE;
      progress = true;
    }
    else if(accept(parser, XS_TOKEN_KW_SEALED))
    {
      result.flags |= XS_SYNTAX_FLAG_SEALED;
      progress = true;
    }
    if(progress)
      result.span.end = token.span.end;
  }
  return result;
}

void attach_modifiers(SyntaxParser *parser, XsSyntaxNode *declaration, Modifiers modifiers)
{
  if(declaration == nullptr)
    return;
  declaration->visibility = modifiers.visibility;
  declaration->flags |= modifiers.flags;
  xs_syntax_node_add(parser->tree, declaration, modifiers.attributes);
  XsSyntaxNode *visibility = node(parser, XS_SYNTAX_VISIBILITY, modifiers.span);
  if(visibility != nullptr)
    visibility->visibility = modifiers.visibility;
  xs_syntax_node_add(parser->tree, declaration, visibility);
}

bool token_text_is(SyntaxParser *parser, XsToken token, const char *text)
{
  size_t length = token.span.end - token.span.start;
  return strlen(text) == length && memcmp(parser->tree->source->text + token.span.start, text, length) == 0;
}

bool xs_syntax_parse(const XsSource *source, uint64_t file_id, XsDiagnostics *diagnostics, XsSyntaxTree *tree)
{
  xs_syntax_tree_init(tree, source, file_id);
  SyntaxParser parser = {.diagnostics = diagnostics, .tree = tree};
  xs_lexer_init(&parser.lexer, source, diagnostics);
  parser.current = read_token(&parser);
  parser.next = read_token(&parser);
  tree->root = node(&parser, XS_SYNTAX_FILE, (XsSpan){0, source->length});

  bool seen_declaration = false;
  while(parser.current.kind != XS_TOKEN_EOF)
  {
    if(parser.current.kind == XS_TOKEN_HASH && parser.next.kind == XS_TOKEN_BANG)
    {
      XsSyntaxNode *attribute = parse_inner_attribute(&parser);
      if(seen_declaration)
        xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR,
                           (XsSpan){attribute->span.start_offset, attribute->span.end_offset},
                           "inner file attributes must appear before declarations");
      xs_syntax_node_add(tree, tree->root, attribute);
      continue;
    }
    size_t before = parser.current.span.start;
    XsSyntaxNode *declaration = parse_declaration(&parser, true);
    if(declaration != nullptr)
    {
      xs_syntax_node_add(tree, tree->root, declaration);
      seen_declaration = true;
      if(declaration->kind != XS_SYNTAX_DECL_NAMESPACE)
        parser.seen_non_namespace = true;
    }
    if(parser.current.span.start == before)
    {
      xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, parser.current.span, "parser made no progress at top level");
      advance(&parser);
    }
  }
  if(tree->allocation_failed)
    xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, (XsSpan){0, 0},
                       "compiler ran out of memory while building structural AST");
  return !xs_diagnostics_has_error(diagnostics);
}
