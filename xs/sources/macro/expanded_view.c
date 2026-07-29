/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "xs/macro.h"

#include <stdlib.h>

static bool expanded_set_add(XsMacroExpandedDeclarationSet *set, XsMacroExpandedDeclaration item)
{
  if(set->count == set->capacity)
  {
    size_t capacity = set->capacity == 0 ? 8 : set->capacity * 2;
    XsMacroExpandedDeclaration *items = realloc(set->items, capacity * sizeof(*items));
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

static bool statement_set_add(XsMacroExpandedStatementSet *set, XsMacroExpandedStatement item)
{
  if(set->count == set->capacity)
  {
    size_t capacity = set->capacity == 0 ? 8 : set->capacity * 2;
    XsMacroExpandedStatement *items = realloc(set->items, capacity * sizeof(*items));
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

static bool add_original_declaration(XsMacroExpandedDeclarationSet *expanded, const XsSyntaxNode *declaration)
{
  XsMacroExpandedDeclaration item = {
      .declaration = declaration,
      .call_span = {.start = declaration->span.start_offset, .end = declaration->span.end_offset},
  };
  return expanded_set_add(expanded, item);
}

static bool add_original_statement(XsMacroExpandedStatementSet *expanded, const XsSyntaxNode *statement)
{
  XsMacroExpandedStatement item = {
      .statement = statement,
      .call_span = {.start = statement->span.start_offset, .end = statement->span.end_offset},
  };
  return statement_set_add(expanded, item);
}

static bool add_macro_declarations(XsMacroExpandedDeclarationSet *expanded,
                                   const XsMacroDeclarationExpansion *expansion)
{
  if(expansion->reparse.tree.root == nullptr)
    return true;
  for(size_t i = 0; i < expansion->reparse.tree.root->child_count; ++i)
  {
    XsMacroExpandedDeclaration item = {
        .declaration = expansion->reparse.tree.root->children[i],
        .call_span = expansion->call_span,
        .from_macro_expansion = true,
    };
    if(!expanded_set_add(expanded, item))
      return false;
  }
  return true;
}

static bool add_macro_statement(XsMacroExpandedStatementSet *expanded, const XsMacroStatementExpansion *expansion)
{
  if(expansion->statement == nullptr)
    return true;
  XsMacroExpandedStatement item = {
      .statement = expansion->statement,
      .call_span = expansion->call_span,
      .from_macro_expansion = true,
  };
  return statement_set_add(expanded, item);
}

static bool expansion_matches_call(const XsMacroDeclarationExpansion *expansion, const XsSyntaxNode *macro_call)
{
  const XsSyntaxNode *call = xs_syntax_find_first(macro_call, XS_SYNTAX_EXPR_MACRO_CALL);
  if(call == nullptr)
    return false;
  return expansion->call_span.start == call->span.start_offset && expansion->call_span.end == call->span.end_offset;
}

static bool statement_expansion_matches_call(const XsMacroStatementExpansion *expansion, const XsSyntaxNode *macro_call)
{
  const XsSyntaxNode *call = xs_syntax_find_first(macro_call, XS_SYNTAX_EXPR_MACRO_CALL);
  if(call == nullptr)
    return false;
  return expansion->call_span.start == call->span.start_offset && expansion->call_span.end == call->span.end_offset;
}

static bool add_expanded_macro_call(XsMacroExpandedDeclarationSet *expanded,
                                    const XsMacroDeclarationExpansionSet *declarations, const XsSyntaxNode *macro_call)
{
  bool matched = false;
  for(size_t i = 0; declarations != nullptr && i < declarations->count; ++i)
  {
    const XsMacroDeclarationExpansion *expansion = &declarations->items[i];
    if(!expansion_matches_call(expansion, macro_call))
      continue;
    matched = true;
    if(!add_macro_declarations(expanded, expansion))
      return false;
  }
  return matched || add_original_declaration(expanded, macro_call);
}

static bool add_expanded_statement_macro_call(XsMacroExpandedStatementSet *expanded,
                                              const XsMacroStatementExpansionSet *statements,
                                              const XsSyntaxNode *macro_call)
{
  bool matched = false;
  for(size_t i = 0; statements != nullptr && i < statements->count; ++i)
  {
    const XsMacroStatementExpansion *expansion = &statements->items[i];
    if(!statement_expansion_matches_call(expansion, macro_call))
      continue;
    matched = true;
    if(!add_macro_statement(expanded, expansion))
      return false;
  }
  return matched || add_original_statement(expanded, macro_call);
}

bool xs_macro_expand_top_level_declarations(const XsSyntaxTree *tree,
                                            const XsMacroDeclarationExpansionSet *declarations,
                                            XsDiagnostics *diagnostics, XsMacroExpandedDeclarationSet *expanded)
{
  if(tree == nullptr || tree->root == nullptr || diagnostics == nullptr || expanded == nullptr)
    return false;
  return xs_macro_expand_child_declarations(tree->root, declarations, diagnostics, expanded);
}

bool xs_macro_expand_child_declarations(const XsSyntaxNode *parent, const XsMacroDeclarationExpansionSet *declarations,
                                        XsDiagnostics *diagnostics, XsMacroExpandedDeclarationSet *expanded)
{
  if(parent == nullptr || diagnostics == nullptr || expanded == nullptr)
    return false;
  *expanded = (XsMacroExpandedDeclarationSet){0};
  for(size_t i = 0; i < parent->child_count; ++i)
  {
    const XsSyntaxNode *child = parent->children[i];
    bool success = child->kind == XS_SYNTAX_DECL_MACRO_CALL ? add_expanded_macro_call(expanded, declarations, child)
                                                            : add_original_declaration(expanded, child);
    if(!success)
    {
      xs_macro_expanded_declaration_set_free(expanded);
      xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, (XsSpan){0, 0},
                         "compiler ran out of memory while building expanded declaration view");
      return false;
    }
  }
  return !expanded->allocation_failed && !xs_diagnostics_has_error(diagnostics);
}

bool xs_macro_expand_child_statements(const XsSyntaxNode *parent, const XsMacroStatementExpansionSet *statements,
                                      XsDiagnostics *diagnostics, XsMacroExpandedStatementSet *expanded)
{
  if(parent == nullptr || diagnostics == nullptr || expanded == nullptr)
    return false;
  *expanded = (XsMacroExpandedStatementSet){0};
  for(size_t i = 0; i < parent->child_count; ++i)
  {
    const XsSyntaxNode *child = parent->children[i];
    bool success = child->kind == XS_SYNTAX_STMT_MACRO_CALL
                       ? add_expanded_statement_macro_call(expanded, statements, child)
                       : add_original_statement(expanded, child);
    if(!success)
    {
      xs_macro_expanded_statement_set_free(expanded);
      xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, (XsSpan){0, 0},
                         "compiler ran out of memory while building expanded statement view");
      return false;
    }
  }
  return !expanded->allocation_failed && !xs_diagnostics_has_error(diagnostics);
}

void xs_macro_expanded_declaration_set_free(XsMacroExpandedDeclarationSet *set)
{
  free(set->items);
  *set = (XsMacroExpandedDeclarationSet){0};
}

void xs_macro_expanded_statement_set_free(XsMacroExpandedStatementSet *set)
{
  free(set->items);
  *set = (XsMacroExpandedStatementSet){0};
}
