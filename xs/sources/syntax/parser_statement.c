/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "parser_internal.h"

static void parse_property_accessors(SyntaxParser *parser, XsSyntaxNode *declaration)
{
    if(!accept(parser, XS_TOKEN_LEFT_BRACE))
        return;
    while(parser->current.kind != XS_TOKEN_RIGHT_BRACE && parser->current.kind != XS_TOKEN_EOF)
    {
        size_t start = parser->current.span.start;
        if(parser->current.kind != XS_TOKEN_KW_GETTER && parser->current.kind != XS_TOKEN_KW_SETTER)
        {
            xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR, parser->current.span,
                               "expected getter or setter property accessor");
            advance(parser);
            continue;
        }
        XsTokenKind accessor_kind = parser->current.kind;
        advance(parser);
        XsSyntaxNode *accessor = node(parser, XS_SYNTAX_PROPERTY_ACCESSOR, (XsSpan){start, parser->previous.span.end});
        accessor->token_kind = accessor_kind;
        if(parser->current.kind == XS_TOKEN_LEFT_BRACE)
            xs_syntax_node_add(parser->tree, accessor, parse_block(parser));
        else
            expect(parser, XS_TOKEN_SEMICOLON, "expected ';' after property accessor");
        finish_node(parser, accessor, parser->previous.span.end);
        xs_syntax_node_add(parser->tree, declaration, accessor);
    }
    expect(parser, XS_TOKEN_RIGHT_BRACE, "expected '}' after property accessors");
}

XsSyntaxNode *parse_variable(SyntaxParser *parser, bool require_semicolon)
{
    size_t start = parser->current.span.start;
    uint32_t flags = 0;
    if(accept(parser, XS_TOKEN_KW_VAL))
        flags |= XS_SYNTAX_FLAG_IMMUTABLE;
    else if(accept(parser, XS_TOKEN_KW_CONST))
        flags |= XS_SYNTAX_FLAG_CONSTANT;
    else if(accept(parser, XS_TOKEN_KW_STATIC))
        flags |= XS_SYNTAX_FLAG_STATIC_CONSTANT;
    else if(accept(parser, XS_TOKEN_KW_ATOMIC))
        flags |= XS_SYNTAX_FLAG_STATIC;
    XsSyntaxNode *declaration = node(parser, XS_SYNTAX_DECL_VARIABLE, (XsSpan){start, start});
    declaration->flags = flags;
    xs_syntax_node_add(parser->tree, declaration, identifier(parser));
    if(accept(parser, XS_TOKEN_INFER_ASSIGN))
    {
        declaration->flags |= XS_SYNTAX_FLAG_INFERRED_TYPE;
        xs_syntax_node_add(parser->tree, declaration, parse_expression(parser, 1));
        if(require_semicolon)
            expect(parser, XS_TOKEN_SEMICOLON, "expected ';' after inferred variable declaration");
        finish_node(parser, declaration, parser->previous.span.end);
        return declaration;
    }
    expect(parser, XS_TOKEN_COLON, "type annotation is required after variable name");
    xs_syntax_node_add(parser->tree, declaration, parse_type(parser));
    if(accept(parser, XS_TOKEN_ASSIGN))
        xs_syntax_node_add(parser->tree, declaration, parse_expression(parser, 1));
    if(parser->current.kind == XS_TOKEN_LEFT_BRACE)
        parse_property_accessors(parser, declaration);
    else if(require_semicolon)
        expect(parser, XS_TOKEN_SEMICOLON, "expected ';' after variable declaration");
    finish_node(parser, declaration, parser->previous.span.end);
    return declaration;
}

XsSyntaxNode *parse_block(SyntaxParser *parser)
{
    size_t start = parser->current.span.start;
    if(!expect(parser, XS_TOKEN_LEFT_BRACE, "expected '{' before block"))
        return node(parser, XS_SYNTAX_STMT_BLOCK, (XsSpan){start, start});
    XsSyntaxNode *block = node(parser, XS_SYNTAX_STMT_BLOCK, (XsSpan){start, parser->previous.span.end});
    while(parser->current.kind != XS_TOKEN_RIGHT_BRACE && parser->current.kind != XS_TOKEN_EOF)
    {
        size_t before = parser->current.span.start;
        xs_syntax_node_add(parser->tree, block, parse_statement(parser));
        if(parser->current.span.start == before)
        {
            xs_diagnostics_add(parser->diagnostics, XS_DIAGNOSTIC_ERROR, parser->current.span,
                               "parser made no progress in block");
            advance(parser);
        }
    }
    expect(parser, XS_TOKEN_RIGHT_BRACE, "expected '}' after block");
    finish_node(parser, block, parser->previous.span.end);
    return block;
}

static XsSyntaxNode *parse_if(SyntaxParser *parser)
{
    size_t start = parser->previous.span.start;
    XsSyntaxNode *statement = node(parser, XS_SYNTAX_STMT_IF, (XsSpan){start, parser->previous.span.end});
    bool parenthesized = accept(parser, XS_TOKEN_LEFT_PAREN);
    bool previous_suppression = parser->suppress_typed_object_literal;
    parser->suppress_typed_object_literal = !parenthesized;
    xs_syntax_node_add(parser->tree, statement, parse_expression(parser, 1));
    parser->suppress_typed_object_literal = previous_suppression;
    if(parenthesized)
        expect(parser, XS_TOKEN_RIGHT_PAREN, "expected ')' after if condition");
    xs_syntax_node_add(parser->tree, statement, parse_block(parser));
    while(parser->current.kind == XS_TOKEN_KW_ELSE && parser->next.kind != XS_TOKEN_COLON)
    {
        advance(parser);
        if(accept(parser, XS_TOKEN_KW_IF))
        {
            XsSyntaxNode *branch =
                node(parser, XS_SYNTAX_STMT_ELSE_IF, (XsSpan){parser->previous.span.start, parser->previous.span.end});
            parenthesized = accept(parser, XS_TOKEN_LEFT_PAREN);
            previous_suppression = parser->suppress_typed_object_literal;
            parser->suppress_typed_object_literal = !parenthesized;
            xs_syntax_node_add(parser->tree, branch, parse_expression(parser, 1));
            parser->suppress_typed_object_literal = previous_suppression;
            if(parenthesized)
                expect(parser, XS_TOKEN_RIGHT_PAREN, "expected ')' after else-if condition");
            xs_syntax_node_add(parser->tree, branch, parse_block(parser));
            finish_node(parser, branch, parser->previous.span.end);
            xs_syntax_node_add(parser->tree, statement, branch);
        }
        else
        {
            xs_syntax_node_add(parser->tree, statement, parse_block(parser));
            break;
        }
    }
    finish_node(parser, statement, parser->previous.span.end);
    return statement;
}

static XsSyntaxNode *parse_discard(SyntaxParser *parser, size_t start)
{
    XsSyntaxNode *statement = node(parser, XS_SYNTAX_STMT_DISCARD, (XsSpan){start, parser->previous.span.end});
    expect(parser, XS_TOKEN_COLON, "expected ':' after else discard marker");
    bool previous_allow_untyped_new = parser->allow_untyped_new;
    parser->allow_untyped_new = true;
    xs_syntax_node_add(parser->tree, statement, parse_expression(parser, 1));
    parser->allow_untyped_new = previous_allow_untyped_new;
    expect(parser, XS_TOKEN_SEMICOLON, "expected ';' after else discard statement");
    finish_node(parser, statement, parser->previous.span.end);
    return statement;
}

static bool for_header_is_each(const SyntaxParser *parser)
{
    SyntaxParser lookahead = *parser;
    size_t depth = 0;
    while(lookahead.current.kind != XS_TOKEN_EOF)
    {
        if(lookahead.current.kind == XS_TOKEN_LEFT_PAREN)
            ++depth;
        else if(lookahead.current.kind == XS_TOKEN_RIGHT_PAREN)
        {
            if(depth == 0)
                return false;
            --depth;
        }
        else if(lookahead.current.kind == XS_TOKEN_KW_IN && depth == 0)
        {
            return true;
        }
        else if(lookahead.current.kind == XS_TOKEN_SEMICOLON && depth == 0)
        {
            return false;
        }
        else if(lookahead.current.kind == XS_TOKEN_LEFT_BRACE && depth == 0)
        {
            return false;
        }
        advance(&lookahead);
    }
    return false;
}

static bool starts_pattern_variable(const SyntaxParser *parser)
{
    if(parser->current.kind != XS_TOKEN_LEFT_PAREN)
        return false;
    SyntaxParser lookahead = *parser;
    size_t depth = 0;
    do
    {
        if(lookahead.current.kind == XS_TOKEN_LEFT_PAREN)
            ++depth;
        else if(lookahead.current.kind == XS_TOKEN_RIGHT_PAREN)
            --depth;
        advance(&lookahead);
    } while(depth != 0 && lookahead.current.kind != XS_TOKEN_EOF);
    return depth == 0 && (lookahead.current.kind == XS_TOKEN_COLON || lookahead.current.kind == XS_TOKEN_INFER_ASSIGN);
}

static XsSyntaxNode *parse_pattern_variable(SyntaxParser *parser)
{
    size_t start = parser->current.span.start;
    XsSyntaxNode *declaration = node(parser, XS_SYNTAX_DECL_PATTERN_VARIABLE, (XsSpan){start, start});
    xs_syntax_node_add(parser->tree, declaration, parse_pattern(parser));
    if(accept(parser, XS_TOKEN_INFER_ASSIGN))
    {
        declaration->flags |= XS_SYNTAX_FLAG_INFERRED_TYPE;
        xs_syntax_node_add(parser->tree, declaration, parse_expression(parser, 1));
    }
    else if(accept(parser, XS_TOKEN_ASSIGN))
    {
        xs_syntax_node_add(parser->tree, declaration, parse_expression(parser, 1));
    }
    expect(parser, XS_TOKEN_SEMICOLON, "expected ';' after pattern variable declaration");
    finish_node(parser, declaration, parser->previous.span.end);
    return declaration;
}

static XsSyntaxNode *parse_for(SyntaxParser *parser, size_t start)
{
    bool parenthesized = accept(parser, XS_TOKEN_LEFT_PAREN);
    bool each = for_header_is_each(parser);
    XsSyntaxNode *statement =
        node(parser, each ? XS_SYNTAX_STMT_FOR_EACH : XS_SYNTAX_STMT_FOR, (XsSpan){start, parser->previous.span.end});
    if(each)
    {
        xs_syntax_node_add(parser->tree, statement, parse_pattern(parser));
        expect(parser, XS_TOKEN_KW_IN, "expected in after for-each pattern");
        bool previous_suppression = parser->suppress_typed_object_literal;
        parser->suppress_typed_object_literal = !parenthesized;
        xs_syntax_node_add(parser->tree, statement, parse_expression(parser, 1));
        parser->suppress_typed_object_literal = previous_suppression;
        if(parenthesized)
            expect(parser, XS_TOKEN_RIGHT_PAREN, "expected ')' after for-each iterable");
    }
    else
    {
        if(parser->current.kind != XS_TOKEN_SEMICOLON)
        {
            statement->flags |= XS_SYNTAX_FLAG_FOR_INITIALIZER;
            if(parser->current.kind == XS_TOKEN_KW_VAL || parser->current.kind == XS_TOKEN_KW_CONST ||
               parser->current.kind == XS_TOKEN_KW_STATIC || parser->current.kind == XS_TOKEN_KW_ATOMIC ||
               (parser->current.kind == XS_TOKEN_IDENTIFIER &&
                (parser->next.kind == XS_TOKEN_COLON || parser->next.kind == XS_TOKEN_INFER_ASSIGN)))
                xs_syntax_node_add(parser->tree, statement, parse_variable(parser, false));
            else
                xs_syntax_node_add(parser->tree, statement, parse_expression(parser, 1));
        }
        expect(parser, XS_TOKEN_SEMICOLON, "expected ';' after for initializer");
        if(parser->current.kind != XS_TOKEN_SEMICOLON)
        {
            statement->flags |= XS_SYNTAX_FLAG_FOR_CONDITION;
            xs_syntax_node_add(parser->tree, statement, parse_expression(parser, 1));
        }
        expect(parser, XS_TOKEN_SEMICOLON, "expected ';' after for condition");
        if(parser->current.kind != (parenthesized ? XS_TOKEN_RIGHT_PAREN : XS_TOKEN_LEFT_BRACE))
        {
            statement->flags |= XS_SYNTAX_FLAG_FOR_UPDATE;
            bool previous_suppression = parser->suppress_typed_object_literal;
            parser->suppress_typed_object_literal = !parenthesized;
            xs_syntax_node_add(parser->tree, statement, parse_expression(parser, 1));
            parser->suppress_typed_object_literal = previous_suppression;
        }
        if(parenthesized)
            expect(parser, XS_TOKEN_RIGHT_PAREN, "expected ')' after for increment");
    }
    ++parser->loop_depth;
    xs_syntax_node_add(parser->tree, statement, parse_block(parser));
    --parser->loop_depth;
    finish_node(parser, statement, parser->previous.span.end);
    return statement;
}

static XsSyntaxNode *parse_match(SyntaxParser *parser, size_t start)
{
    XsSyntaxNode *statement = node(parser, XS_SYNTAX_STMT_MATCH, (XsSpan){start, parser->previous.span.end});
    bool parenthesized = accept(parser, XS_TOKEN_LEFT_PAREN);
    bool previous_suppression = parser->suppress_typed_object_literal;
    parser->suppress_typed_object_literal = !parenthesized;
    xs_syntax_node_add(parser->tree, statement, parse_expression(parser, 1));
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
        xs_syntax_node_add(parser->tree, arm, parse_statement(parser));
        accept(parser, XS_TOKEN_COMMA);
        finish_node(parser, arm, parser->previous.span.end);
        xs_syntax_node_add(parser->tree, statement, arm);
    }
    expect(parser, XS_TOKEN_RIGHT_BRACE, "expected '}' after match arms");
    finish_node(parser, statement, parser->previous.span.end);
    return statement;
}

XsSyntaxNode *parse_statement(SyntaxParser *parser)
{
    size_t start = parser->current.span.start;
    if(parser->current.kind == XS_TOKEN_LEFT_BRACE)
        return parse_block(parser);
    if(accept(parser, XS_TOKEN_KW_RETURN))
    {
        XsSyntaxNode *statement = node(parser, XS_SYNTAX_STMT_RETURN, (XsSpan){start, parser->previous.span.end});
        if(parser->current.kind != XS_TOKEN_SEMICOLON)
            xs_syntax_node_add(parser->tree, statement, parse_expression(parser, 1));
        expect(parser, XS_TOKEN_SEMICOLON, "expected ';' after return");
        finish_node(parser, statement, parser->previous.span.end);
        return statement;
    }
    if(accept(parser, XS_TOKEN_KW_ELSE))
        return parse_discard(parser, start);
    if(accept(parser, XS_TOKEN_KW_IF))
        return parse_if(parser);
    if(accept(parser, XS_TOKEN_KW_FOR))
        return parse_for(parser, start);
    if(accept(parser, XS_TOKEN_KW_MATCH))
        return parse_match(parser, start);
    if(accept(parser, XS_TOKEN_KW_DO))
    {
        XsSyntaxNode *statement = node(parser, XS_SYNTAX_STMT_WHILE, (XsSpan){start, parser->previous.span.end});
        statement->flags |= XS_SYNTAX_FLAG_POST_TEST_LOOP;
        ++parser->loop_depth;
        XsSyntaxNode *body = parse_block(parser);
        --parser->loop_depth;
        expect(parser, XS_TOKEN_KW_WHILE, "expected 'while' after do block");
        bool parenthesized = accept(parser, XS_TOKEN_LEFT_PAREN);
        XsSyntaxNode *condition = parse_expression(parser, 1);
        if(parenthesized)
            expect(parser, XS_TOKEN_RIGHT_PAREN, "expected ')' after do-while condition");
        expect(parser, XS_TOKEN_SEMICOLON, "expected ';' after do-while statement");
        xs_syntax_node_add(parser->tree, statement, condition);
        xs_syntax_node_add(parser->tree, statement, body);
        finish_node(parser, statement, parser->previous.span.end);
        return statement;
    }
    if(accept(parser, XS_TOKEN_KW_WHILE))
    {
        XsSyntaxNode *statement = node(parser, XS_SYNTAX_STMT_WHILE, (XsSpan){start, parser->previous.span.end});
        bool parenthesized = accept(parser, XS_TOKEN_LEFT_PAREN);
        bool previous_suppression = parser->suppress_typed_object_literal;
        parser->suppress_typed_object_literal = !parenthesized;
        xs_syntax_node_add(parser->tree, statement, parse_expression(parser, 1));
        parser->suppress_typed_object_literal = previous_suppression;
        if(parenthesized)
            expect(parser, XS_TOKEN_RIGHT_PAREN, "expected ')' after while condition");
        ++parser->loop_depth;
        xs_syntax_node_add(parser->tree, statement, parse_block(parser));
        --parser->loop_depth;
        finish_node(parser, statement, parser->previous.span.end);
        return statement;
    }
    if(accept(parser, XS_TOKEN_KW_LOOP))
    {
        XsSyntaxNode *statement = node(parser, XS_SYNTAX_STMT_LOOP, (XsSpan){start, parser->previous.span.end});
        ++parser->loop_depth;
        xs_syntax_node_add(parser->tree, statement, parse_block(parser));
        --parser->loop_depth;
        finish_node(parser, statement, parser->previous.span.end);
        return statement;
    }
    if(accept(parser, XS_TOKEN_KW_BREAK) || accept(parser, XS_TOKEN_KW_CONTINUE))
    {
        bool is_break = parser->previous.kind == XS_TOKEN_KW_BREAK;
        XsSyntaxNode *statement = node(parser, is_break ? XS_SYNTAX_STMT_BREAK : XS_SYNTAX_STMT_CONTINUE,
                                       (XsSpan){start, parser->previous.span.end});
        if(parser->loop_depth == 0)
            xs_diagnostics_add(
                parser->diagnostics, XS_DIAGNOSTIC_ERROR,
                statement->text.length == 0 ? parser->previous.span : (XsSpan){start, parser->previous.span.end},
                is_break ? "break can only be used inside loops" : "continue can only be used inside loops");
        expect(parser, XS_TOKEN_SEMICOLON, "expected ';' after loop control statement");
        finish_node(parser, statement, parser->previous.span.end);
        return statement;
    }
    if(parser->current.kind == XS_TOKEN_KW_VAL || parser->current.kind == XS_TOKEN_KW_CONST ||
       parser->current.kind == XS_TOKEN_KW_STATIC || parser->current.kind == XS_TOKEN_KW_ATOMIC ||
       (parser->current.kind == XS_TOKEN_IDENTIFIER &&
        (parser->next.kind == XS_TOKEN_COLON || parser->next.kind == XS_TOKEN_INFER_ASSIGN)))
    {
        XsSyntaxNode *statement = node(parser, XS_SYNTAX_STMT_VARIABLE, (XsSpan){start, start});
        xs_syntax_node_add(parser->tree, statement, parse_variable(parser, true));
        finish_node(parser, statement, parser->previous.span.end);
        return statement;
    }
    if(parser->current.kind == XS_TOKEN_KW_MACRO_RULES)
        return parse_declaration(parser, false);
    if(starts_pattern_variable(parser))
    {
        XsSyntaxNode *statement = node(parser, XS_SYNTAX_STMT_VARIABLE, (XsSpan){start, start});
        xs_syntax_node_add(parser->tree, statement, parse_pattern_variable(parser));
        finish_node(parser, statement, parser->previous.span.end);
        return statement;
    }

    XsSyntaxNode *statement = node(parser, XS_SYNTAX_STMT_EXPRESSION, (XsSpan){start, start});
    XsSyntaxNode *expression = parse_expression(parser, 1);
    xs_syntax_node_add(parser->tree, statement, expression);
    if(accept(parser, XS_TOKEN_SEMICOLON))
        statement->flags |= XS_SYNTAX_FLAG_DISCARDED;
    else if(parser->current.kind != XS_TOKEN_RIGHT_BRACE && parser->current.kind != XS_TOKEN_EOF)
        expect(parser, XS_TOKEN_SEMICOLON, "expected ';' after non-tail expression statement");
    if(expression != nullptr && expression->kind == XS_SYNTAX_EXPR_MACRO_CALL)
        statement->kind = XS_SYNTAX_STMT_MACRO_CALL;
    finish_node(parser, statement, parser->previous.span.end);
    return statement;
}
