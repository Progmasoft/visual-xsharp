/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "parser_internal.h"

#include <stdio.h>
#include <string.h>

static bool syntax_text_equal(XsText left, XsText right)
{
  return left.length == right.length && memcmp(left.data, right.data, left.length) == 0;
}

static bool syntax_nodes_equal(const XsSyntaxNode *left, const XsSyntaxNode *right)
{
  if(left == right)
    return true;
  if(left == nullptr || right == nullptr || left->kind != right->kind || left->token_kind != right->token_kind ||
     left->flags != right->flags || left->child_count != right->child_count)
    return false;
  if(left->child_count == 0 && !syntax_text_equal(left->text, right->text))
    return false;
  for(size_t index = 0; index < left->child_count; ++index)
  {
    if(!syntax_nodes_equal(left->children[index], right->children[index]))
      return false;
  }
  return true;
}

static const XsSyntaxNode *first_child_kind(const XsSyntaxNode *node, XsSyntaxKind kind)
{
  for(size_t index = 0; index < node->child_count; ++index)
  {
    if(node->children[index]->kind == kind)
      return node->children[index];
  }
  return nullptr;
}

static void skip_forbidden_data_member(SyntaxParser *parser);
static void parse_base_list(SyntaxParser *parser, XsSyntaxNode *declaration);

static const XsSyntaxNode *enum_variant_payload(const XsSyntaxNode *variant)
{
  return variant->child_count == 2 ? variant->children[1] : nullptr;
}

static void validate_enum_variant_names(SyntaxParser *parser, XsSyntaxNode *declaration, bool data_enum)
{
  for(size_t index = 0; index < declaration->child_count; ++index)
  {
    XsSyntaxNode *variant = declaration->children[index];
    if(variant->kind != XS_SYNTAX_ENUM_VARIANT)
      continue;
    const XsSyntaxNode *name = variant->children[0];
    const XsSyntaxNode *payload = enum_variant_payload(variant);
    for(size_t previous_index = 0; previous_index < index; ++previous_index)
    {
      const XsSyntaxNode *previous = declaration->children[previous_index];
      if(previous->kind != XS_SYNTAX_ENUM_VARIANT || !syntax_text_equal(name->text, previous->children[0]->text))
        continue;
      const XsSyntaxNode *previous_payload = enum_variant_payload(previous);
      if(!data_enum)
        xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR,
                           (XsSpan){name->span.start_offset, name->span.end_offset},
                           "regular enum variants must have unique names");
      else if(payload == nullptr || previous_payload == nullptr || syntax_nodes_equal(payload, previous_payload))
        xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR,
                           (XsSpan){name->span.start_offset, name->span.end_offset},
                           "data enum variant overloads require distinct payload types");
      else
        variant->flags |= XS_SYNTAX_FLAG_ENUM_VARIANT_OVERLOAD;
      break;
    }
  }
}

static bool generic_constraint_comma_starts_parameter(SyntaxParser *parser)
{
  if(parser->current.kind != XS_TOKEN_COMMA || parser->next.kind != XS_TOKEN_IDENTIFIER)
    return false;
  SyntaxParser lookahead = *parser;
  advance(&lookahead);
  advance(&lookahead);
  return lookahead.current.kind == XS_TOKEN_COLON;
}

static void parse_generics(SyntaxParser *parser, XsSyntaxNode *parent)
{
  if(!accept(parser, XS_TOKEN_LESS))
    return;
  while(parser->current.kind != XS_TOKEN_GREATER && parser->current.kind != XS_TOKEN_EOF)
  {
    size_t start = parser->current.span.start;
    XsSyntaxNode *parameter = node(parser, XS_SYNTAX_GENERIC_PARAMETER, (XsSpan){start, start});
    xs_syntax_node_add(parser->tree, parameter, identifier(parser));
    if(accept(parser, XS_TOKEN_COLON))
    {
      do
      {
        xs_syntax_node_add(parser->tree, parameter, parse_type(parser));
        if(parser->current.kind != XS_TOKEN_COMMA || generic_constraint_comma_starts_parameter(parser))
          break;
        advance(parser);
      } while(true);
    }
    finish_node(parser, parameter, parser->previous.span.end);
    xs_syntax_node_add(parser->tree, parent, parameter);
    if(!accept(parser, XS_TOKEN_COMMA))
      break;
  }
  expect(parser, XS_TOKEN_GREATER, "expected '>' after generic parameters");
}

static void parse_parameters(SyntaxParser *parser, XsSyntaxNode *function)
{
  expect(parser, XS_TOKEN_LEFT_PAREN, "expected '(' before parameters");
  if(parser->current.kind != XS_TOKEN_RIGHT_PAREN)
  {
    do
    {
      size_t start = parser->current.span.start;
      XsSyntaxNode *parameter = node(parser, XS_SYNTAX_PARAMETER, (XsSpan){start, start});
      xs_syntax_node_add(parser->tree, parameter, identifier(parser));
      expect(parser, XS_TOKEN_COLON, "expected ':' after parameter name");
      xs_syntax_node_add(parser->tree, parameter, parse_type(parser));
      finish_node(parser, parameter, parser->previous.span.end);
      xs_syntax_node_add(parser->tree, function, parameter);
    } while(accept(parser, XS_TOKEN_COMMA));
  }
  expect(parser, XS_TOKEN_RIGHT_PAREN, "expected ')' after parameters");
}

static bool token_is_overloadable_operator(XsTokenKind kind)
{
  switch(kind)
  {
  case XS_TOKEN_PLUS:
  case XS_TOKEN_MINUS:
  case XS_TOKEN_STAR:
  case XS_TOKEN_SLASH:
  case XS_TOKEN_PERCENT:
  case XS_TOKEN_EQUAL:
  case XS_TOKEN_NOT_EQUAL:
  case XS_TOKEN_LESS:
  case XS_TOKEN_LESS_EQUAL:
  case XS_TOKEN_GREATER:
  case XS_TOKEN_GREATER_EQUAL:
  case XS_TOKEN_AMPERSAND:
  case XS_TOKEN_PIPE:
  case XS_TOKEN_CARET:
  case XS_TOKEN_SHIFT_LEFT:
  case XS_TOKEN_SHIFT_RIGHT:
    return true;
  default:
    return false;
  }
}

static uint32_t dispatch_modifier_flags(const XsSyntaxNode *declaration)
{
  return declaration->flags & (XS_SYNTAX_FLAG_VIRTUAL | XS_SYNTAX_FLAG_OVERRIDE | XS_SYNTAX_FLAG_SEALED);
}

static void reject_dispatch_modifiers(SyntaxParser *parser, const XsSyntaxNode *declaration, const char *context)
{
  if(dispatch_modifier_flags(declaration) == 0)
    return;
  char message[160];
  snprintf(message, sizeof(message), "virtual, override, and sealed are not valid on %s", context);
  xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR,
                     (XsSpan){declaration->span.start_offset, declaration->span.end_offset}, message);
}

static void validate_method_dispatch_modifiers(SyntaxParser *parser, const XsSyntaxNode *function, bool class_method,
                                               bool interface_method)
{
  uint32_t flags = dispatch_modifier_flags(function);
  if(flags == 0)
    return;
  XsSpan span = {function->span.start_offset, function->span.end_offset};
  if(!class_method || interface_method)
  {
    xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR, span,
                       "explicit virtual, override, and sealed modifiers are only valid on class methods");
    return;
  }
  if((function->flags & XS_SYNTAX_FLAG_STATIC) != 0)
    xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR, span,
                       "static methods cannot be virtual, override, or sealed");
  if((flags & XS_SYNTAX_FLAG_SEALED) != 0 && (flags & XS_SYNTAX_FLAG_OVERRIDE) == 0)
    xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR, span, "a sealed method must also be an override");
}

static void validate_type_dispatch_modifiers(SyntaxParser *parser, const XsSyntaxNode *declaration, bool interface)
{
  XsSpan span = {declaration->span.start_offset, declaration->span.end_offset};
  if((declaration->flags & (XS_SYNTAX_FLAG_VIRTUAL | XS_SYNTAX_FLAG_OVERRIDE)) != 0 ||
     (interface && (declaration->flags & XS_SYNTAX_FLAG_SEALED) != 0))
    xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR, span,
                       "only classes may use the sealed type modifier; virtual and override apply to methods");
  if((declaration->flags & XS_SYNTAX_FLAG_SEALED) != 0 && (declaration->flags & XS_SYNTAX_FLAG_INCOMPLETE) != 0)
    xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR, span, "a class cannot be both sealed and incomplete");
}

static XsSyntaxNode *parse_function(SyntaxParser *parser, Modifiers modifiers, size_t start, bool signature_allowed,
                                    bool referential_transparent)
{
  XsSyntaxNode *function = node(parser, XS_SYNTAX_DECL_FUNCTION, (XsSpan){start, parser->previous.span.end});
  attach_modifiers(parser, function, modifiers);
  if(referential_transparent)
    function->flags |= XS_SYNTAX_FLAG_REFERENTIAL_TRANSPARENT;
  XsSyntaxNode *name = identifier(parser);
  xs_syntax_node_add(parser->tree, function, name);
  if(name != nullptr && syntax_text_equal(name->text, (XsText){.data = "operator", .length = 8}))
  {
    if(!token_is_overloadable_operator(parser->current.kind))
      xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR, parser->current.span,
                         "expected overloadable operator after 'operator'");
    else
    {
      XsSyntaxNode *operator_token = node(parser, XS_SYNTAX_TOKEN, parser->current.span);
      operator_token->token_kind = parser->current.kind;
      xs_syntax_node_add(parser->tree, function, operator_token);
      function->flags |= XS_SYNTAX_FLAG_OPERATOR;
      advance(parser);
    }
  }
  parse_generics(parser, function);
  parse_parameters(parser, function);
  if(accept(parser, XS_TOKEN_ARROW))
  {
    XsSyntaxNode *return_type = parse_type(parser);
    return_type->flags |= XS_SYNTAX_FLAG_RETURN_TYPE;
    xs_syntax_node_add(parser->tree, function, return_type);
  }
  if(accept(parser, XS_TOKEN_SEMICOLON))
  {
    if(!signature_allowed && (function->flags & XS_SYNTAX_FLAG_INCOMPLETE) == 0)
      xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR, parser->previous.span,
                         "function declarations without bodies must be marked incomplete");
    function->flags |= XS_SYNTAX_FLAG_INCOMPLETE;
  }
  else
  {
    if((function->flags & XS_SYNTAX_FLAG_INCOMPLETE) != 0)
      xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR, parser->current.span,
                         "incomplete functions must not have bodies");
    xs_syntax_node_add(parser->tree, function, parse_block(parser));
  }
  finish_node(parser, function, parser->previous.span.end);
  return function;
}

static bool accept_trailing_glob(SyntaxParser *parser)
{
  if(parser->current.kind != XS_TOKEN_STAR &&
     ((parser->current.kind != XS_TOKEN_DOUBLE_COLON && parser->current.kind != XS_TOKEN_DOT) ||
      parser->next.kind != XS_TOKEN_STAR))
    return false;
  if(parser->current.kind != XS_TOKEN_STAR)
    advance(parser);
  return accept(parser, XS_TOKEN_STAR);
}

static XsSyntaxNode *parse_import(SyntaxParser *parser, bool selected, size_t start)
{
  XsSyntaxNode *import = node(parser, XS_SYNTAX_DECL_IMPORT, (XsSpan){start, parser->previous.span.end});
  if(selected)
  {
    xs_syntax_node_add(parser->tree, import, parse_path(parser));
    expect(parser, XS_TOKEN_KW_IMPORT, "expected import after module path");
    if(accept_trailing_glob(parser))
    {
      import->flags |= XS_SYNTAX_FLAG_WILDCARD;
    }
    else
    {
      do
      {
        XsSyntaxNode *name =
            node(parser, XS_SYNTAX_IMPORT_NAME, (XsSpan){parser->current.span.start, parser->current.span.start});
        xs_syntax_node_add(parser->tree, name, identifier(parser));
        if(accept(parser, XS_TOKEN_KW_AS))
          xs_syntax_node_add(parser->tree, name, identifier(parser));
        finish_node(parser, name, parser->previous.span.end);
        xs_syntax_node_add(parser->tree, import, name);
      } while(accept(parser, XS_TOKEN_COMMA));
    }
  }
  else
  {
    do
    {
      xs_syntax_node_add(parser->tree, import, parse_path(parser));
    } while(accept(parser, XS_TOKEN_COMMA));
    if(accept(parser, XS_TOKEN_KW_AS))
      xs_syntax_node_add(parser->tree, import, identifier(parser));
  }
  expect(parser, XS_TOKEN_SEMICOLON, "expected ';' after import");
  finish_node(parser, import, parser->previous.span.end);
  return import;
}

static XsSyntaxNode *parse_using(SyntaxParser *parser, size_t start)
{
  XsSyntaxNode *using_decl = node(parser, XS_SYNTAX_DECL_IMPORT, (XsSpan){start, parser->previous.span.end});
  using_decl->flags |= XS_SYNTAX_FLAG_USING;
  bool namespace_using = false;
  if(accept(parser, XS_TOKEN_KW_NAMESPACE))
  {
    namespace_using = true;
    using_decl->flags |= XS_SYNTAX_FLAG_WILDCARD;
    xs_syntax_node_add(parser->tree, using_decl, parse_path(parser));
  }
  else if(parser->current.kind == XS_TOKEN_IDENTIFIER && parser->next.kind == XS_TOKEN_ASSIGN)
  {
    using_decl->flags |= XS_SYNTAX_FLAG_USING_ALIAS;
    xs_syntax_node_add(parser->tree, using_decl, identifier(parser));
    expect(parser, XS_TOKEN_ASSIGN, "expected '=' after using alias");
    xs_syntax_node_add(parser->tree, using_decl, parse_path(parser));
  }
  else
  {
    xs_syntax_node_add(parser->tree, using_decl, parse_path(parser));
  }
  if(accept_trailing_glob(parser))
    using_decl->flags |= XS_SYNTAX_FLAG_WILDCARD;
  if((using_decl->flags & XS_SYNTAX_FLAG_WILDCARD) != 0 && !namespace_using)
    xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR, parser->previous.span,
                       "using does not support glob import; use 'using namespace' instead");
  expect(parser, XS_TOKEN_SEMICOLON, "expected ';' after using declaration");
  finish_node(parser, using_decl, parser->previous.span.end);
  return using_decl;
}

static XsSyntaxNode *parse_declaration_macro_call(SyntaxParser *parser, size_t start)
{
  XsSyntaxNode *declaration = node(parser, XS_SYNTAX_DECL_MACRO_CALL, (XsSpan){start, start});
  xs_syntax_node_add(parser->tree, declaration, parse_expression(parser, 1));
  expect(parser, XS_TOKEN_SEMICOLON, "expected ';' after macro call declaration");
  finish_node(parser, declaration, parser->previous.span.end);
  return declaration;
}

static XsSyntaxNode *extern_abi_token(SyntaxParser *parser, XsSpan span)
{
  XsSyntaxNode *abi = node(parser, XS_SYNTAX_TOKEN, span);
  if(abi != nullptr)
    abi->token_kind = XS_TOKEN_STRING;
  return abi;
}

static XsSyntaxNode *parse_extern_static(SyntaxParser *parser, Modifiers modifiers, size_t start, XsSpan abi_span,
                                         bool static_consumed)
{
  if(!static_consumed)
    expect(parser, XS_TOKEN_KW_STATIC, "expected 'static'");
  XsSyntaxNode *declaration = node(parser, XS_SYNTAX_DECL_VARIABLE, (XsSpan){start, parser->previous.span.end});
  declaration->flags |= XS_SYNTAX_FLAG_EXTERN | XS_SYNTAX_FLAG_STATIC;
  attach_modifiers(parser, declaration, modifiers);
  xs_syntax_node_add(parser->tree, declaration, extern_abi_token(parser, abi_span));
  xs_syntax_node_add(parser->tree, declaration, identifier(parser));
  expect(parser, XS_TOKEN_COLON, "expected ':' after extern static name");
  xs_syntax_node_add(parser->tree, declaration, parse_type(parser));
  if(accept(parser, XS_TOKEN_ASSIGN))
  {
    xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR, parser->previous.span,
                       "extern static declarations must not have initializers");
    xs_syntax_node_add(parser->tree, declaration, parse_expression(parser, 1));
  }
  expect(parser, XS_TOKEN_SEMICOLON, "expected ';' after extern static declaration");
  finish_node(parser, declaration, parser->previous.span.end);
  return declaration;
}

static XsSyntaxNode *parse_extern_block(SyntaxParser *parser, Modifiers modifiers, size_t start)
{
  expect(parser, XS_TOKEN_KW_EXTERN, "expected 'extern'");
  XsSpan abi_span = parser->current.span;
  if(parser->current.kind == XS_TOKEN_STRING)
  {
    advance(parser);
  }
  else
  {
    xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR, parser->current.span,
                       "expected ABI string after extern");
  }
  XsSyntaxNode *declaration = node(parser, XS_SYNTAX_DECL_EXTERN_BLOCK, (XsSpan){start, parser->previous.span.end});
  attach_modifiers(parser, declaration, modifiers);
  reject_dispatch_modifiers(parser, declaration, "extern blocks");
  xs_syntax_node_add(parser->tree, declaration, extern_abi_token(parser, abi_span));
  expect(parser, XS_TOKEN_LEFT_BRACE, "expected '{' after extern ABI");
  while(parser->current.kind != XS_TOKEN_RIGHT_BRACE && parser->current.kind != XS_TOKEN_EOF)
  {
    size_t before = parser->current.span.start;
    Modifiers member = parse_modifiers(parser);
    if(accept(parser, XS_TOKEN_KW_FN) || accept(parser, XS_TOKEN_KW_OP))
    {
      bool referential_transparent = parser->previous.kind == XS_TOKEN_KW_OP;
      XsSyntaxNode *function = parse_function(parser, member, before, true, referential_transparent);
      validate_method_dispatch_modifiers(parser, function, false, false);
      function->flags |= XS_SYNTAX_FLAG_EXTERN | XS_SYNTAX_FLAG_INCOMPLETE;
      xs_syntax_node_add(parser->tree, function, extern_abi_token(parser, abi_span));
      if(xs_syntax_find_first(function, XS_SYNTAX_STMT_BLOCK) != nullptr)
        xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR,
                           (XsSpan){function->span.start_offset, function->span.end_offset},
                           "extern functions must not have bodies");
      xs_syntax_node_add(parser->tree, declaration, function);
    }
    else if(parser->current.kind == XS_TOKEN_KW_STATIC ||
            ((member.flags & XS_SYNTAX_FLAG_STATIC) != 0 && parser->current.kind == XS_TOKEN_IDENTIFIER))
    {
      xs_syntax_node_add(
          parser->tree, declaration,
          parse_extern_static(parser, member, before, abi_span, parser->current.kind != XS_TOKEN_KW_STATIC));
    }
    else
    {
      xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR, parser->current.span,
                         "expected extern function or static declaration");
      skip_forbidden_data_member(parser);
    }
    if(parser->current.span.start == before)
    {
      xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR, parser->current.span,
                         "parser made no progress in extern block");
      advance(parser);
    }
  }
  expect(parser, XS_TOKEN_RIGHT_BRACE, "expected '}' after extern block");
  finish_node(parser, declaration, parser->previous.span.end);
  return declaration;
}

static XsSyntaxNode *parse_enum(SyntaxParser *parser, Modifiers modifiers, size_t start)
{
  XsSyntaxNode *declaration = node(parser, XS_SYNTAX_DECL_ENUM, (XsSpan){start, parser->previous.span.end});
  attach_modifiers(parser, declaration, modifiers);
  reject_dispatch_modifiers(parser, declaration, "enum declarations");
  bool data_enum = accept(parser, XS_TOKEN_KW_DATA);
  if(data_enum)
    declaration->flags |= XS_SYNTAX_FLAG_DATA_ENUM;
  xs_syntax_node_add(parser->tree, declaration, identifier(parser));
  parse_generics(parser, declaration);
  if(data_enum)
    parse_base_list(parser, declaration);
  else if(parser->current.kind == XS_TOKEN_COLON)
  {
    xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR, parser->current.span,
                       "regular enum declarations cannot have base types");
    parse_base_list(parser, declaration);
  }
  expect(parser, XS_TOKEN_LEFT_BRACE, "expected '{' before enum variants");
  bool has_payload_variant = false;
  while(parser->current.kind != XS_TOKEN_RIGHT_BRACE && parser->current.kind != XS_TOKEN_EOF)
  {
    size_t start_variant = parser->current.span.start;
    XsSyntaxNode *variant = node(parser, XS_SYNTAX_ENUM_VARIANT, (XsSpan){start_variant, start_variant});
    xs_syntax_node_add(parser->tree, variant, identifier(parser));
    if(accept(parser, XS_TOKEN_COLON))
    {
      XsSpan colon_span = parser->previous.span;
      has_payload_variant = true;
      if(!data_enum)
        xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR, colon_span,
                           "regular enum variants cannot contain payload types");
      XsSyntaxNode *payload = parse_type(parser);
      if(data_enum && payload != nullptr && payload->kind == XS_SYNTAX_TYPE_TUPLE)
        xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR,
                           (XsSpan){payload->span.start_offset, payload->span.end_offset},
                           "data enum tuple payloads are not supported");
      xs_syntax_node_add(parser->tree, variant, payload);
    }
    finish_node(parser, variant, parser->previous.span.end);
    xs_syntax_node_add(parser->tree, declaration, variant);
    accept(parser, XS_TOKEN_COMMA);
  }
  expect(parser, XS_TOKEN_RIGHT_BRACE, "expected '}' after enum");
  if(data_enum && !has_payload_variant)
    xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR, (XsSpan){start, parser->previous.span.end},
                       "enum data declarations require at least one typed variant");
  validate_enum_variant_names(parser, declaration, data_enum);
  finish_node(parser, declaration, parser->previous.span.end);
  return declaration;
}

static void skip_forbidden_data_member(SyntaxParser *parser)
{
  size_t depth = 0;
  do
  {
    if(parser->current.kind == XS_TOKEN_LEFT_BRACE || parser->current.kind == XS_TOKEN_LEFT_PAREN ||
       parser->current.kind == XS_TOKEN_LEFT_BRACKET)
      ++depth;
    else if(parser->current.kind == XS_TOKEN_RIGHT_BRACE || parser->current.kind == XS_TOKEN_RIGHT_PAREN ||
            parser->current.kind == XS_TOKEN_RIGHT_BRACKET)
    {
      if(depth == 0)
        return;
      --depth;
    }
    advance(parser);
    if(depth == 0 && parser->previous.kind == XS_TOKEN_SEMICOLON)
      return;
  } while(parser->current.kind != XS_TOKEN_EOF && parser->current.kind != XS_TOKEN_RIGHT_BRACE);
}

static size_t callable_parameter_count(const XsSyntaxNode *callable)
{
  size_t count = 0;
  for(size_t index = 0; index < callable->child_count; ++index)
    count += callable->children[index]->kind == XS_SYNTAX_PARAMETER;
  return count;
}

static const XsSyntaxNode *callable_parameter_at(const XsSyntaxNode *callable, size_t wanted)
{
  for(size_t index = 0; index < callable->child_count; ++index)
  {
    if(callable->children[index]->kind != XS_SYNTAX_PARAMETER)
      continue;
    if(wanted == 0)
      return callable->children[index];
    --wanted;
  }
  return nullptr;
}

static bool callable_parameter_types_equal(const XsSyntaxNode *left, const XsSyntaxNode *right)
{
  const size_t count = callable_parameter_count(left);
  if(count != callable_parameter_count(right))
    return false;
  for(size_t index = 0; index < count; ++index)
  {
    const XsSyntaxNode *left_parameter = callable_parameter_at(left, index);
    const XsSyntaxNode *right_parameter = callable_parameter_at(right, index);
    if(left_parameter == nullptr || right_parameter == nullptr || left_parameter->child_count < 2 ||
       right_parameter->child_count < 2 ||
       !syntax_nodes_equal(left_parameter->children[1], right_parameter->children[1]))
      return false;
  }
  return true;
}

static bool callable_operator_equal(const XsSyntaxNode *left, const XsSyntaxNode *right)
{
  if((left->flags & XS_SYNTAX_FLAG_OPERATOR) != (right->flags & XS_SYNTAX_FLAG_OPERATOR))
    return false;
  if((left->flags & XS_SYNTAX_FLAG_OPERATOR) == 0)
    return true;
  for(size_t index = 0; index < left->child_count; ++index)
  {
    if(left->children[index]->kind != XS_SYNTAX_TOKEN)
      continue;
    for(size_t other = 0; other < right->child_count; ++other)
    {
      if(right->children[other]->kind == XS_SYNTAX_TOKEN)
        return left->children[index]->token_kind == right->children[other]->token_kind;
    }
  }
  return false;
}

static void validate_callables(SyntaxParser *parser, XsSyntaxNode *declaration)
{
  for(size_t index = 0; index < declaration->child_count; ++index)
  {
    XsSyntaxNode *callable = declaration->children[index];
    if(callable->kind != XS_SYNTAX_DECL_FUNCTION && callable->kind != XS_SYNTAX_CLASS_CONSTRUCTOR)
      continue;
    const XsSyntaxNode *name = first_child_kind(callable, XS_SYNTAX_IDENTIFIER);
    if(name == nullptr)
      continue;
    for(size_t previous_index = 0; previous_index < index; ++previous_index)
    {
      const XsSyntaxNode *previous = declaration->children[previous_index];
      const XsSyntaxNode *previous_name = first_child_kind(previous, XS_SYNTAX_IDENTIFIER);
      if(previous->kind != callable->kind || previous_name == nullptr ||
         !syntax_text_equal(name->text, previous_name->text) || !callable_operator_equal(callable, previous))
        continue;
      if(callable_parameter_types_equal(callable, previous))
        xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR,
                           (XsSpan){name->span.start_offset, name->span.end_offset},
                           "callable conflicts with an identical parameter type list");
      else
        callable->flags |= XS_SYNTAX_FLAG_OVERLOAD;
      break;
    }
  }
}

static XsSyntaxNode *parse_data(SyntaxParser *parser, Modifiers modifiers, size_t start)
{
  XsSyntaxNode *declaration = node(parser, XS_SYNTAX_DECL_DATA, (XsSpan){start, parser->previous.span.end});
  attach_modifiers(parser, declaration, modifiers);
  reject_dispatch_modifiers(parser, declaration, "data declarations");
  xs_syntax_node_add(parser->tree, declaration, identifier(parser));
  parse_generics(parser, declaration);
  parse_base_list(parser, declaration);
  const XsSyntaxNode *data_name = first_child_kind(declaration, XS_SYNTAX_IDENTIFIER);
  expect(parser, XS_TOKEN_LEFT_BRACE, "expected '{' before data members");
  while(parser->current.kind != XS_TOKEN_RIGHT_BRACE && parser->current.kind != XS_TOKEN_EOF)
  {
    size_t before = parser->current.span.start;
    Modifiers member = parse_modifiers(parser);
    if(accept(parser, XS_TOKEN_KW_FN) || accept(parser, XS_TOKEN_KW_OP))
    {
      bool referential_transparent = parser->previous.kind == XS_TOKEN_KW_OP;
      XsSyntaxNode *function = parse_function(parser, member, before, false, referential_transparent);
      validate_method_dispatch_modifiers(parser, function, false, false);
      xs_syntax_node_add(parser->tree, declaration, function);
    }
    else if(parser->current.kind == XS_TOKEN_IDENTIFIER && parser->next.kind == XS_TOKEN_LEFT_PAREN)
    {
      XsSyntaxNode *constructor = node(parser, XS_SYNTAX_CLASS_CONSTRUCTOR, (XsSpan){before, parser->current.span.end});
      XsSyntaxNode *constructor_name = identifier(parser);
      if(constructor_name != nullptr && data_name != nullptr &&
         !syntax_text_equal(data_name->text, constructor_name->text))
        xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR,
                           (XsSpan){constructor_name->span.start_offset, constructor_name->span.end_offset},
                           "constructor name must match the data name");
      attach_modifiers(parser, constructor, member);
      reject_dispatch_modifiers(parser, constructor, "constructors");
      xs_syntax_node_add(parser->tree, constructor, constructor_name);
      parse_parameters(parser, constructor);
      xs_syntax_node_add(parser->tree, constructor, parse_block(parser));
      finish_node(parser, constructor, parser->previous.span.end);
      xs_syntax_node_add(parser->tree, declaration, constructor);
    }
    else if(parser->current.kind == XS_TOKEN_IDENTIFIER && parser->next.kind == XS_TOKEN_COLON)
    {
      size_t start_field = parser->current.span.start;
      XsSyntaxNode *field = node(parser, XS_SYNTAX_DATA_FIELD, (XsSpan){start_field, start_field});
      attach_modifiers(parser, field, member);
      reject_dispatch_modifiers(parser, field, "data fields");
      xs_syntax_node_add(parser->tree, field, identifier(parser));
      expect(parser, XS_TOKEN_COLON, "expected ':' after data field name");
      xs_syntax_node_add(parser->tree, field, parse_type(parser));
      accept(parser, XS_TOKEN_SEMICOLON);
      finish_node(parser, field, parser->previous.span.end);
      xs_syntax_node_add(parser->tree, declaration, field);
    }
    else
    {
      xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR, parser->current.span,
                         "expected data field, constructor, or method");
      skip_forbidden_data_member(parser);
    }
    if(parser->current.span.start == before)
    {
      xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR, parser->current.span,
                         "parser made no progress in data members");
      advance(parser);
    }
  }
  expect(parser, XS_TOKEN_RIGHT_BRACE, "expected '}' after data declaration");
  validate_callables(parser, declaration);
  finish_node(parser, declaration, parser->previous.span.end);
  return declaration;
}

static void parse_base_list(SyntaxParser *parser, XsSyntaxNode *declaration)
{
  if(!accept(parser, XS_TOKEN_COLON))
    return;
  do
  {
    size_t start = parser->current.span.start;
    XsSyntaxNode *base = node(parser, XS_SYNTAX_BASE_SPECIFIER, (XsSpan){start, start});
    bool saw_access = false;
    bool saw_virtual = false;
    while(true)
    {
      if(accept(parser, XS_TOKEN_KW_VIRTUAL))
      {
        if(saw_virtual)
          xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR, parser->previous.span,
                             "duplicate virtual inheritance modifier");
        base->flags |= XS_SYNTAX_FLAG_VIRTUAL;
        saw_virtual = true;
      }
      else if(accept(parser, XS_TOKEN_KW_PUBLIC))
      {
        if(saw_access)
          xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR, parser->previous.span,
                             "base type may have only one access modifier");
        base->visibility = XS_SYNTAX_VISIBILITY_PUBLIC;
        saw_access = true;
      }
      else if(accept(parser, XS_TOKEN_KW_PROTECTED))
      {
        if(saw_access)
          xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR, parser->previous.span,
                             "base type may have only one access modifier");
        base->visibility = XS_SYNTAX_VISIBILITY_PROTECTED;
        saw_access = true;
      }
      else if(accept(parser, XS_TOKEN_KW_PRIVATE))
      {
        if(saw_access)
          xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR, parser->previous.span,
                             "base type may have only one access modifier");
        base->visibility = XS_SYNTAX_VISIBILITY_PRIVATE;
        saw_access = true;
      }
      else if(accept(parser, XS_TOKEN_KW_INTERNAL))
      {
        if(saw_access)
          xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR, parser->previous.span,
                             "base type may have only one access modifier");
        base->visibility = XS_SYNTAX_VISIBILITY_INTERNAL;
        saw_access = true;
      }
      else
        break;
    }
    if(!saw_access)
      base->visibility = XS_SYNTAX_VISIBILITY_INTERNAL;
    xs_syntax_node_add(parser->tree, base, parse_type(parser));
    finish_node(parser, base, parser->previous.span.end);
    xs_syntax_node_add(parser->tree, declaration, base);
  } while(accept(parser, XS_TOKEN_COMMA));
}

static XsSyntaxNode *parse_class(SyntaxParser *parser, Modifiers modifiers, size_t start, bool interface)
{
  XsSyntaxNode *declaration = node(parser, interface ? XS_SYNTAX_DECL_INTERFACE : XS_SYNTAX_DECL_CLASS,
                                   (XsSpan){start, parser->previous.span.end});
  attach_modifiers(parser, declaration, modifiers);
  validate_type_dispatch_modifiers(parser, declaration, interface);
  XsSyntaxNode *class_name = identifier(parser);
  xs_syntax_node_add(parser->tree, declaration, class_name);
  parse_generics(parser, declaration);
  parse_base_list(parser, declaration);
  expect(parser, XS_TOKEN_LEFT_BRACE, "expected '{' before declaration members");
  while(parser->current.kind != XS_TOKEN_RIGHT_BRACE && parser->current.kind != XS_TOKEN_EOF)
  {
    size_t before = parser->current.span.start;
    if(accept(parser, XS_TOKEN_KW_EXTENDS))
    {
      xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR, parser->previous.span,
                         "use the canonical ':' base-list syntax instead of extends");
      skip_forbidden_data_member(parser);
    }
    else if(accept(parser, XS_TOKEN_KW_IMPLEMENTS))
    {
      xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR, parser->previous.span,
                         "use the canonical ':' base-list syntax instead of implements");
      skip_forbidden_data_member(parser);
    }
    else
    {
      Modifiers member = parse_modifiers(parser);
      if(accept(parser, XS_TOKEN_KW_FN) || accept(parser, XS_TOKEN_KW_OP))
      {
        bool referential_transparent = parser->previous.kind == XS_TOKEN_KW_OP;
        XsSyntaxNode *function = parse_function(parser, member, before, interface, referential_transparent);
        validate_method_dispatch_modifiers(parser, function, true, interface);
        if(interface && (function->flags & XS_SYNTAX_FLAG_INCOMPLETE) == 0)
        {
          XsSpan function_span = {function->span.start_offset, function->span.end_offset};
          xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR, function_span,
                             "interface methods must be declarations without bodies");
        }
        xs_syntax_node_add(parser->tree, declaration, function);
      }
      else if(interface)
      {
        xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR, parser->current.span,
                           "interfaces may only contain function declarations");
        skip_forbidden_data_member(parser);
      }
      else if(accept(parser, XS_TOKEN_KW_CLASS))
        xs_syntax_node_add(parser->tree, declaration, parse_class(parser, member, before, false));
      else if(accept(parser, XS_TOKEN_KW_ENUM))
        xs_syntax_node_add(parser->tree, declaration, parse_enum(parser, member, before));
      else if(accept(parser, XS_TOKEN_KW_DATA))
        xs_syntax_node_add(parser->tree, declaration, parse_data(parser, member, before));
      else if(accept(parser, XS_TOKEN_KW_MACRO_RULES))
      {
        XsSyntaxNode *macro = parse_macro(parser, before);
        attach_modifiers(parser, macro, member);
        xs_syntax_node_add(parser->tree, declaration, macro);
      }
      else if(parser->current.kind == XS_TOKEN_IDENTIFIER && parser->next.kind == XS_TOKEN_BANG)
        xs_syntax_node_add(parser->tree, declaration, parse_declaration_macro_call(parser, before));
      else if(parser->current.kind == XS_TOKEN_IDENTIFIER && parser->next.kind == XS_TOKEN_COLON)
      {
        XsSyntaxNode *field = parse_variable(parser, true);
        field->kind = XS_SYNTAX_CLASS_FIELD;
        attach_modifiers(parser, field, member);
        reject_dispatch_modifiers(parser, field, "fields and properties");
        xs_syntax_node_add(parser->tree, declaration, field);
      }
      else if(parser->current.kind == XS_TOKEN_IDENTIFIER && parser->next.kind == XS_TOKEN_LEFT_PAREN)
      {
        XsSyntaxNode *constructor =
            node(parser, XS_SYNTAX_CLASS_CONSTRUCTOR, (XsSpan){before, parser->current.span.end});
        attach_modifiers(parser, constructor, member);
        reject_dispatch_modifiers(parser, constructor, "constructors");
        XsSyntaxNode *constructor_name = identifier(parser);
        XsSpan constructor_span = constructor_name == nullptr ? parser->previous.span
                                                              : (XsSpan){constructor_name->span.start_offset,
                                                                         constructor_name->span.end_offset};
        if(class_name != nullptr && constructor_name != nullptr &&
           !syntax_text_equal(class_name->text, constructor_name->text))
          xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR, constructor_span,
                             "constructor name must match the class name");
        xs_syntax_node_add(parser->tree, constructor, constructor_name);
        parse_parameters(parser, constructor);
        xs_syntax_node_add(parser->tree, constructor, parse_block(parser));
        finish_node(parser, constructor, parser->previous.span.end);
        xs_syntax_node_add(parser->tree, declaration, constructor);
      }
      else if(parser->current.kind == XS_TOKEN_IDENTIFIER && parser->next.kind == XS_TOKEN_DOT)
      {
        XsSyntaxNode *destructor = node(parser, XS_SYNTAX_CLASS_DESTRUCTOR, (XsSpan){before, parser->current.span.end});
        attach_modifiers(parser, destructor, member);
        reject_dispatch_modifiers(parser, destructor, "destructors");
        xs_syntax_node_add(parser->tree, destructor, identifier(parser));
        expect(parser, XS_TOKEN_DOT, "expected '.' in destructor declaration");
        if(parser->current.kind != XS_TOKEN_IDENTIFIER || !token_text_is(parser, parser->current, "Drop"))
          xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR, parser->current.span,
                             "destructor name must be Drop");
        xs_syntax_node_add(parser->tree, destructor, identifier(parser));
        expect(parser, XS_TOKEN_LEFT_PAREN, "expected '(' after destructor name");
        expect(parser, XS_TOKEN_RIGHT_PAREN, "destructors do not accept parameters");
        xs_syntax_node_add(parser->tree, destructor, parse_block(parser));
        finish_node(parser, destructor, parser->previous.span.end);
        xs_syntax_node_add(parser->tree, declaration, destructor);
      }
      else
      {
        xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR, parser->current.span,
                           "expected class or interface member");
        advance(parser);
      }
    }
    if(parser->current.span.start == before)
    {
      xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR, parser->current.span,
                         "parser made no progress in declaration members");
      advance(parser);
    }
  }
  expect(parser, XS_TOKEN_RIGHT_BRACE, "expected '}' after declaration members");
  validate_callables(parser, declaration);
  finish_node(parser, declaration, parser->previous.span.end);
  return declaration;
}

XsSyntaxNode *parse_declaration(SyntaxParser *parser, bool top_level)
{
  size_t start = parser->current.span.start;
  Modifiers modifiers = parse_modifiers(parser);
  if(accept(parser, XS_TOKEN_KW_NAMESPACE))
  {
    XsSyntaxNode *declaration = node(parser, XS_SYNTAX_DECL_NAMESPACE, (XsSpan){start, parser->previous.span.end});
    attach_modifiers(parser, declaration, modifiers);
    xs_syntax_node_add(parser->tree, declaration, parse_path(parser));
    if(!top_level)
      xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR, parser->previous.span,
                         "namespace declarations are only valid at file or namespace scope");
    if(accept(parser, XS_TOKEN_SEMICOLON))
    {
      if(parser->has_source_namespace)
        xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR, parser->previous.span,
                           "a file may contain only one source-scoped namespace");
      if(parser->seen_non_namespace)
        xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR, parser->previous.span,
                           "a source-scoped namespace must precede ordinary declarations");
      parser->has_source_namespace = true;
    }
    else if(accept(parser, XS_TOKEN_LEFT_BRACE))
    {
      declaration->flags |= XS_SYNTAX_FLAG_BLOCK_NAMESPACE;
      parser->seen_non_namespace = true;
      while(parser->current.kind != XS_TOKEN_RIGHT_BRACE && parser->current.kind != XS_TOKEN_EOF)
      {
        size_t before = parser->current.span.start;
        xs_syntax_node_add(parser->tree, declaration, parse_declaration(parser, true));
        if(parser->current.span.start == before)
        {
          xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR, parser->current.span,
                             "parser made no progress in namespace block");
          advance(parser);
        }
      }
      expect(parser, XS_TOKEN_RIGHT_BRACE, "expected '}' after namespace block");
    }
    else
    {
      xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR, parser->current.span,
                         "expected ';' or '{' after namespace path");
    }
    finish_node(parser, declaration, parser->previous.span.end);
    return declaration;
  }
  if(accept(parser, XS_TOKEN_KW_IMPORT))
    return parse_import(parser, false, start);
  if(accept(parser, XS_TOKEN_KW_USING))
    return parse_using(parser, start);
  if(parser->current.kind == XS_TOKEN_KW_EXTERN)
    return parse_extern_block(parser, modifiers, start);
  if(accept(parser, XS_TOKEN_KW_FN) || accept(parser, XS_TOKEN_KW_OP))
  {
    bool referential_transparent = parser->previous.kind == XS_TOKEN_KW_OP;
    XsSyntaxNode *function = parse_function(parser, modifiers, start, false, referential_transparent);
    validate_method_dispatch_modifiers(parser, function, false, false);
    return function;
  }
  if(accept(parser, XS_TOKEN_KW_CLASS))
    return parse_class(parser, modifiers, start, false);
  if(accept(parser, XS_TOKEN_KW_INTERFACE))
    return parse_class(parser, modifiers, start, true);
  if(accept(parser, XS_TOKEN_KW_ENUM))
    return parse_enum(parser, modifiers, start);
  if(accept(parser, XS_TOKEN_KW_DATA))
    return parse_data(parser, modifiers, start);
  if(accept(parser, XS_TOKEN_KW_MACRO_RULES))
  {
    XsSyntaxNode *macro = parse_macro(parser, start);
    attach_modifiers(parser, macro, modifiers);
    return macro;
  }
  if(parser->current.kind == XS_TOKEN_IDENTIFIER && parser->next.kind == XS_TOKEN_BANG)
    return parse_declaration_macro_call(parser, start);
  if(parser->current.kind == XS_TOKEN_KW_VAL || parser->current.kind == XS_TOKEN_KW_CONST ||
     parser->current.kind == XS_TOKEN_KW_ATOMIC ||
     (parser->current.kind == XS_TOKEN_IDENTIFIER &&
      (parser->next.kind == XS_TOKEN_COLON || parser->next.kind == XS_TOKEN_INFER_ASSIGN)))
  {
    XsSyntaxNode *variable = parse_variable(parser, true);
    attach_modifiers(parser, variable, modifiers);
    return variable;
  }
  xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR, parser->current.span,
                     top_level ? "top-level execution is not allowed; expected a declaration" : "expected declaration");
  if(parser->current.kind != XS_TOKEN_EOF)
    advance(parser);
  return node(parser, XS_SYNTAX_TOKEN, (XsSpan){start, parser->previous.span.end});
}
