/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef XS_SYNTAX_PARSER_INTERNAL_H
#define XS_SYNTAX_PARSER_INTERNAL_H

#include "xs/diagnostic.h"
#include "xs/lexer.h"
#include "xs/syntax_ast.h"

#include <stddef.h>
#include <stdint.h>

typedef struct
{
  XsLexer lexer;
  XsDiagnostics *diagnostics;
  XsSyntaxTree *tree;
  XsToken previous;
  XsToken current;
  XsToken next;
  size_t loop_depth;
  bool allow_untyped_new;
  bool suppress_typed_object_literal;
  bool has_source_namespace;
  bool seen_non_namespace;
} SyntaxParser;

typedef struct
{
  XsSyntaxVisibility visibility;
  uint32_t flags;
  XsSpan span;
  XsSyntaxNode *attributes;
} Modifiers;

void xs_syntax_parser_advance(SyntaxParser *parser);
bool xs_syntax_parser_accept(SyntaxParser *parser, XsTokenKind kind);
bool xs_syntax_parser_expect(SyntaxParser *parser, XsTokenKind kind, const char *message);
XsSyntaxNode *xs_syntax_parser_node(SyntaxParser *parser, XsSyntaxKind kind, XsSpan span);
void xs_syntax_parser_finish_node(SyntaxParser *parser, XsSyntaxNode *value, size_t end);
XsSyntaxNode *xs_syntax_parser_identifier(SyntaxParser *parser);
bool xs_syntax_parser_token_text_is(SyntaxParser *parser, XsToken token, const char *text);
XsSyntaxNode *xs_syntax_parse_path(SyntaxParser *parser);
XsSyntaxNode *xs_syntax_parse_inner_attribute(SyntaxParser *parser);
Modifiers xs_syntax_parse_modifiers(SyntaxParser *parser);
void xs_syntax_attach_modifiers(SyntaxParser *parser, XsSyntaxNode *declaration, Modifiers modifiers);
XsSyntaxNode *xs_syntax_parse_type(SyntaxParser *parser);
XsSyntaxNode *xs_syntax_parse_pattern(SyntaxParser *parser);
XsSyntaxNode *xs_syntax_parse_expression(SyntaxParser *parser, unsigned minimum_precedence);
XsSyntaxNode *xs_syntax_parse_statement(SyntaxParser *parser);
XsSyntaxNode *xs_syntax_parse_block(SyntaxParser *parser);
XsSyntaxNode *xs_syntax_parse_variable(SyntaxParser *parser, bool require_semicolon);
XsSyntaxNode *xs_syntax_parse_declaration(SyntaxParser *parser, bool top_level);
XsSyntaxNode *xs_syntax_parse_macro(SyntaxParser *parser, size_t start);

#define advance xs_syntax_parser_advance
#define accept xs_syntax_parser_accept
#define expect xs_syntax_parser_expect
#define node xs_syntax_parser_node
#define finish_node xs_syntax_parser_finish_node
#define identifier xs_syntax_parser_identifier
#define token_text_is xs_syntax_parser_token_text_is
#define parse_path xs_syntax_parse_path
#define parse_inner_attribute xs_syntax_parse_inner_attribute
#define parse_modifiers xs_syntax_parse_modifiers
#define attach_modifiers xs_syntax_attach_modifiers
#define parse_type xs_syntax_parse_type
#define parse_pattern xs_syntax_parse_pattern
#define parse_expression xs_syntax_parse_expression
#define parse_statement xs_syntax_parse_statement
#define parse_block xs_syntax_parse_block
#define parse_variable xs_syntax_parse_variable
#define parse_declaration xs_syntax_parse_declaration
#define parse_macro xs_syntax_parse_macro

#endif
