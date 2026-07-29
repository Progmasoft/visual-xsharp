/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "xs/macro.h"
#include "xs/syntax_parser.h"

#include <stdlib.h>
#include <string.h>

typedef struct
{
  XsSpan *items;
  size_t count;
  size_t capacity;
} MacroCallSpans;

static bool append_text(char *buffer, size_t *cursor, const char *text, size_t length)
{
  if(length == 0)
    return true;
  memcpy(buffer + *cursor, text, length);
  *cursor += length;
  return true;
}

static bool spans_equal(XsSpan left, XsSpan right)
{
  return left.start == right.start && left.end == right.end;
}

static bool span_list_add(MacroCallSpans *spans, XsSpan span)
{
  if(spans->count == spans->capacity)
  {
    size_t capacity = spans->capacity == 0 ? 4 : spans->capacity * 2;
    XsSpan *items = realloc(spans->items, capacity * sizeof(*items));
    if(items == nullptr)
      return false;
    spans->items = items;
    spans->capacity = capacity;
  }
  spans->items[spans->count++] = span;
  return true;
}

static bool span_list_contains(const MacroCallSpans *spans, XsSpan span)
{
  for(size_t i = 0; i < spans->count; ++i)
  {
    if(spans_equal(spans->items[i], span))
      return true;
  }
  return false;
}

static bool collect_statement_calls(const XsSyntaxNode *node, MacroCallSpans *spans)
{
  if(node == nullptr)
    return true;
  if(node->kind == XS_SYNTAX_STMT_MACRO_CALL)
  {
    const XsSyntaxNode *call = xs_syntax_find_first(node, XS_SYNTAX_EXPR_MACRO_CALL);
    if(call == nullptr)
      return true;
    XsSpan span = {.start = call->span.start_offset, .end = call->span.end_offset};
    return span_list_add(spans, span);
  }
  for(size_t i = 0; i < node->child_count; ++i)
  {
    if(!collect_statement_calls(node->children[i], spans))
      return false;
  }
  return true;
}

static bool collect_declaration_calls(const XsSyntaxNode *node, MacroCallSpans *spans)
{
  if(node == nullptr)
    return true;
  if(node->kind == XS_SYNTAX_DECL_MACRO_CALL)
  {
    const XsSyntaxNode *call = xs_syntax_find_first(node, XS_SYNTAX_EXPR_MACRO_CALL);
    if(call == nullptr)
      return true;
    XsSpan span = {.start = call->span.start_offset, .end = call->span.end_offset};
    return span_list_add(spans, span);
  }
  for(size_t i = 0; i < node->child_count; ++i)
  {
    if(!collect_declaration_calls(node->children[i], spans))
      return false;
  }
  return true;
}

static size_t expansion_text_length(const XsMacroExpansion *expansion)
{
  size_t length = 0;
  for(size_t i = 0; i < expansion->token_count; ++i)
    length += expansion->tokens[i].text.length + 1;
  return length;
}

static bool expansion_needs_semicolon(const XsMacroExpansion *expansion)
{
  if(expansion->token_count == 0)
    return false;
  XsTokenKind last = expansion->tokens[expansion->token_count - 1].kind;
  return last != XS_TOKEN_SEMICOLON && last != XS_TOKEN_RIGHT_BRACE;
}

static char *render_statement_source(const XsMacroExpansion *expansion, size_t *length)
{
  static const char prefix[] = "fn __xs_macro_expansion_probe() { ";
  static const char suffix[] = " }\n";
  bool needs_semicolon = expansion_needs_semicolon(expansion);
  size_t capacity = sizeof(prefix) - 1 + expansion_text_length(expansion) + (needs_semicolon ? 1 : 0) + sizeof(suffix);
  char *text = malloc(capacity);
  if(text == nullptr)
    return nullptr;

  size_t cursor = 0;
  append_text(text, &cursor, prefix, sizeof(prefix) - 1);
  for(size_t i = 0; i < expansion->token_count; ++i)
  {
    if(i != 0)
      append_text(text, &cursor, " ", 1);
    append_text(text, &cursor, expansion->tokens[i].text.data, expansion->tokens[i].text.length);
  }
  if(needs_semicolon)
    append_text(text, &cursor, ";", 1);
  append_text(text, &cursor, suffix, sizeof(suffix) - 1);
  text[cursor] = '\0';
  *length = cursor;
  return text;
}

static char *render_declaration_source(const XsMacroExpansion *expansion, size_t *length)
{
  static const char suffix[] = "\n";
  bool needs_semicolon = expansion_needs_semicolon(expansion);
  size_t capacity = expansion_text_length(expansion) + (needs_semicolon ? 1 : 0) + sizeof(suffix);
  char *text = malloc(capacity);
  if(text == nullptr)
    return nullptr;

  size_t cursor = 0;
  for(size_t i = 0; i < expansion->token_count; ++i)
  {
    if(i != 0)
      append_text(text, &cursor, " ", 1);
    append_text(text, &cursor, expansion->tokens[i].text.data, expansion->tokens[i].text.length);
  }
  if(needs_semicolon)
    append_text(text, &cursor, ";", 1);
  append_text(text, &cursor, suffix, sizeof(suffix) - 1);
  text[cursor] = '\0';
  *length = cursor;
  return text;
}

bool xs_macro_reparse_expansion_as_statement(const XsMacroExpansion *expansion, uint64_t file_id,
                                             XsDiagnostics *diagnostics, XsMacroReparseResult *result)
{
  if(expansion == nullptr || diagnostics == nullptr || result == nullptr)
    return false;
  if(expansion->token_count == 0)
  {
    xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, expansion->call_span,
                       "macro expansion did not produce a statement");
    return false;
  }
  *result = (XsMacroReparseResult){0};
  size_t length = 0;
  char *text = render_statement_source(expansion, &length);
  if(text == nullptr)
  {
    xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, expansion->call_span,
                       "compiler ran out of memory while reparsing macro expansion");
    return false;
  }
  result->text = text;
  result->source = (XsSource){.path = "<macro-expansion>", .text = text, .length = length};
  if(!xs_syntax_parse(&result->source, file_id, diagnostics, &result->tree))
  {
    xs_macro_reparse_result_free(result);
    return false;
  }
  return true;
}

bool xs_macro_reparse_expansion_as_declarations(const XsMacroExpansion *expansion, uint64_t file_id,
                                                XsDiagnostics *diagnostics, XsMacroReparseResult *result)
{
  if(expansion == nullptr || diagnostics == nullptr || result == nullptr)
    return false;
  if(expansion->token_count == 0)
  {
    xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, expansion->call_span,
                       "macro expansion did not produce a declaration");
    return false;
  }
  *result = (XsMacroReparseResult){0};
  size_t length = 0;
  char *text = render_declaration_source(expansion, &length);
  if(text == nullptr)
  {
    xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, expansion->call_span,
                       "compiler ran out of memory while reparsing macro declarations");
    return false;
  }
  result->text = text;
  result->source = (XsSource){.path = "<macro-declarations>", .text = text, .length = length};
  if(!xs_syntax_parse(&result->source, file_id, diagnostics, &result->tree))
  {
    xs_macro_reparse_result_free(result);
    return false;
  }
  return true;
}

const XsSyntaxNode *xs_macro_reparse_result_statement(const XsMacroReparseResult *result)
{
  const XsSyntaxNode *function =
      xs_syntax_find_first(result == nullptr ? nullptr : result->tree.root, XS_SYNTAX_DECL_FUNCTION);
  const XsSyntaxNode *block = xs_syntax_find_first(function, XS_SYNTAX_STMT_BLOCK);
  if(block == nullptr || block->child_count == 0)
    return nullptr;
  return block->children[0];
}

void xs_macro_reparse_result_free(XsMacroReparseResult *result)
{
  xs_syntax_tree_free(&result->tree);
  free(result->text);
  *result = (XsMacroReparseResult){0};
}

static bool statement_set_add(XsMacroStatementExpansionSet *set, XsMacroStatementExpansion item)
{
  if(set->count == set->capacity)
  {
    size_t capacity = set->capacity == 0 ? 4 : set->capacity * 2;
    XsMacroStatementExpansion *items = realloc(set->items, capacity * sizeof(*items));
    if(items == nullptr)
    {
      set->allocation_failed = true;
      return false;
    }
    set->items = items;
    set->capacity = capacity;
  }
  set->items[set->count++] = item;
  return true;
}

static bool declaration_set_add(XsMacroDeclarationExpansionSet *set, XsMacroDeclarationExpansion item)
{
  if(set->count == set->capacity)
  {
    size_t capacity = set->capacity == 0 ? 4 : set->capacity * 2;
    XsMacroDeclarationExpansion *items = realloc(set->items, capacity * sizeof(*items));
    if(items == nullptr)
    {
      set->allocation_failed = true;
      return false;
    }
    set->items = items;
    set->capacity = capacity;
  }
  set->items[set->count++] = item;
  return true;
}

void xs_macro_statement_expansion_set_free(XsMacroStatementExpansionSet *set)
{
  for(size_t i = 0; i < set->count; ++i)
    xs_macro_reparse_result_free(&set->items[i].reparse);
  free(set->items);
  *set = (XsMacroStatementExpansionSet){0};
}

void xs_macro_declaration_expansion_set_free(XsMacroDeclarationExpansionSet *set)
{
  for(size_t i = 0; i < set->count; ++i)
    xs_macro_reparse_result_free(&set->items[i].reparse);
  free(set->items);
  *set = (XsMacroDeclarationExpansionSet){0};
}

const XsSyntaxNode *xs_macro_statement_expansion_find(const XsMacroStatementExpansionSet *set,
                                                      const XsSyntaxNode *statement)
{
  if(set == nullptr || statement == nullptr || statement->kind != XS_SYNTAX_STMT_MACRO_CALL)
    return nullptr;
  const XsSyntaxNode *call = xs_syntax_find_first(statement, XS_SYNTAX_EXPR_MACRO_CALL);
  if(call == nullptr)
    return nullptr;
  for(size_t i = 0; i < set->count; ++i)
  {
    XsSpan span = set->items[i].call_span;
    if(span.start == call->span.start_offset && span.end == call->span.end_offset)
      return set->items[i].statement;
  }
  return nullptr;
}

bool xs_macro_statement_expansion_matches(const XsMacroStatementExpansion *expansion, const XsSyntaxNode *statement)
{
  if(expansion == nullptr || statement == nullptr || statement->kind != XS_SYNTAX_STMT_MACRO_CALL)
    return false;
  const XsSyntaxNode *call = xs_syntax_find_first(statement, XS_SYNTAX_EXPR_MACRO_CALL);
  if(call == nullptr)
    return false;
  return expansion->call_span.start == call->span.start_offset && expansion->call_span.end == call->span.end_offset;
}

const XsMacroDeclarationExpansion *xs_macro_declaration_expansion_find(const XsMacroDeclarationExpansionSet *set,
                                                                       const XsSyntaxNode *declaration)
{
  if(set == nullptr || declaration == nullptr || declaration->kind != XS_SYNTAX_DECL_MACRO_CALL)
    return nullptr;
  const XsSyntaxNode *call = xs_syntax_find_first(declaration, XS_SYNTAX_EXPR_MACRO_CALL);
  if(call == nullptr)
    return nullptr;
  for(size_t i = 0; i < set->count; ++i)
  {
    XsSpan span = set->items[i].call_span;
    if(span.start == call->span.start_offset && span.end == call->span.end_offset)
      return &set->items[i];
  }
  return nullptr;
}

bool xs_macro_expand_statements(const XsSyntaxTree *tree, XsDiagnostics *diagnostics, XsMacroStatementExpansionSet *set)
{
  if(tree == nullptr || tree->root == nullptr || diagnostics == nullptr || set == nullptr)
    return false;
  *set = (XsMacroStatementExpansionSet){0};

  MacroCallSpans statement_calls = {0};
  if(!collect_statement_calls(tree->root, &statement_calls))
  {
    xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, (XsSpan){0, 0},
                       "compiler ran out of memory while collecting macro statement calls");
    return false;
  }

  XsMacroExpansionSet tokens = {0};
  if(!xs_macro_expand_tokens(tree, diagnostics, &tokens))
  {
    free(statement_calls.items);
    xs_macro_expansion_set_free(&tokens);
    return false;
  }

  for(size_t i = 0; i < tokens.count; ++i)
  {
    if(!span_list_contains(&statement_calls, tokens.items[i].call_span))
      continue;
    XsMacroReparseResult reparse;
    if(!xs_macro_reparse_expansion_as_statement(&tokens.items[i], tree->file_id, diagnostics, &reparse))
    {
      free(statement_calls.items);
      xs_macro_statement_expansion_set_free(set);
      xs_macro_expansion_set_free(&tokens);
      return false;
    }
    const XsSyntaxNode *statement = xs_macro_reparse_result_statement(&reparse);
    if(statement == nullptr)
    {
      xs_macro_reparse_result_free(&reparse);
      xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, tokens.items[i].call_span,
                         "macro expansion did not produce a statement");
      free(statement_calls.items);
      xs_macro_statement_expansion_set_free(set);
      xs_macro_expansion_set_free(&tokens);
      return false;
    }
    XsMacroStatementExpansion item = {
        .call_span = tokens.items[i].call_span,
        .reparse = reparse,
        .statement = statement,
    };
    if(!statement_set_add(set, item))
    {
      xs_macro_reparse_result_free(&reparse);
      free(statement_calls.items);
      xs_macro_statement_expansion_set_free(set);
      xs_macro_expansion_set_free(&tokens);
      xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, tokens.items[i].call_span,
                         "compiler ran out of memory while recording macro statement expansion");
      return false;
    }
  }

  free(statement_calls.items);
  xs_macro_expansion_set_free(&tokens);
  return !set->allocation_failed && !xs_diagnostics_has_error(diagnostics);
}

bool xs_macro_expand_declarations(const XsSyntaxTree *tree, XsDiagnostics *diagnostics,
                                  XsMacroDeclarationExpansionSet *set)
{
  if(tree == nullptr || tree->root == nullptr || diagnostics == nullptr || set == nullptr)
    return false;
  *set = (XsMacroDeclarationExpansionSet){0};

  MacroCallSpans declaration_calls = {0};
  if(!collect_declaration_calls(tree->root, &declaration_calls))
  {
    xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, (XsSpan){0, 0},
                       "compiler ran out of memory while collecting macro declaration calls");
    return false;
  }

  XsMacroExpansionSet tokens = {0};
  if(!xs_macro_expand_tokens(tree, diagnostics, &tokens))
  {
    free(declaration_calls.items);
    xs_macro_expansion_set_free(&tokens);
    return false;
  }

  for(size_t i = 0; i < tokens.count; ++i)
  {
    if(!span_list_contains(&declaration_calls, tokens.items[i].call_span))
      continue;
    XsMacroReparseResult reparse;
    if(!xs_macro_reparse_expansion_as_declarations(&tokens.items[i], tree->file_id, diagnostics, &reparse))
    {
      free(declaration_calls.items);
      xs_macro_declaration_expansion_set_free(set);
      xs_macro_expansion_set_free(&tokens);
      return false;
    }
    size_t declaration_count = reparse.tree.root == nullptr ? 0 : reparse.tree.root->child_count;
    if(declaration_count == 0)
    {
      xs_macro_reparse_result_free(&reparse);
      xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, tokens.items[i].call_span,
                         "macro expansion did not produce a declaration");
      free(declaration_calls.items);
      xs_macro_declaration_expansion_set_free(set);
      xs_macro_expansion_set_free(&tokens);
      return false;
    }
    XsMacroDeclarationExpansion item = {
        .call_span = tokens.items[i].call_span,
        .reparse = reparse,
        .declaration_count = declaration_count,
    };
    if(!declaration_set_add(set, item))
    {
      xs_macro_reparse_result_free(&reparse);
      free(declaration_calls.items);
      xs_macro_declaration_expansion_set_free(set);
      xs_macro_expansion_set_free(&tokens);
      xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, tokens.items[i].call_span,
                         "compiler ran out of memory while recording macro declaration expansion");
      return false;
    }
  }

  free(declaration_calls.items);
  xs_macro_expansion_set_free(&tokens);
  return !set->allocation_failed && !xs_diagnostics_has_error(diagnostics);
}
