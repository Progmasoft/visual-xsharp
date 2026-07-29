/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "xs/hir/inheritance.h"

#include "syntax_helpers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
  bool found;
  bool sealed;
} OverrideMatch;

static bool text_equal(XsText left, XsText right)
{
  return left.length == right.length && memcmp(left.data, right.data, left.length) == 0;
}

static bool syntax_equal(const XsSyntaxNode *left, const XsSyntaxNode *right)
{
  if(left == right)
    return true;
  if(left == nullptr || right == nullptr || left->kind != right->kind || left->token_kind != right->token_kind ||
     left->child_count != right->child_count)
    return false;
  if(left->child_count == 0 && !text_equal(left->text, right->text))
    return false;
  for(size_t index = 0; index < left->child_count; ++index)
  {
    if(!syntax_equal(left->children[index], right->children[index]))
      return false;
  }
  return true;
}

static const XsSyntaxNode *base_named_type(const XsSyntaxNode *type)
{
  if(type == nullptr)
    return nullptr;
  if(type->kind == XS_SYNTAX_BASE_SPECIFIER)
    type = type->child_count == 0 ? nullptr : type->children[0];
  if(type == nullptr)
    return nullptr;
  if(type->kind == XS_SYNTAX_TYPE_NAMED)
    return type;
  if(type->kind == XS_SYNTAX_TYPE_GENERIC)
    return xs_hir_first_child_kind(type, XS_SYNTAX_TYPE_NAMED);
  return nullptr;
}

static bool is_base_type_node(const XsSyntaxNode *node)
{
  return node != nullptr && node->kind == XS_SYNTAX_BASE_SPECIFIER;
}

static bool is_data_enum(const XsHirSymbol *symbol)
{
  return symbol != nullptr && symbol->kind == XS_HIR_SYMBOL_ENUM &&
         (symbol->syntax->flags & XS_SYNTAX_FLAG_DATA_ENUM) != 0;
}

static bool standard_enum_data_family(const XsSyntaxNode *type)
{
  const XsSyntaxNode *named = base_named_type(type);
  const XsSyntaxNode *path = xs_hir_first_child_kind(named, XS_SYNTAX_PATH);
  char *name = xs_hir_path_to_string(path);
  if(name == nullptr)
    return false;
  bool matches = strcmp(name, "Optional") == 0 || strcmp(name, "std.optional.Optional") == 0 ||
                 strcmp(name, "Result") == 0 || strcmp(name, "std.result.Result") == 0;
  free(name);
  return matches;
}

static bool supports_base_list(const XsHirSymbol *symbol)
{
  return symbol != nullptr && (symbol->kind == XS_HIR_SYMBOL_CLASS || symbol->kind == XS_HIR_SYMBOL_INTERFACE ||
                               symbol->kind == XS_HIR_SYMBOL_DATA || is_data_enum(symbol));
}

static const XsHirSymbol *resolve_base(const XsSyntaxNode *type, const char *namespace_name,
                                       const XsHirSymbolTable *symbols, const XsHirImportScope *import)
{
  const XsSyntaxNode *named = base_named_type(type);
  char *path = xs_hir_path_to_string(xs_hir_first_child_kind(named, XS_SYNTAX_PATH));
  if(path == nullptr)
    return nullptr;
  const XsHirImportBinding *binding = xs_hir_import_scope_find(import, path);
  const XsHirSymbol *symbol = binding == nullptr ? nullptr : binding->symbol;
  if(symbol == nullptr && strchr(path, '.') != nullptr)
    symbol = xs_hir_symbol_table_find(symbols, path);
  if(symbol == nullptr)
  {
    char *qualified = xs_hir_join_qualified(namespace_name, path);
    if(qualified != nullptr)
    {
      symbol = xs_hir_symbol_table_find(symbols, qualified);
      free(qualified);
    }
  }
  free(path);
  return symbol;
}

static bool method_signature_equal(const XsSyntaxNode *left, const XsSyntaxNode *right)
{
  const XsSyntaxNode *left_name = xs_hir_first_child_kind(left, XS_SYNTAX_IDENTIFIER);
  const XsSyntaxNode *right_name = xs_hir_first_child_kind(right, XS_SYNTAX_IDENTIFIER);
  if(left_name == nullptr || right_name == nullptr || !text_equal(left_name->text, right_name->text))
    return false;
  size_t left_parameter = 0;
  size_t right_parameter = 0;
  while(true)
  {
    while(left_parameter < left->child_count && left->children[left_parameter]->kind != XS_SYNTAX_PARAMETER)
      ++left_parameter;
    while(right_parameter < right->child_count && right->children[right_parameter]->kind != XS_SYNTAX_PARAMETER)
      ++right_parameter;
    bool left_done = left_parameter == left->child_count;
    bool right_done = right_parameter == right->child_count;
    if(left_done || right_done)
      return left_done && right_done;
    const XsSyntaxNode *left_type = left->children[left_parameter]->children[1];
    const XsSyntaxNode *right_type = right->children[right_parameter]->children[1];
    if(!syntax_equal(left_type, right_type))
      return false;
    ++left_parameter;
    ++right_parameter;
  }
}

static void find_override_match(const XsHirSymbol *base, const XsSyntaxNode *method, const XsHirSymbolTable *symbols,
                                const XsHirImportScope *import, size_t depth, OverrideMatch *match)
{
  if(base == nullptr || base->kind != XS_HIR_SYMBOL_CLASS || depth > symbols->count)
    return;
  for(size_t index = 0; index < base->syntax->child_count; ++index)
  {
    const XsSyntaxNode *member = base->syntax->children[index];
    if(member->kind != XS_SYNTAX_DECL_FUNCTION || !method_signature_equal(member, method))
      continue;
    if((member->flags & (XS_SYNTAX_FLAG_VIRTUAL | XS_SYNTAX_FLAG_OVERRIDE)) != 0)
    {
      match->found = true;
      match->sealed = match->sealed || (member->flags & XS_SYNTAX_FLAG_SEALED) != 0;
    }
  }
  for(size_t index = 0; index < base->syntax->child_count; ++index)
  {
    const XsSyntaxNode *type = base->syntax->children[index];
    if(!is_base_type_node(type))
      continue;
    const XsHirSymbol *next = resolve_base(type, base->namespace_name, symbols, import);
    find_override_match(next, method, symbols, import, depth + 1, match);
  }
}

static bool inherits_from(const XsHirSymbol *symbol, const XsHirSymbol *target, const XsHirSymbolTable *symbols,
                          const XsHirImportScope *import, size_t depth)
{
  if(symbol == nullptr || depth > symbols->count)
    return false;
  if(symbol == target)
    return true;
  for(size_t index = 0; index < symbol->syntax->child_count; ++index)
  {
    const XsSyntaxNode *type = symbol->syntax->children[index];
    if(!is_base_type_node(type))
      continue;
    const XsHirSymbol *base = resolve_base(type, symbol->namespace_name, symbols, import);
    if(inherits_from(base, target, symbols, import, depth + 1))
      return true;
  }
  return false;
}

static bool report(XsDiagnostics *diagnostics, const XsSyntaxNode *node, const char *message)
{
  return xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, xs_hir_node_span(node), message);
}

static bool validate_base(const XsHirSymbol *owner, const XsSyntaxNode *type, const XsHirSymbolTable *symbols,
                          const XsHirImportScope *import, XsDiagnostics *diagnostics)
{
  const XsHirSymbol *base = resolve_base(type, owner->namespace_name, symbols, import);
  if(base == nullptr)
  {
    if(!standard_enum_data_family(type))
      return true;
    if(is_data_enum(owner))
      return true;
    return report(diagnostics, type, "only enum data types may inherit standard enum data families");
  }
  bool success = true;
  if(!supports_base_list(base))
    success = report(diagnostics, type, "base-list entry does not resolve to an inheritable type") && success;
  if(owner->kind == XS_HIR_SYMBOL_CLASS && base->kind != XS_HIR_SYMBOL_CLASS && base->kind != XS_HIR_SYMBOL_INTERFACE)
    success = report(diagnostics, type, "class base entries must be classes or implemented interfaces") && success;
  if(owner->kind == XS_HIR_SYMBOL_INTERFACE && base->kind != XS_HIR_SYMBOL_INTERFACE)
    success = report(diagnostics, type, "interfaces may inherit only from interfaces") && success;
  if(owner->kind == XS_HIR_SYMBOL_DATA && base->kind != XS_HIR_SYMBOL_DATA)
    success = report(diagnostics, type, "data types may inherit only from data types") && success;
  if(is_data_enum(owner) && !is_data_enum(base))
    success = report(diagnostics, type, "enum data types may inherit only from enum data types") && success;
  if((base->syntax->flags & XS_SYNTAX_FLAG_SEALED) != 0)
    success = report(diagnostics, type, "sealed classes cannot be inherited") && success;
  if(inherits_from(base, owner, symbols, import, 0))
    success = report(diagnostics, type, "inheritance cycle detected") && success;
  for(size_t index = 0; index < owner->syntax->child_count; ++index)
  {
    const XsSyntaxNode *previous = owner->syntax->children[index];
    if(previous == type)
      break;
    if(is_base_type_node(previous) && resolve_base(previous, owner->namespace_name, symbols, import) == base)
    {
      success = report(diagnostics, type, "duplicate type in inheritance base list") && success;
      break;
    }
  }
  return success;
}

static bool validate_overrides(const XsHirSymbol *owner, const XsHirSymbolTable *symbols,
                               const XsHirImportScope *import, XsDiagnostics *diagnostics)
{
  bool success = true;
  for(size_t method_index = 0; method_index < owner->syntax->child_count; ++method_index)
  {
    const XsSyntaxNode *method = owner->syntax->children[method_index];
    if(method->kind != XS_SYNTAX_DECL_FUNCTION || (method->flags & XS_SYNTAX_FLAG_OVERRIDE) == 0)
      continue;
    OverrideMatch match = {0};
    for(size_t base_index = 0; base_index < owner->syntax->child_count; ++base_index)
    {
      const XsSyntaxNode *type = owner->syntax->children[base_index];
      if(!is_base_type_node(type))
        continue;
      find_override_match(resolve_base(type, owner->namespace_name, symbols, import), method, symbols, import, 0,
                          &match);
    }
    if(!match.found)
      success = report(diagnostics, method, "override method has no compatible inherited virtual method") && success;
    else if(match.sealed)
      success = report(diagnostics, method, "sealed inherited method cannot be overridden") && success;
  }
  return success;
}

bool xs_hir_validate_inheritance(const XsSyntaxTree *tree, const XsHirSymbolTable *symbols,
                                 const XsHirImportScope *import, XsDiagnostics *diagnostics)
{
  if(tree == nullptr || symbols == nullptr || import == nullptr || diagnostics == nullptr)
    return false;
  bool success = true;
  for(size_t symbol_index = 0; symbol_index < symbols->count; ++symbol_index)
  {
    const XsHirSymbol *owner = &symbols->symbols[symbol_index];
    if(owner->span.file_id != tree->file_id || !supports_base_list(owner))
      continue;
    for(size_t child_index = 0; child_index < owner->syntax->child_count; ++child_index)
    {
      const XsSyntaxNode *type = owner->syntax->children[child_index];
      if(is_base_type_node(type))
        success = validate_base(owner, type, symbols, import, diagnostics) && success;
    }
    if(owner->kind == XS_HIR_SYMBOL_CLASS)
      success = validate_overrides(owner, symbols, import, diagnostics) && success;
  }
  return success && !xs_diagnostics_has_error(diagnostics);
}
