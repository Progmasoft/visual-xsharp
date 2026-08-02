/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "parser_internal.h"

#include <stdio.h>

static XsSyntaxNode *parse_literal(SyntaxParser *parser);

static bool token_can_continue_expression_path(XsTokenKind kind)
{
    return kind == XS_TOKEN_IDENTIFIER || kind == XS_TOKEN_KW_ATOMIC || kind == XS_TOKEN_KW_NONE;
}

static XsSyntaxNode *expression_path_segment(SyntaxParser *parser)
{
    if(token_can_continue_expression_path(parser->current.kind))
    {
        XsToken token = parser->current;
        advance(parser);
        return node(parser, XS_SYNTAX_IDENTIFIER, token.span);
    }
    xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR, parser->current.span, "expected identifier");
    return nullptr;
}

static XsSyntaxNode *parse_expression_path(SyntaxParser *parser)
{
    size_t start = parser->current.span.start;
    XsSyntaxNode *path = node(parser, XS_SYNTAX_PATH, (XsSpan){start, start});
    XsSyntaxNode *segment = expression_path_segment(parser);
    if(segment == nullptr)
        return path;
    xs_syntax_node_add(parser->tree, path, segment);
    while(parser->current.kind == XS_TOKEN_DOUBLE_COLON && token_can_continue_expression_path(parser->next.kind))
    {
        advance(parser);
        segment = expression_path_segment(parser);
        if(segment == nullptr)
            break;
        xs_syntax_node_add(parser->tree, path, segment);
    }
    finish_node(parser, path, parser->previous.span.end);
    return path;
}

static bool generic_qualifier_follows(const SyntaxParser *parser)
{
    SyntaxParser lookahead = *parser;
    if(!accept(&lookahead, XS_TOKEN_LESS))
        return false;
    size_t depth = 1;
    while(depth != 0 && lookahead.current.kind != XS_TOKEN_EOF)
    {
        if(accept(&lookahead, XS_TOKEN_LESS))
            ++depth;
        else if(accept(&lookahead, XS_TOKEN_GREATER))
            --depth;
        else
            advance(&lookahead);
    }
    return depth == 0 && lookahead.current.kind == XS_TOKEN_DOUBLE_COLON &&
           token_can_continue_expression_path(lookahead.next.kind);
}

static XsSyntaxNode *parse_generic_qualifier(SyntaxParser *parser, XsSyntaxNode *path)
{
    size_t start = path->span.start_offset;
    XsSyntaxNode *qualifier = node(parser, XS_SYNTAX_EXPR_GENERIC_QUALIFIER, (XsSpan){start, path->span.end_offset});
    xs_syntax_node_add(parser->tree, qualifier, path);
    bool turbofish = accept(parser, XS_TOKEN_DOUBLE_COLON);
    if(turbofish)
        expect(parser, XS_TOKEN_LESS, "expected '<' after '::' in turbofish");
    else
        expect(parser, XS_TOKEN_LESS, "expected '<' in generic qualifier");
    if(parser->current.kind != XS_TOKEN_GREATER)
    {
        do
        {
            xs_syntax_node_add(parser->tree, qualifier, parse_type(parser));
        } while(accept(parser, XS_TOKEN_COMMA));
    }
    expect(parser, XS_TOKEN_GREATER, "expected '>' after generic arguments");
    if(!turbofish)
    {
        expect(parser, XS_TOKEN_DOUBLE_COLON, "expected '::' after generic type qualifier");
        xs_syntax_node_add(parser->tree, qualifier, parse_expression_path(parser));
    }
    finish_node(parser, qualifier, parser->previous.span.end);
    return qualifier;
}

static XsSyntaxNode *parse_member_identifier(SyntaxParser *parser)
{
    if(parser->current.kind == XS_TOKEN_IDENTIFIER || parser->current.kind == XS_TOKEN_INTEGER ||
       parser->current.kind == XS_TOKEN_KW_NONE)
    {
        XsToken token = parser->current;
        advance(parser);
        return node(parser, XS_SYNTAX_IDENTIFIER, token.span);
    }
    xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR, parser->current.span, "expected identifier");
    return nullptr;
}

static XsSyntaxNode *parse_tuple_expression_element(SyntaxParser *parser)
{
    if(parser->current.kind != XS_TOKEN_IDENTIFIER || parser->next.kind != XS_TOKEN_COLON)
        return parse_expression(parser, 1);
    size_t start = parser->current.span.start;
    XsSyntaxNode *field = node(parser, XS_SYNTAX_TUPLE_FIELD, (XsSpan){start, start});
    xs_syntax_node_add(parser->tree, field, identifier(parser));
    expect(parser, XS_TOKEN_COLON, "expected ':' after tuple field name");
    xs_syntax_node_add(parser->tree, field, parse_expression(parser, 1));
    finish_node(parser, field, parser->previous.span.end);
    return field;
}

static void parse_control_value(SyntaxParser *parser, XsSyntaxNode *parent, const char *owner)
{
    char close_message[64];
    snprintf(close_message, sizeof(close_message), "expected ')' after %s condition", owner);
    bool parenthesized = accept(parser, XS_TOKEN_LEFT_PAREN);
    bool previous_suppression = parser->suppress_typed_object_literal;
    parser->suppress_typed_object_literal = !parenthesized;
    xs_syntax_node_add(parser->tree, parent, parse_expression(parser, 1));
    parser->suppress_typed_object_literal = previous_suppression;
    if(parenthesized)
        expect(parser, XS_TOKEN_RIGHT_PAREN, close_message);
}

static XsSyntaxNode *parse_if_expression(SyntaxParser *parser, size_t start)
{
    expect(parser, XS_TOKEN_KW_IF, "expected 'if'");
    XsSyntaxNode *expression = node(parser, XS_SYNTAX_EXPR_IF, (XsSpan){start, parser->previous.span.end});
    parse_control_value(parser, expression, "if");
    xs_syntax_node_add(parser->tree, expression, parse_block(parser));
    if(!expect(parser, XS_TOKEN_KW_ELSE, "if expression requires else"))
    {
        finish_node(parser, expression, parser->previous.span.end);
        return expression;
    }
    xs_syntax_node_add(parser->tree, expression, parse_block(parser));
    finish_node(parser, expression, parser->previous.span.end);
    return expression;
}

static XsSyntaxNode *parse_match_expression(SyntaxParser *parser, size_t start)
{
    expect(parser, XS_TOKEN_KW_MATCH, "expected 'match'");
    XsSyntaxNode *expression = node(parser, XS_SYNTAX_EXPR_MATCH, (XsSpan){start, parser->previous.span.end});
    bool parenthesized = accept(parser, XS_TOKEN_LEFT_PAREN);
    bool previous_suppression = parser->suppress_typed_object_literal;
    parser->suppress_typed_object_literal = !parenthesized;
    xs_syntax_node_add(parser->tree, expression, parse_expression(parser, 1));
    parser->suppress_typed_object_literal = previous_suppression;
    if(parenthesized)
        expect(parser, XS_TOKEN_RIGHT_PAREN, "expected ')' after match value");
    expect(parser, XS_TOKEN_LEFT_BRACE, "expected '{' before match arms");
    while(parser->current.kind != XS_TOKEN_RIGHT_BRACE && parser->current.kind != XS_TOKEN_EOF)
    {
        size_t arm_start = parser->current.span.start;
        XsSyntaxNode *arm = node(parser, XS_SYNTAX_MATCH_ARM, (XsSpan){arm_start, arm_start});
        xs_syntax_node_add(parser->tree, arm, parse_pattern(parser));
        expect(parser, XS_TOKEN_ARROW, "expected '->' after match pattern");
        xs_syntax_node_add(parser->tree, arm, parse_block(parser));
        accept(parser, XS_TOKEN_COMMA);
        finish_node(parser, arm, parser->previous.span.end);
        xs_syntax_node_add(parser->tree, expression, arm);
    }
    expect(parser, XS_TOKEN_RIGHT_BRACE, "expected '}' after match arms");
    finish_node(parser, expression, parser->previous.span.end);
    return expression;
}

static void parse_expression_parameters(SyntaxParser *parser, XsSyntaxNode *function)
{
    expect(parser, XS_TOKEN_LEFT_PAREN, "expected '(' before function expression parameters");
    if(parser->current.kind != XS_TOKEN_RIGHT_PAREN)
    {
        do
        {
            size_t start = parser->current.span.start;
            XsSyntaxNode *parameter = node(parser, XS_SYNTAX_PARAMETER, (XsSpan){start, start});
            xs_syntax_node_add(parser->tree, parameter, identifier(parser));
            finish_node(parser, parameter, parser->previous.span.end);
            xs_syntax_node_add(parser->tree, function, parameter);
        } while(accept(parser, XS_TOKEN_COMMA));
    }
    expect(parser, XS_TOKEN_RIGHT_PAREN, "expected ')' after function expression parameters");
}

static XsSyntaxNode *parse_function_expression(SyntaxParser *parser, size_t start, bool move_capture, bool asynchronous)
{
    expect(parser, XS_TOKEN_KW_FN, "expected 'fn' in function expression");
    XsSyntaxNode *function = node(parser, XS_SYNTAX_EXPR_FUNCTION, (XsSpan){start, parser->previous.span.end});
    if(move_capture)
        function->flags |= XS_SYNTAX_FLAG_MOVE_CAPTURE;
    if(asynchronous)
        function->flags |= XS_SYNTAX_FLAG_ASYNC;
    parse_expression_parameters(parser, function);
    xs_syntax_node_add(parser->tree, function, parse_block(parser));
    finish_node(parser, function, parser->previous.span.end);
    return function;
}

static XsSyntaxNode *parse_untyped_pattern(SyntaxParser *parser)
{
    size_t start = parser->current.span.start;
    if(accept(parser, XS_TOKEN_KW_ELSE))
        return node(parser, XS_SYNTAX_PATTERN_ELSE, parser->previous.span);
    if(parser->current.kind == XS_TOKEN_INTEGER || parser->current.kind == XS_TOKEN_FLOAT ||
       parser->current.kind == XS_TOKEN_STRING || parser->current.kind == XS_TOKEN_CHARACTER ||
       parser->current.kind == XS_TOKEN_KW_TRUE || parser->current.kind == XS_TOKEN_KW_FALSE ||
       parser->current.kind == XS_TOKEN_KW_NONE)
    {
        XsSyntaxNode *pattern = node(parser, XS_SYNTAX_PATTERN_LITERAL, parser->current.span);
        xs_syntax_node_add(parser->tree, pattern, parse_literal(parser));
        finish_node(parser, pattern, parser->previous.span.end);
        return pattern;
    }
    if(accept(parser, XS_TOKEN_LEFT_PAREN))
    {
        XsSyntaxNode *tuple = node(parser, XS_SYNTAX_PATTERN_TUPLE, (XsSpan){start, parser->previous.span.end});
        if(parser->current.kind != XS_TOKEN_RIGHT_PAREN)
        {
            do
            {
                xs_syntax_node_add(parser->tree, tuple, parse_pattern(parser));
            } while(accept(parser, XS_TOKEN_COMMA));
        }
        expect(parser, XS_TOKEN_RIGHT_PAREN, "expected ')' after tuple pattern");
        finish_node(parser, tuple, parser->previous.span.end);
        return tuple;
    }
    if(parser->current.kind == XS_TOKEN_IDENTIFIER)
    {
        XsToken first = parser->current;
        XsSyntaxNode *path = parse_expression_path(parser);
        if(parser->current.kind == XS_TOKEN_LEFT_PAREN || path->child_count > 1)
        {
            XsSyntaxNode *variant =
                node(parser, XS_SYNTAX_PATTERN_ENUM_VARIANT, (XsSpan){start, path->span.end_offset});
            xs_syntax_node_add(parser->tree, variant, path);
            if(accept(parser, XS_TOKEN_LEFT_PAREN))
            {
                if(parser->current.kind != XS_TOKEN_RIGHT_PAREN)
                {
                    do
                    {
                        xs_syntax_node_add(parser->tree, variant, parse_pattern(parser));
                    } while(accept(parser, XS_TOKEN_COMMA));
                }
                expect(parser, XS_TOKEN_RIGHT_PAREN, "expected ')' after enum pattern bindings");
            }
            finish_node(parser, variant, parser->previous.span.end);
            return variant;
        }
        XsSyntaxNode *pattern = node(parser, XS_SYNTAX_PATTERN_IDENTIFIER, first.span);
        xs_syntax_node_add(parser->tree, pattern, path->children[0]);
        return pattern;
    }
    xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR, parser->current.span, "expected pattern");
    if(parser->current.kind != XS_TOKEN_EOF)
        advance(parser);
    return node(parser, XS_SYNTAX_PATTERN_ELSE, (XsSpan){start, parser->previous.span.end});
}

XsSyntaxNode *parse_pattern(SyntaxParser *parser)
{
    XsSyntaxNode *pattern = parse_untyped_pattern(parser);
    if(!accept(parser, XS_TOKEN_COLON))
        return pattern;
    XsSyntaxNode *typed =
        node(parser, XS_SYNTAX_PATTERN_TYPED, (XsSpan){pattern->span.start_offset, parser->previous.span.end});
    xs_syntax_node_add(parser->tree, typed, pattern);
    xs_syntax_node_add(parser->tree, typed, parse_type(parser));
    finish_node(parser, typed, parser->previous.span.end);
    return typed;
}

static XsSyntaxNode *parse_literal(SyntaxParser *parser)
{
    XsToken token = parser->current;
    advance(parser);
    XsSyntaxNode *literal = node(parser, XS_SYNTAX_EXPR_LITERAL, token.span);
    if(literal != nullptr)
        literal->token_kind = token.kind;
    return literal;
}

static XsSyntaxNode *parse_square_literal(SyntaxParser *parser)
{
    size_t start = parser->current.span.start;
    expect(parser, XS_TOKEN_LEFT_BRACKET, "expected '['");
    if(accept(parser, XS_TOKEN_RIGHT_BRACKET))
        return node(parser, XS_SYNTAX_EXPR_ARRAY_LITERAL, (XsSpan){start, parser->previous.span.end});
    if(accept(parser, XS_TOKEN_COLON))
    {
        XsSyntaxNode *empty = node(parser, XS_SYNTAX_EXPR_MAP_LITERAL, (XsSpan){start, parser->previous.span.end});
        expect(parser, XS_TOKEN_RIGHT_BRACKET, "expected ']' after empty map literal");
        finish_node(parser, empty, parser->previous.span.end);
        return empty;
    }
    XsSyntaxNode *first = parse_expression(parser, 1);
    if(accept(parser, XS_TOKEN_COLON))
    {
        XsSyntaxNode *map = node(parser, XS_SYNTAX_EXPR_MAP_LITERAL, (XsSpan){start, parser->previous.span.end});
        XsSyntaxNode *entry =
            node(parser, XS_SYNTAX_MAP_ENTRY, (XsSpan){first->span.start_offset, parser->previous.span.end});
        xs_syntax_node_add(parser->tree, entry, first);
        xs_syntax_node_add(parser->tree, entry, parse_expression(parser, 1));
        finish_node(parser, entry, parser->previous.span.end);
        xs_syntax_node_add(parser->tree, map, entry);
        while(accept(parser, XS_TOKEN_COMMA) && parser->current.kind != XS_TOKEN_RIGHT_BRACKET)
        {
            XsSyntaxNode *next = node(parser, XS_SYNTAX_MAP_ENTRY, parser->current.span);
            xs_syntax_node_add(parser->tree, next, parse_expression(parser, 1));
            expect(parser, XS_TOKEN_COLON, "expected ':' after map key");
            xs_syntax_node_add(parser->tree, next, parse_expression(parser, 1));
            finish_node(parser, next, parser->previous.span.end);
            xs_syntax_node_add(parser->tree, map, next);
        }
        expect(parser, XS_TOKEN_RIGHT_BRACKET, "expected ']' after map literal");
        finish_node(parser, map, parser->previous.span.end);
        return map;
    }
    XsSyntaxNode *array = node(parser, XS_SYNTAX_EXPR_ARRAY_LITERAL, (XsSpan){start, first->span.end_offset});
    xs_syntax_node_add(parser->tree, array, first);
    while(accept(parser, XS_TOKEN_COMMA) && parser->current.kind != XS_TOKEN_RIGHT_BRACKET)
        xs_syntax_node_add(parser->tree, array, parse_expression(parser, 1));
    expect(parser, XS_TOKEN_RIGHT_BRACKET, "expected ']' after array literal");
    finish_node(parser, array, parser->previous.span.end);
    return array;
}

static XsSyntaxNode *parse_brace_literal(SyntaxParser *parser, XsSyntaxNode *type_expression)
{
    size_t start = type_expression == nullptr ? parser->current.span.start : type_expression->span.start_offset;
    expect(parser, XS_TOKEN_LEFT_BRACE, "expected '{'");
    bool object = type_expression != nullptr ||
                  (parser->current.kind == XS_TOKEN_IDENTIFIER && parser->next.kind == XS_TOKEN_COLON);
    XsSyntaxKind kind = type_expression != nullptr
                            ? XS_SYNTAX_EXPR_TYPED_OBJECT_LITERAL
                            : (object ? XS_SYNTAX_EXPR_OBJECT_LITERAL : XS_SYNTAX_EXPR_SET_LITERAL);
    XsSyntaxNode *literal = node(parser, kind, (XsSpan){start, start});
    if(type_expression != nullptr)
        xs_syntax_node_add(parser->tree, literal, type_expression);
    while(parser->current.kind != XS_TOKEN_RIGHT_BRACE && parser->current.kind != XS_TOKEN_EOF)
    {
        size_t before = parser->current.span.start;
        if(object)
        {
            XsSyntaxNode *field = node(parser, XS_SYNTAX_OBJECT_FIELD, (XsSpan){before, before});
            xs_syntax_node_add(parser->tree, field, identifier(parser));
            expect(parser, XS_TOKEN_COLON, "expected ':' after object field name");
            xs_syntax_node_add(parser->tree, field, parse_expression(parser, 1));
            finish_node(parser, field, parser->previous.span.end);
            xs_syntax_node_add(parser->tree, literal, field);
            if(!accept(parser, XS_TOKEN_COMMA))
                accept(parser, XS_TOKEN_SEMICOLON);
        }
        else
        {
            xs_syntax_node_add(parser->tree, literal, parse_expression(parser, 1));
            if(!accept(parser, XS_TOKEN_COMMA))
                break;
        }
        if(parser->current.span.start == before)
        {
            xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR, parser->current.span,
                               "parser made no progress in literal");
            advance(parser);
        }
    }
    expect(parser, XS_TOKEN_RIGHT_BRACE, "expected '}' after literal");
    finish_node(parser, literal, parser->previous.span.end);
    return literal;
}

static XsSyntaxNode *parse_primary(SyntaxParser *parser)
{
    size_t start = parser->current.span.start;
    switch(parser->current.kind)
    {
    case XS_TOKEN_INTEGER:
    case XS_TOKEN_FLOAT:
    case XS_TOKEN_STRING:
    case XS_TOKEN_CHARACTER:
    case XS_TOKEN_KW_TRUE:
    case XS_TOKEN_KW_FALSE:
    case XS_TOKEN_KW_NONE:
        return parse_literal(parser);
    case XS_TOKEN_IDENTIFIER:
    case XS_TOKEN_KW_ATOMIC:
    {
        XsToken name = parser->current;
        XsSyntaxNode *path = parse_expression_path(parser);
        if(parser->current.kind == XS_TOKEN_BANG && parser->next.kind == XS_TOKEN_LEFT_PAREN)
        {
            advance(parser);
            XsSyntaxNode *call = node(parser, XS_SYNTAX_EXPR_MACRO_CALL, (XsSpan){start, parser->previous.span.end});
            xs_syntax_node_add(parser->tree, call, node(parser, XS_SYNTAX_IDENTIFIER, name.span));
            if(!expect(parser, XS_TOKEN_LEFT_PAREN, "macro calls require parentheses"))
                return call;
            size_t depth = 1;
            while(depth != 0 && parser->current.kind != XS_TOKEN_EOF)
            {
                XsToken token = parser->current;
                if(token.kind == XS_TOKEN_LEFT_PAREN)
                    ++depth;
                else if(token.kind == XS_TOKEN_RIGHT_PAREN)
                    --depth;
                advance(parser);
                if(depth != 0)
                {
                    XsSyntaxNode *argument = node(parser, XS_SYNTAX_TOKEN, token.span);
                    if(argument != nullptr)
                        argument->token_kind = token.kind;
                    xs_syntax_node_add(parser->tree, call, argument);
                }
            }
            finish_node(parser, call, parser->previous.span.end);
            return call;
        }
        XsSyntaxNode *value = path;
        if((parser->current.kind == XS_TOKEN_DOUBLE_COLON && parser->next.kind == XS_TOKEN_LESS) ||
           (parser->current.kind == XS_TOKEN_LESS && generic_qualifier_follows(parser)))
            value = parse_generic_qualifier(parser, path);
        if(value != path)
            return value;
        XsSyntaxNode *expression =
            node(parser, XS_SYNTAX_EXPR_IDENTIFIER, (XsSpan){value->span.start_offset, value->span.end_offset});
        if(path->child_count == 1 && path->children[0]->kind == XS_SYNTAX_IDENTIFIER)
            xs_syntax_node_add(parser->tree, expression, path->children[0]);
        else
            xs_syntax_node_add(parser->tree, expression, value);
        return expression;
    }
    case XS_TOKEN_LEFT_PAREN:
    {
        advance(parser);
        if(accept(parser, XS_TOKEN_RIGHT_PAREN))
            return node(parser, XS_SYNTAX_EXPR_TUPLE, (XsSpan){start, parser->previous.span.end});
        XsSyntaxNode *first = parse_tuple_expression_element(parser);
        if(!accept(parser, XS_TOKEN_COMMA))
        {
            expect(parser, XS_TOKEN_RIGHT_PAREN, "expected ')' after expression");
            if(first != nullptr && first->kind == XS_SYNTAX_TUPLE_FIELD)
            {
                XsSyntaxNode *tuple = node(parser, XS_SYNTAX_EXPR_TUPLE, (XsSpan){start, start});
                xs_syntax_node_add(parser->tree, tuple, first);
                finish_node(parser, tuple, parser->previous.span.end);
                return tuple;
            }
            return first;
        }
        XsSyntaxNode *tuple = node(parser, XS_SYNTAX_EXPR_TUPLE, (XsSpan){start, start});
        bool named = first != nullptr && first->kind == XS_SYNTAX_TUPLE_FIELD;
        xs_syntax_node_add(parser->tree, tuple, first);
        do
        {
            if(parser->current.kind == XS_TOKEN_RIGHT_PAREN)
                break;
            XsSyntaxNode *element = parse_tuple_expression_element(parser);
            if(element != nullptr && (element->kind == XS_SYNTAX_TUPLE_FIELD) != named)
                xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR,
                                   (XsSpan){element->span.start_offset, element->span.end_offset},
                                   "tuple expression cannot mix named and positional elements");
            xs_syntax_node_add(parser->tree, tuple, element);
        } while(accept(parser, XS_TOKEN_COMMA));
        expect(parser, XS_TOKEN_RIGHT_PAREN, "expected ')' after tuple expression");
        finish_node(parser, tuple, parser->previous.span.end);
        return tuple;
    }
    case XS_TOKEN_LEFT_BRACE:
        return parse_brace_literal(parser, nullptr);
    case XS_TOKEN_LEFT_BRACKET:
        return parse_square_literal(parser);
    case XS_TOKEN_KW_FN:
        return parse_function_expression(parser, start, false, false);
    case XS_TOKEN_KW_IF:
        return parse_if_expression(parser, start);
    case XS_TOKEN_KW_MATCH:
        return parse_match_expression(parser, start);
    default:
        xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR, parser->current.span, "expected expression");
        if(parser->current.kind != XS_TOKEN_EOF)
            advance(parser);
        return node(parser, XS_SYNTAX_EXPR_LITERAL, (XsSpan){start, parser->previous.span.end});
    }
}

static XsSyntaxNode *parse_postfix(SyntaxParser *parser);

static bool split_positional_member_token(const SyntaxParser *parser, XsSpan *first, XsSpan *second)
{
    if(parser->current.kind != XS_TOKEN_FLOAT)
        return false;
    XsSpan span = parser->current.span;
    const char *text = parser->tree->source->text;
    size_t separator = span.end;
    for(size_t index = span.start; index < span.end; ++index)
    {
        char character = text[index];
        if(character == '.')
        {
            if(separator != span.end)
                return false;
            separator = index;
        }
        else if(character < '0' || character > '9')
            return false;
    }
    if(separator == span.start || separator + 1 >= span.end)
        return false;
    *first = (XsSpan){span.start, separator};
    *second = (XsSpan){separator + 1, span.end};
    return true;
}

static XsSyntaxNode *append_positional_member(SyntaxParser *parser, XsSyntaxNode *receiver, size_t start,
                                              XsSpan selector)
{
    XsSyntaxNode *member = node(parser, XS_SYNTAX_EXPR_MEMBER_ACCESS, (XsSpan){start, selector.end});
    xs_syntax_node_add(parser->tree, member, receiver);
    xs_syntax_node_add(parser->tree, member, node(parser, XS_SYNTAX_IDENTIFIER, selector));
    finish_node(parser, member, selector.end);
    return member;
}

static XsSyntaxNode *parse_prefix(SyntaxParser *parser)
{
    size_t start = parser->current.span.start;
    XsTokenKind operator_kind = parser->current.kind;
    XsSyntaxKind kind;
    switch(operator_kind)
    {
    case XS_TOKEN_BANG:
    case XS_TOKEN_PLUS:
    case XS_TOKEN_MINUS:
    case XS_TOKEN_PLUS_PLUS:
    case XS_TOKEN_MINUS_MINUS:
        kind = XS_SYNTAX_EXPR_UNARY;
        break;
    case XS_TOKEN_STAR:
        kind = XS_SYNTAX_EXPR_DEREFERENCE;
        break;
    case XS_TOKEN_KW_AWAIT:
        kind = XS_SYNTAX_EXPR_AWAIT;
        break;
    case XS_TOKEN_KW_MOVE:
    {
        advance(parser);
        bool asynchronous = accept(parser, XS_TOKEN_KW_ASYNC);
        if(parser->current.kind == XS_TOKEN_KW_FN)
            return parse_function_expression(parser, start, true, asynchronous);
        if(asynchronous)
            xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR, parser->current.span,
                               "expected 'fn' after 'move async'");
        kind = XS_SYNTAX_EXPR_MOVE;
        break;
    }
    case XS_TOKEN_KW_ASYNC:
        advance(parser);
        if(parser->current.kind == XS_TOKEN_KW_FN)
            return parse_function_expression(parser, start, false, true);
        xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR, parser->current.span,
                           "expected 'fn' after 'async'");
        kind = XS_SYNTAX_EXPR_AWAIT;
        break;
    case XS_TOKEN_AMPERSAND:
        kind = XS_SYNTAX_EXPR_BORROW;
        break;
    case XS_TOKEN_KW_NEW:
        advance(parser);
        XsSyntaxNode *created = node(parser, XS_SYNTAX_EXPR_NEW, (XsSpan){start, parser->previous.span.end});
        if(parser->current.kind != XS_TOKEN_LEFT_PAREN)
            xs_syntax_node_add(parser->tree, created, parse_type(parser));
        else if(!parser->allow_untyped_new)
            xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR, parser->current.span,
                               "expected constructed type after 'new'");
        expect(parser, XS_TOKEN_LEFT_PAREN, "expected '(' after new");
        if(parser->current.kind != XS_TOKEN_RIGHT_PAREN)
        {
            do
            {
                xs_syntax_node_add(parser->tree, created, parse_expression(parser, 1));
            } while(accept(parser, XS_TOKEN_COMMA));
        }
        expect(parser, XS_TOKEN_RIGHT_PAREN, "expected ')' after new arguments");
        finish_node(parser, created, parser->previous.span.end);
        return created;
    default:
        return parse_primary(parser);
    }
    if(operator_kind != XS_TOKEN_KW_MOVE && operator_kind != XS_TOKEN_KW_ASYNC)
        advance(parser);
    if(kind == XS_SYNTAX_EXPR_BORROW && accept(parser, XS_TOKEN_KW_MUT))
        kind = XS_SYNTAX_EXPR_MUTABLE_BORROW;
    XsSyntaxNode *expression = node(parser, kind, (XsSpan){start, parser->previous.span.end});
    expression->token_kind = operator_kind;
    if(operator_kind == XS_TOKEN_PLUS_PLUS || operator_kind == XS_TOKEN_MINUS_MINUS)
        expression->flags |= XS_SYNTAX_FLAG_PREFIX_UPDATE;
    // Postfix operations bind more tightly than prefix operations. Parsing the
    // operand through the postfix layer makes `!value.member` mean
    // `!(value.member)` while consecutive prefix operators remain recursive.
    xs_syntax_node_add(parser->tree, expression, parse_postfix(parser));
    finish_node(parser, expression, parser->previous.span.end);
    return expression;
}

static XsSyntaxNode *parse_postfix(SyntaxParser *parser)
{
    XsSyntaxNode *expression = parse_prefix(parser);
    for(;;)
    {
        size_t start = expression == nullptr ? parser->current.span.start : expression->span.start_offset;
        if(accept(parser, XS_TOKEN_LEFT_PAREN))
        {
            XsSyntaxNode *call = node(parser, XS_SYNTAX_EXPR_CALL, (XsSpan){start, parser->previous.span.end});
            xs_syntax_node_add(parser->tree, call, expression);
            if(parser->current.kind != XS_TOKEN_RIGHT_PAREN)
            {
                do
                {
                    xs_syntax_node_add(parser->tree, call, parse_expression(parser, 1));
                } while(accept(parser, XS_TOKEN_COMMA));
            }
            expect(parser, XS_TOKEN_RIGHT_PAREN, "expected ')' after call arguments");
            finish_node(parser, call, parser->previous.span.end);
            expression = call;
        }
        else if(accept(parser, XS_TOKEN_DOT))
        {
            XsSpan first_selector;
            XsSpan second_selector;
            if(split_positional_member_token(parser, &first_selector, &second_selector))
            {
                advance(parser);
                expression = append_positional_member(parser, expression, start, first_selector);
                expression = append_positional_member(parser, expression, start, second_selector);
                continue;
            }
            XsSyntaxNode *name = parse_member_identifier(parser);
            XsSyntaxKind kind =
                parser->current.kind == XS_TOKEN_LEFT_PAREN ? XS_SYNTAX_EXPR_METHOD_CALL : XS_SYNTAX_EXPR_MEMBER_ACCESS;
            XsSyntaxNode *member = node(parser, kind, (XsSpan){start, parser->previous.span.end});
            xs_syntax_node_add(parser->tree, member, expression);
            xs_syntax_node_add(parser->tree, member, name);
            if(kind == XS_SYNTAX_EXPR_METHOD_CALL)
            {
                advance(parser);
                if(parser->current.kind != XS_TOKEN_RIGHT_PAREN)
                {
                    do
                    {
                        xs_syntax_node_add(parser->tree, member, parse_expression(parser, 1));
                    } while(accept(parser, XS_TOKEN_COMMA));
                }
                expect(parser, XS_TOKEN_RIGHT_PAREN, "expected ')' after method arguments");
            }
            finish_node(parser, member, parser->previous.span.end);
            expression = member;
        }
        else if(accept(parser, XS_TOKEN_QUESTION_DOT))
        {
            XsSyntaxNode *name = parse_member_identifier(parser);
            XsSyntaxKind kind = parser->current.kind == XS_TOKEN_LEFT_PAREN ? XS_SYNTAX_EXPR_OPTIONAL_METHOD_CALL
                                                                            : XS_SYNTAX_EXPR_OPTIONAL_MEMBER_ACCESS;
            XsSyntaxNode *member = node(parser, kind, (XsSpan){start, parser->previous.span.end});
            xs_syntax_node_add(parser->tree, member, expression);
            xs_syntax_node_add(parser->tree, member, name);
            if(kind == XS_SYNTAX_EXPR_OPTIONAL_METHOD_CALL)
            {
                advance(parser);
                if(parser->current.kind != XS_TOKEN_RIGHT_PAREN)
                {
                    do
                    {
                        xs_syntax_node_add(parser->tree, member, parse_expression(parser, 1));
                    } while(accept(parser, XS_TOKEN_COMMA));
                }
                expect(parser, XS_TOKEN_RIGHT_PAREN, "expected ')' after optional method arguments");
            }
            finish_node(parser, member, parser->previous.span.end);
            expression = member;
        }
        else if(accept(parser, XS_TOKEN_BANG))
        {
            XsSyntaxNode *forgiving =
                node(parser, XS_SYNTAX_EXPR_OPTIONAL_FORGIVING, (XsSpan){start, parser->previous.span.end});
            forgiving->token_kind = XS_TOKEN_BANG;
            xs_syntax_node_add(parser->tree, forgiving, expression);
            finish_node(parser, forgiving, parser->previous.span.end);
            expression = forgiving;
        }
        else if(accept(parser, XS_TOKEN_AT))
        {
            XsSyntaxNode *propagation =
                node(parser, XS_SYNTAX_EXPR_RESULT_PROPAGATION, (XsSpan){start, parser->previous.span.end});
            propagation->token_kind = XS_TOKEN_AT;
            xs_syntax_node_add(parser->tree, propagation, expression);
            finish_node(parser, propagation, parser->previous.span.end);
            expression = propagation;
        }
        else if(accept(parser, XS_TOKEN_LEFT_BRACKET))
        {
            XsSyntaxNode *index = node(parser, XS_SYNTAX_EXPR_INDEX, (XsSpan){start, parser->previous.span.end});
            xs_syntax_node_add(parser->tree, index, expression);
            xs_syntax_node_add(parser->tree, index, parse_expression(parser, 1));
            expect(parser, XS_TOKEN_RIGHT_BRACKET, "expected ']' after index");
            finish_node(parser, index, parser->previous.span.end);
            expression = index;
        }
        else if(parser->current.kind == XS_TOKEN_PLUS_PLUS || parser->current.kind == XS_TOKEN_MINUS_MINUS)
        {
            XsTokenKind operator_kind = parser->current.kind;
            advance(parser);
            XsSyntaxNode *update = node(parser, XS_SYNTAX_EXPR_UNARY, (XsSpan){start, parser->previous.span.end});
            update->token_kind = operator_kind;
            xs_syntax_node_add(parser->tree, update, expression);
            finish_node(parser, update, parser->previous.span.end);
            expression = update;
        }
        else if(!parser->suppress_typed_object_literal && parser->current.kind == XS_TOKEN_LEFT_BRACE &&
                expression != nullptr &&
                (expression->kind == XS_SYNTAX_EXPR_IDENTIFIER || expression->kind == XS_SYNTAX_EXPR_GENERIC_QUALIFIER))
        {
            expression = parse_brace_literal(parser, expression);
        }
        else
        {
            break;
        }
    }
    return expression;
}

static unsigned precedence(XsTokenKind kind)
{
    switch(kind)
    {
    case XS_TOKEN_ASSIGN:
    case XS_TOKEN_PLUS_ASSIGN:
    case XS_TOKEN_MINUS_ASSIGN:
    case XS_TOKEN_STAR_ASSIGN:
    case XS_TOKEN_SLASH_ASSIGN:
    case XS_TOKEN_PERCENT_ASSIGN:
    case XS_TOKEN_AMPERSAND_ASSIGN:
    case XS_TOKEN_PIPE_ASSIGN:
    case XS_TOKEN_CARET_ASSIGN:
    case XS_TOKEN_QUESTION_QUESTION_ASSIGN:
        return 1;
    case XS_TOKEN_QUESTION_QUESTION:
        return 2;
    case XS_TOKEN_LOGICAL_OR:
        return 3;
    case XS_TOKEN_LOGICAL_AND:
        return 4;
    case XS_TOKEN_PIPE:
        return 5;
    case XS_TOKEN_CARET:
        return 6;
    case XS_TOKEN_AMPERSAND:
        return 7;
    case XS_TOKEN_EQUAL:
    case XS_TOKEN_NOT_EQUAL:
        return 8;
    case XS_TOKEN_GREATER:
    case XS_TOKEN_GREATER_EQUAL:
    case XS_TOKEN_LESS:
    case XS_TOKEN_LESS_EQUAL:
        return 9;
    case XS_TOKEN_SHIFT_LEFT:
    case XS_TOKEN_SHIFT_RIGHT:
        return 10;
    case XS_TOKEN_PLUS:
    case XS_TOKEN_MINUS:
        return 11;
    case XS_TOKEN_STAR:
    case XS_TOKEN_SLASH:
    case XS_TOKEN_PERCENT:
        return 12;
    default:
        return 0;
    }
}

XsSyntaxNode *parse_expression(SyntaxParser *parser, unsigned minimum_precedence)
{
    XsSyntaxNode *left = parse_postfix(parser);
    for(;;)
    {
        XsTokenKind operator_kind = parser->current.kind;
        unsigned current_precedence = precedence(operator_kind);
        if(current_precedence < minimum_precedence)
            break;
        XsToken operator_token = parser->current;
        advance(parser);
        bool assignment = current_precedence == 1;
        bool right_associative = assignment || operator_kind == XS_TOKEN_QUESTION_QUESTION;
        XsSyntaxNode *right = parse_expression(parser, right_associative ? current_precedence : current_precedence + 1);
        XsSyntaxNode *combined = node(parser, assignment ? XS_SYNTAX_EXPR_ASSIGNMENT : XS_SYNTAX_EXPR_BINARY,
                                      (XsSpan){left->span.start_offset, right->span.end_offset});
        combined->token_kind = operator_kind;
        xs_syntax_node_add(parser->tree, combined, left);
        XsSyntaxNode *operator_node = node(parser, XS_SYNTAX_TOKEN, operator_token.span);
        operator_node->token_kind = operator_kind;
        xs_syntax_node_add(parser->tree, combined, operator_node);
        xs_syntax_node_add(parser->tree, combined, right);
        left = combined;
    }
    return left;
}
