/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "internal.h"

#include <string.h>

bool xs_macro_text_equal(XsText left, XsText right)
{
  return left.length == right.length && memcmp(left.data, right.data, left.length) == 0;
}

bool xs_macro_token_text_matches(const XsSyntaxNode *matcher, const XsSyntaxNode *argument)
{
  if(matcher->token_kind != argument->token_kind)
    return false;
  if(matcher->text.length == 0 || argument->text.length == 0)
    return true;
  return xs_macro_text_equal(matcher->text, argument->text);
}

static bool token_is_literal(XsTokenKind kind)
{
  return kind == XS_TOKEN_INTEGER || kind == XS_TOKEN_FLOAT || kind == XS_TOKEN_STRING || kind == XS_TOKEN_CHARACTER ||
         kind == XS_TOKEN_KW_TRUE || kind == XS_TOKEN_KW_FALSE || kind == XS_TOKEN_KW_NONE;
}

static bool token_is_visibility(XsTokenKind kind)
{
  return kind == XS_TOKEN_KW_PUBLIC || kind == XS_TOKEN_KW_PRIVATE || kind == XS_TOKEN_KW_PROTECTED ||
         kind == XS_TOKEN_KW_INTERNAL;
}

static bool single_token_fragment_supported(XsText kind)
{
  return xs_macro_text_equal(kind, (XsText){.data = "tt", .length = 2}) ||
         xs_macro_text_equal(kind, (XsText){.data = "ident", .length = 5}) ||
         xs_macro_text_equal(kind, (XsText){.data = "literal", .length = 7}) ||
         xs_macro_text_equal(kind, (XsText){.data = "lifetime", .length = 8}) ||
         xs_macro_text_equal(kind, (XsText){.data = "vis", .length = 3});
}

bool xs_macro_fragment_kind_is(const XsSyntaxNode *fragment, const char *kind)
{
  size_t length = strlen(kind);
  return fragment->child_count >= 2 && fragment->children[1]->text.length == length &&
         memcmp(fragment->children[1]->text.data, kind, length) == 0;
}

bool xs_macro_fragment_supported(XsText kind)
{
  return single_token_fragment_supported(kind) || xs_macro_text_equal(kind, (XsText){.data = "expr", .length = 4}) ||
         xs_macro_text_equal(kind, (XsText){.data = "stmt", .length = 4}) ||
         xs_macro_text_equal(kind, (XsText){.data = "block", .length = 5}) ||
         xs_macro_text_equal(kind, (XsText){.data = "ty", .length = 2}) ||
         xs_macro_text_equal(kind, (XsText){.data = "path", .length = 4}) ||
         xs_macro_text_equal(kind, (XsText){.data = "item", .length = 4}) ||
         xs_macro_text_equal(kind, (XsText){.data = "pat", .length = 3});
}

bool xs_macro_fragment_is_sequence(const XsSyntaxNode *fragment)
{
  return xs_macro_fragment_kind_is(fragment, "expr") || xs_macro_fragment_kind_is(fragment, "stmt") ||
         xs_macro_fragment_kind_is(fragment, "block") || xs_macro_fragment_kind_is(fragment, "ty") ||
         xs_macro_fragment_kind_is(fragment, "path") || xs_macro_fragment_kind_is(fragment, "item") ||
         xs_macro_fragment_kind_is(fragment, "pat");
}

bool xs_macro_fragment_matches(const XsSyntaxNode *fragment, const XsSyntaxNode *argument)
{
  if(fragment->child_count < 2)
    return false;
  XsText kind = fragment->children[1]->text;
  if(xs_macro_text_equal(kind, (XsText){.data = "tt", .length = 2}))
    return true;
  if(xs_macro_text_equal(kind, (XsText){.data = "ident", .length = 5}))
    return argument->token_kind == XS_TOKEN_IDENTIFIER;
  if(xs_macro_text_equal(kind, (XsText){.data = "literal", .length = 7}))
    return token_is_literal(argument->token_kind);
  if(xs_macro_text_equal(kind, (XsText){.data = "lifetime", .length = 8}))
    return argument->token_kind == XS_TOKEN_LIFETIME;
  if(xs_macro_text_equal(kind, (XsText){.data = "vis", .length = 3}))
    return token_is_visibility(argument->token_kind);
  return false;
}

static bool text_is_cstr(XsText text, const char *value)
{
  size_t length = strlen(value);
  return text.length == length && memcmp(text.data, value, length) == 0;
}

static bool path_is_single_module(const XsSyntaxNode *path, const char *module_name)
{
  if(path == nullptr || path->kind != XS_SYNTAX_PATH || path->child_count != 1)
    return false;
  const XsSyntaxNode *segment = path->children[0];
  return segment->kind == XS_SYNTAX_IDENTIFIER && text_is_cstr(segment->text, module_name);
}

static bool module_exports_macro(const char *module_name, XsText macro_name)
{
  if(strcmp(module_name, "panic") == 0)
  {
    return text_is_cstr(macro_name, "assert") || text_is_cstr(macro_name, "assert_eq") ||
           text_is_cstr(macro_name, "assert_ne") || text_is_cstr(macro_name, "debug_assert") ||
           text_is_cstr(macro_name, "debug_assert_eq") || text_is_cstr(macro_name, "panic");
  }
  if(strcmp(module_name, "stdio") == 0)
  {
    return text_is_cstr(macro_name, "print") || text_is_cstr(macro_name, "println") ||
           text_is_cstr(macro_name, "eprint") || text_is_cstr(macro_name, "eprintln") ||
           text_is_cstr(macro_name, "format");
  }
  return false;
}

static bool imported_name_matches(const XsSyntaxNode *import_name, XsText macro_name)
{
  if(import_name == nullptr || import_name->kind != XS_SYNTAX_IMPORT_NAME || import_name->child_count == 0)
    return false;
  const XsSyntaxNode *name = import_name->children[0];
  return name->kind == XS_SYNTAX_IDENTIFIER && xs_macro_text_equal(name->text, macro_name);
}

static bool import_resolves_module_macro(const XsSyntaxNode *import_node, const char *module_name, XsText macro_name)
{
  bool has_selected_names = false;
  bool has_module_path = false;
  for(size_t index = 0; index < import_node->child_count; ++index)
  {
    const XsSyntaxNode *child = import_node->children[index];
    if(child->kind == XS_SYNTAX_IMPORT_NAME)
    {
      has_selected_names = true;
      if(imported_name_matches(child, macro_name))
        return module_exports_macro(module_name, macro_name);
    }
    else if(child->kind == XS_SYNTAX_PATH && path_is_single_module(child, module_name))
      has_module_path = true;
  }
  if(!has_module_path)
    return false;
  if((import_node->flags & XS_SYNTAX_FLAG_WILDCARD) != 0)
    return module_exports_macro(module_name, macro_name);
  return !has_selected_names && module_exports_macro(module_name, macro_name);
}

bool xs_macro_external_import_resolves(const XsSyntaxTree *tree, XsText name)
{
  if(module_exports_macro("panic", name))
    return true;
  if(tree == nullptr || tree->root == nullptr)
    return false;
  for(size_t index = 0; index < tree->root->child_count; ++index)
  {
    const XsSyntaxNode *child = tree->root->children[index];
    if(child->kind != XS_SYNTAX_DECL_IMPORT)
      continue;
    if((child->flags & XS_SYNTAX_FLAG_USING) != 0)
      continue;
    if(import_resolves_module_macro(child, "panic", name) || import_resolves_module_macro(child, "stdio", name))
      return true;
  }
  return false;
}
