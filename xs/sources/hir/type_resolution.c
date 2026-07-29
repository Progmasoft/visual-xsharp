/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "xs/hir/type_resolution.h"
#include "standard_library.h"
#include "type_resolution_internal.h"
#include "xs/hir/type_info.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct GenericScope GenericScope;
struct GenericScope
{
  const GenericScope *parent;
  const XsSyntaxNode *declaration;
};

static const XsSyntaxNode *first_child_kind(const XsSyntaxNode *node, XsSyntaxKind kind)
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

static char *copy_cstr(const char *text)
{
  size_t length = strlen(text);
  char *copy = malloc(length + 1);
  if(copy != nullptr)
    memcpy(copy, text, length + 1);
  return copy;
}

static char *join_qualified(const char *namespace_name, const char *name)
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

static XsSpan node_span(const XsSyntaxNode *node)
{
  return (XsSpan){.start = node->span.start_offset, .end = node->span.end_offset};
}

static bool text_equal(XsText left, XsText right)
{
  return left.length == right.length && memcmp(left.data, right.data, left.length) == 0;
}

static bool primitive_type_name(XsText name)
{
  return xs_hir_primitive_find(name.data, name.length) != nullptr;
}

static bool text_matches_cstr(XsText text, const char *value)
{
  size_t length = strlen(value);
  return text.length == length && memcmp(text.data, value, length) == 0;
}

static const XsSyntaxNode *path_identifier_at(const XsSyntaxNode *path, size_t target)
{
  if(path == nullptr || path->kind != XS_SYNTAX_PATH)
    return nullptr;
  size_t index = 0;
  for(size_t i = 0; i < path->child_count; ++i)
  {
    if(path->children[i]->kind != XS_SYNTAX_IDENTIFIER)
      continue;
    if(index == target)
      return path->children[i];
    ++index;
  }
  return nullptr;
}

static size_t path_segment_count(const XsSyntaxNode *path)
{
  size_t count = 0;
  if(path == nullptr)
    return 0;
  for(size_t i = 0; i < path->child_count; ++i)
  {
    if(path->children[i]->kind == XS_SYNTAX_IDENTIFIER)
      ++count;
  }
  return count;
}

static char *path_to_string(const XsSyntaxNode *path)
{
  size_t count = path_segment_count(path);
  if(count == 0)
    return copy_cstr("");
  size_t length = count - 1;
  for(size_t i = 0; i < count; ++i)
    length += path_identifier_at(path, i)->text.length;
  char *result = malloc(length + 1);
  if(result == nullptr)
    return nullptr;
  size_t offset = 0;
  for(size_t i = 0; i < count; ++i)
  {
    const XsSyntaxNode *identifier = path_identifier_at(path, i);
    if(offset != 0)
      result[offset++] = '.';
    memcpy(result + offset, identifier->text.data, identifier->text.length);
    offset += identifier->text.length;
  }
  result[offset] = '\0';
  return result;
}

static const XsHirStandardTypeInfo *find_standard_type_from_path(const XsSyntaxNode *path)
{
  char *name = path_to_string(path);
  if(name == nullptr)
    return nullptr;
  const XsHirStandardTypeInfo *type = xs_hir_standard_type_find(name);
  free(name);
  return type;
}

static bool type_placeholder_name(XsText name)
{
  return text_matches_cstr(name, "else");
}

static bool generic_scope_contains(const GenericScope *scope, XsText name)
{
  for(const GenericScope *current = scope; current != nullptr; current = current->parent)
  {
    for(size_t i = 0; i < current->declaration->child_count; ++i)
    {
      const XsSyntaxNode *parameter = current->declaration->children[i];
      if(parameter->kind != XS_SYNTAX_GENERIC_PARAMETER)
        continue;
      const XsSyntaxNode *identifier = first_child_kind(parameter, XS_SYNTAX_IDENTIFIER);
      if(identifier != nullptr && text_equal(identifier->text, name))
        return true;
    }
  }
  return false;
}

static bool symbol_is_type(const XsHirSymbol *symbol)
{
  return symbol != nullptr && (symbol->kind == XS_HIR_SYMBOL_CLASS || symbol->kind == XS_HIR_SYMBOL_INTERFACE ||
                               symbol->kind == XS_HIR_SYMBOL_ENUM || symbol->kind == XS_HIR_SYMBOL_DATA);
}

static const char *module_for_file(const XsHirSymbolTable *symbols, uint64_t file_id)
{
  for(size_t i = 0; i < symbols->count; ++i)
  {
    if(symbols->symbols[i].span.file_id == file_id)
      return symbols->symbols[i].module_name;
  }
  return "";
}

static bool symbol_visible_from(const XsHirSymbol *symbol, const XsHirSymbolTable *symbols, const char *namespace_name,
                                uint64_t current_file_id)
{
  if(symbol == nullptr)
    return false;
  switch(symbol->visibility)
  {
  case XS_SYNTAX_VISIBILITY_PUBLIC:
    return true;
  case XS_SYNTAX_VISIBILITY_INTERNAL:
    return strcmp(symbol->module_name, module_for_file(symbols, current_file_id)) == 0;
  case XS_SYNTAX_VISIBILITY_PRIVATE:
  case XS_SYNTAX_VISIBILITY_DEFAULT:
    return strcmp(symbol->namespace_name, namespace_name) == 0 && symbol->span.file_id == current_file_id;
  case XS_SYNTAX_VISIBILITY_PROTECTED:
    return false;
  }
  return false;
}

static bool symbol_module_available(const XsHirSymbol *symbol, const XsHirSymbolTable *symbols,
                                    uint64_t current_file_id, const XsHirImportScope *import)
{
  return symbol == nullptr || strcmp(symbol->module_name, module_for_file(symbols, current_file_id)) == 0 ||
         xs_hir_import_scope_has_module(import, symbol->namespace_name);
}

static const XsHirSymbol *resolve_user_type(const char *path, const char *namespace_name,
                                            const XsHirSymbolTable *symbols, const XsHirImportScope *import)
{
  const XsHirImportBinding *binding = xs_hir_import_scope_find(import, path);
  if(binding != nullptr)
    return binding->symbol;
  if(strchr(path, '.') != nullptr)
    return xs_hir_symbol_table_find(symbols, path);
  char *qualified = join_qualified(namespace_name, path);
  if(qualified == nullptr)
    return nullptr;
  const XsHirSymbol *symbol = xs_hir_symbol_table_find(symbols, qualified);
  free(qualified);
  return symbol;
}

static bool report_unresolved_type(XsDiagnostics *diagnostics, const XsSyntaxNode *type, const char *path)
{
  char message[512];
  snprintf(message, sizeof(message), "type '%s' does not resolve to a primitive, generic parameter, or visible type",
           path);
  return xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, node_span(type), message);
}

static bool report_invisible_type(XsDiagnostics *diagnostics, const XsSyntaxNode *type, const char *path,
                                  const char *namespace_name)
{
  char message[512];
  snprintf(message, sizeof(message), "type '%s' is not visible from namespace '%s'", path, namespace_name);
  return xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, node_span(type), message);
}

static bool report_unimported_type(XsDiagnostics *diagnostics, const XsSyntaxNode *type, const char *path)
{
  char message[512];
  snprintf(message, sizeof(message), "module for type '%s' is not imported", path);
  return xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, node_span(type), message);
}

static bool resolve_type_node(const XsSyntaxNode *type, const char *namespace_name, uint64_t current_file_id,
                              const GenericScope *generics, const XsHirSymbolTable *symbols,
                              const XsHirImportScope *import, XsDiagnostics *diagnostics);
static bool report_generic_arity(XsDiagnostics *diagnostics, const XsSyntaxNode *type, const char *path,
                                 size_t expected, size_t actual)
{
  char message[512];
  snprintf(message, sizeof(message), "generic type '%s' expects %zu type argument(s), got %zu", path, expected, actual);
  return xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, node_span(type), message);
}

static bool report_generic_arity_range(XsDiagnostics *diagnostics, const XsSyntaxNode *type, const char *path,
                                       size_t minimum, size_t maximum, size_t actual)
{
  if(minimum == maximum)
    return report_generic_arity(diagnostics, type, path, minimum, actual);
  char message[512];
  snprintf(message, sizeof(message), "generic type '%s' expects %zu to %zu type argument(s), got %zu", path, minimum,
           maximum, actual);
  return xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, node_span(type), message);
}

static bool standard_type_arity_ok(const XsHirStandardTypeInfo *type, size_t actual)
{
  return type != nullptr && actual >= type->min_arity && actual <= type->max_arity;
}

static bool standard_type_is_result(const XsHirStandardTypeInfo *type)
{
  return type != nullptr && strcmp(type->canonical_name, "std.result.Result") == 0;
}

static bool standard_enum_data_family_base(const XsSyntaxNode *specifier)
{
  if(specifier == nullptr || specifier->kind != XS_SYNTAX_BASE_SPECIFIER || specifier->child_count == 0)
    return false;
  const XsSyntaxNode *type = specifier->children[0];
  if(type->kind != XS_SYNTAX_TYPE_NAMED)
    return false;
  const XsHirStandardTypeInfo *standard = find_standard_type_from_path(first_child_kind(type, XS_SYNTAX_PATH));
  return standard != nullptr &&
         (strcmp(standard->canonical_name, "std.optional.Optional") == 0 || standard_type_is_result(standard));
}

static bool report_incomplete_result_type(XsDiagnostics *diagnostics, const XsSyntaxNode *type)
{
  return xs_diagnostics_add(
      diagnostics, XS_DIAGNOSTIC_ERROR, node_span(type),
      "single-argument Result is valid only as Result<()>; write both Result<T, E> payload types");
}

static bool report_constraint_kind(XsDiagnostics *diagnostics, const XsSyntaxNode *type, const char *path)
{
  char message[512];
  snprintf(message, sizeof(message), "generic constraint '%s' must resolve to an interface", path);
  return xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, node_span(type), message);
}
static bool report_duplicate_generic_parameter(XsDiagnostics *diagnostics, const XsSyntaxNode *identifier)
{
  char message[512];
  int length = identifier->text.length > 128 ? 128 : (int)identifier->text.length;
  snprintf(message, sizeof(message), "generic parameter '%.*s' is already defined in this declaration", length,
           identifier->text.data);
  return xs_diagnostics_add(diagnostics, XS_DIAGNOSTIC_ERROR, node_span(identifier), message);
}

static size_t generic_parameter_count(const XsSyntaxNode *declaration)
{
  size_t count = 0;
  if(declaration == nullptr)
    return 0;
  for(size_t i = 0; i < declaration->child_count; ++i)
  {
    if(declaration->children[i]->kind == XS_SYNTAX_GENERIC_PARAMETER)
      ++count;
  }
  return count;
}

static bool validate_generic_parameter_names(const XsSyntaxNode *declaration, XsDiagnostics *diagnostics)
{
  bool success = true;
  for(size_t i = 0; i < declaration->child_count; ++i)
  {
    const XsSyntaxNode *parameter = declaration->children[i];
    if(parameter->kind != XS_SYNTAX_GENERIC_PARAMETER)
      continue;
    const XsSyntaxNode *identifier = first_child_kind(parameter, XS_SYNTAX_IDENTIFIER);
    if(identifier == nullptr)
      continue;
    for(size_t j = 0; j < i; ++j)
    {
      const XsSyntaxNode *previous = declaration->children[j];
      if(previous->kind != XS_SYNTAX_GENERIC_PARAMETER)
        continue;
      const XsSyntaxNode *previous_identifier = first_child_kind(previous, XS_SYNTAX_IDENTIFIER);
      if(previous_identifier != nullptr && text_equal(previous_identifier->text, identifier->text))
        success = report_duplicate_generic_parameter(diagnostics, identifier) && success;
    }
  }
  return success;
}

static bool resolve_named_type(const XsSyntaxNode *type, const char *namespace_name, const GenericScope *generics,
                               const XsHirSymbolTable *symbols, const XsHirImportScope *import,
                               XsDiagnostics *diagnostics, uint64_t current_file_id, bool check_arity,
                               const XsHirSymbol **resolved)
{
  if(resolved != nullptr)
    *resolved = nullptr;
  const XsSyntaxNode *path = first_child_kind(type, XS_SYNTAX_PATH);
  const XsSyntaxNode *first = path_identifier_at(path, 0);
  if(first == nullptr)
    return true;
  if(path_segment_count(path) == 1 &&
     (primitive_type_name(first->text) || generic_scope_contains(generics, first->text) ||
      type_placeholder_name(first->text)))
    return true;
  char *name = path_to_string(path);
  if(name == nullptr)
    return false;
  const XsHirStandardTypeInfo *standard = xs_hir_standard_type_find(name);
  if(standard != nullptr)
  {
    bool success = true;
    if(xs_hir_standard_type_lookup(standard, import) == XS_HIR_STANDARD_MISSING_IMPORT)
      success = report_unimported_type(diagnostics, type, name) && success;
    if(check_arity && standard->max_arity != 0)
      success =
          report_generic_arity_range(diagnostics, type, name, standard->min_arity, standard->max_arity, 0) && success;
    free(name);
    return success;
  }
  const XsHirSymbol *symbol = resolve_user_type(name, namespace_name, symbols, import);
  bool success = true;
  if(!symbol_is_type(symbol))
    success = report_unresolved_type(diagnostics, type, name);
  else if(!symbol_visible_from(symbol, symbols, namespace_name, current_file_id))
    success = report_invisible_type(diagnostics, type, name, namespace_name);
  else if(strchr(name, '.') != nullptr && !symbol_module_available(symbol, symbols, current_file_id, import))
    success = report_unimported_type(diagnostics, type, name);
  else
  {
    if(check_arity)
    {
      size_t expected = generic_parameter_count(symbol->syntax);
      if(expected != 0)
        success = report_generic_arity(diagnostics, type, name, expected, 0);
    }
    if(resolved != nullptr)
      *resolved = symbol;
  }
  free(name);
  return success;
}

static bool resolve_generic_type_node(const XsSyntaxNode *type, const char *namespace_name, uint64_t current_file_id,
                                      const GenericScope *generics, const XsHirSymbolTable *symbols,
                                      const XsHirImportScope *import, XsDiagnostics *diagnostics)
{
  if(type->child_count == 0)
    return true;
  const XsSyntaxNode *base = type->children[0];
  const XsHirSymbol *symbol = nullptr;
  bool success =
      resolve_named_type(base, namespace_name, generics, symbols, import, diagnostics, current_file_id, false, &symbol);
  const XsSyntaxNode *base_path = first_child_kind(base, XS_SYNTAX_PATH);
  const XsHirStandardTypeInfo *standard = find_standard_type_from_path(base_path);
  if(standard != nullptr)
  {
    char *name = path_to_string(base_path);
    if(name == nullptr)
      return false;
    size_t actual = type->child_count - 1;
    if(!standard_type_arity_ok(standard, actual))
      success = report_generic_arity_range(diagnostics, type, name, standard->min_arity, standard->max_arity, actual) &&
                success;
    if(standard_type_is_result(standard) && actual == 1 && type->children[1]->kind != XS_SYNTAX_TYPE_UNIT)
      success = report_incomplete_result_type(diagnostics, type) && success;
    free(name);
  }
  if(symbol != nullptr)
  {
    char *name = path_to_string(first_child_kind(base, XS_SYNTAX_PATH));
    if(name == nullptr)
      return false;
    size_t expected = generic_parameter_count(symbol->syntax);
    size_t actual = type->child_count - 1;
    if(expected != actual)
      success = report_generic_arity(diagnostics, type, name, expected, actual) && success;
    free(name);
  }
  for(size_t i = 1; i < type->child_count; ++i)
    success =
        resolve_type_node(type->children[i], namespace_name, current_file_id, generics, symbols, import, diagnostics) &&
        success;
  return success;
}

static bool resolve_constraint_type(const XsSyntaxNode *type, const char *namespace_name, uint64_t current_file_id,
                                    const GenericScope *generics, const XsHirSymbolTable *symbols,
                                    const XsHirImportScope *import, XsDiagnostics *diagnostics)
{
  if(type->kind == XS_SYNTAX_TYPE_GENERIC)
  {
    if(type->child_count == 0)
      return true;
    const XsSyntaxNode *base = type->children[0];
    const XsHirSymbol *symbol = nullptr;
    bool success = resolve_named_type(base, namespace_name, generics, symbols, import, diagnostics, current_file_id,
                                      false, &symbol);
    if(symbol != nullptr)
    {
      char *name = path_to_string(first_child_kind(base, XS_SYNTAX_PATH));
      if(name == nullptr)
        return false;
      if(symbol->kind != XS_HIR_SYMBOL_INTERFACE)
        success = report_constraint_kind(diagnostics, base, name) && success;
      free(name);
    }
    return resolve_generic_type_node(type, namespace_name, current_file_id, generics, symbols, import, diagnostics) &&
           success;
  }
  if(type->kind != XS_SYNTAX_TYPE_NAMED)
    return resolve_type_node(type, namespace_name, current_file_id, generics, symbols, import, diagnostics);
  const XsSyntaxNode *path = first_child_kind(type, XS_SYNTAX_PATH);
  char *name = path_to_string(path);
  if(name == nullptr)
    return false;
  const XsHirSymbol *symbol = nullptr;
  bool success =
      resolve_named_type(type, namespace_name, generics, symbols, import, diagnostics, current_file_id, true, &symbol);
  if(symbol != nullptr && symbol->kind != XS_HIR_SYMBOL_INTERFACE)
    success = report_constraint_kind(diagnostics, type, name) && success;
  free(name);
  return success;
}

static bool resolve_generic_parameter_constraints(const XsSyntaxNode *parameter, const char *namespace_name,
                                                  uint64_t current_file_id, const GenericScope *generics,
                                                  const XsHirSymbolTable *symbols, const XsHirImportScope *import,
                                                  XsDiagnostics *diagnostics)
{
  bool success = true;
  for(size_t i = 0; i < parameter->child_count; ++i)
  {
    const XsSyntaxNode *child = parameter->children[i];
    if(child->kind == XS_SYNTAX_IDENTIFIER)
      continue;
    success = resolve_constraint_type(child, namespace_name, current_file_id, generics, symbols, import, diagnostics) &&
              success;
  }
  return success;
}

static bool resolve_type_node(const XsSyntaxNode *type, const char *namespace_name, uint64_t current_file_id,
                              const GenericScope *generics, const XsHirSymbolTable *symbols,
                              const XsHirImportScope *import, XsDiagnostics *diagnostics)
{
  if(type == nullptr)
    return true;
  bool success = true;
  if(type->kind == XS_SYNTAX_TYPE_NAMED)
  {
    const XsHirSymbol *symbol = nullptr;
    return resolve_named_type(type, namespace_name, generics, symbols, import, diagnostics, current_file_id, true,
                              &symbol);
  }
  if(type->kind == XS_SYNTAX_TYPE_GENERIC)
    return resolve_generic_type_node(type, namespace_name, current_file_id, generics, symbols, import, diagnostics);
  for(size_t i = 0; i < type->child_count; ++i)
  {
    if(type->children[i]->kind == XS_SYNTAX_EXPR_LITERAL)
      continue;
    success =
        resolve_type_node(type->children[i], namespace_name, current_file_id, generics, symbols, import, diagnostics) &&
        success;
  }
  return success;
}

static bool declaration_opens_generic_scope(XsSyntaxKind kind)
{
  return kind == XS_SYNTAX_DECL_FUNCTION || kind == XS_SYNTAX_DECL_CLASS || kind == XS_SYNTAX_DECL_INTERFACE ||
         kind == XS_SYNTAX_DECL_ENUM || kind == XS_SYNTAX_DECL_DATA;
}

static bool resolve_declaration_types(const XsSyntaxNode *node, const char *namespace_name, uint64_t current_file_id,
                                      const GenericScope *generics, const XsHirSymbolTable *symbols,
                                      const XsHirImportScope *import, XsDiagnostics *diagnostics,
                                      const XsMacroDeclarationExpansionSet *macro_declarations,
                                      const XsMacroStatementExpansionSet *macro_statements)
{
  if(node == nullptr)
    return true;
  GenericScope local_scope = {.parent = generics, .declaration = node};
  bool opens_generic_scope = declaration_opens_generic_scope(node->kind);
  const GenericScope *active = opens_generic_scope ? &local_scope : generics;
  bool success = true;
  if(opens_generic_scope)
    success = validate_generic_parameter_names(node, diagnostics);
  if(node->kind == XS_SYNTAX_GENERIC_PARAMETER)
    return resolve_generic_parameter_constraints(node, namespace_name, current_file_id, active, symbols, import,
                                                 diagnostics);
  if(standard_enum_data_family_base(node))
    return true;
  if(node->kind == XS_SYNTAX_TYPE_NAMED || node->kind == XS_SYNTAX_TYPE_GENERIC || node->kind == XS_SYNTAX_TYPE_ARRAY ||
     node->kind == XS_SYNTAX_TYPE_FIXED_ARRAY || node->kind == XS_SYNTAX_TYPE_MAP ||
     node->kind == XS_SYNTAX_TYPE_POINTER || node->kind == XS_SYNTAX_TYPE_REFERENCE ||
     node->kind == XS_SYNTAX_TYPE_MUTABLE_REFERENCE || node->kind == XS_SYNTAX_TYPE_TUPLE ||
     node->kind == XS_SYNTAX_TYPE_FUNCTION)
    return resolve_type_node(node, namespace_name, current_file_id, active, symbols, import, diagnostics);
  if(xs_hir_declaration_uses_expanded_member_view(node, macro_declarations))
  {
    XsMacroExpandedDeclarationSet expanded = {0};
    if(!xs_macro_expand_child_declarations(node, macro_declarations, diagnostics, &expanded))
      return false;
    for(size_t i = 0; i < expanded.count; ++i)
      success = resolve_declaration_types(expanded.items[i].declaration, namespace_name, current_file_id, active,
                                          symbols, import, diagnostics, macro_declarations, macro_statements) &&
                success;
    xs_macro_expanded_declaration_set_free(&expanded);
    return success;
  }
  if(macro_statements != nullptr && xs_hir_node_has_statement_macro_child(node))
  {
    XsMacroExpandedStatementSet expanded = {0};
    if(!xs_macro_expand_child_statements(node, macro_statements, diagnostics, &expanded))
      return false;
    for(size_t i = 0; i < expanded.count; ++i)
      success = resolve_declaration_types(expanded.items[i].statement, namespace_name, current_file_id, active, symbols,
                                          import, diagnostics, macro_declarations, macro_statements) &&
                success;
    xs_macro_expanded_statement_set_free(&expanded);
    return success;
  }
  for(size_t i = 0; i < node->child_count; ++i)
    success = resolve_declaration_types(node->children[i], namespace_name, current_file_id, active, symbols, import,
                                        diagnostics, macro_declarations, macro_statements) &&
              success;
  return success;
}

bool xs_hir_resolve_types(const XsSyntaxTree *tree, const XsHirSymbolTable *symbols, const XsHirImportScope *import,
                          XsDiagnostics *diagnostics)
{
  return xs_hir_resolve_types_expanded(tree, nullptr, symbols, import, diagnostics);
}

bool xs_hir_resolve_types_expanded(const XsSyntaxTree *tree, const XsMacroStatementExpansionSet *macro_statements,
                                   const XsHirSymbolTable *symbols, const XsHirImportScope *import,
                                   XsDiagnostics *diagnostics)
{
  return xs_hir_resolve_types_with_macros(tree, nullptr, macro_statements, symbols, import, diagnostics);
}

bool xs_hir_resolve_types_with_macros(const XsSyntaxTree *tree,
                                      const XsMacroDeclarationExpansionSet *macro_declarations,
                                      const XsMacroStatementExpansionSet *macro_statements,
                                      const XsHirSymbolTable *symbols, const XsHirImportScope *import,
                                      XsDiagnostics *diagnostics)
{
  if(tree == nullptr || tree->root == nullptr || symbols == nullptr || import == nullptr || diagnostics == nullptr)
    return false;
  bool success = true;
  for(size_t i = 0; i < symbols->count; ++i)
  {
    const XsHirSymbol *symbol = &symbols->symbols[i];
    if(symbol->span.file_id != tree->file_id)
      continue;
    success = resolve_declaration_types(symbol->syntax, symbol->namespace_name, tree->file_id, nullptr, symbols, import,
                                        diagnostics, macro_declarations, macro_statements) &&
              success;
  }
  return success && !xs_diagnostics_has_error(diagnostics);
}
